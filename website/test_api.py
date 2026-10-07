import copy, http.client, json, os, tempfile, threading, unittest
from pathlib import Path
from server import make_server, ROOT

class API(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(); self.folder=Path(self.tmp.name)
        self.start(); self.cookie=''; self.csrf=''
    def start(self):
        self.server=make_server(self.folder,ROOT/'data/history.json',port=0)
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True); self.thread.start()
        self.port=self.server.server_port
    def tearDown(self):
        self.server.shutdown();self.server.server_close();self.thread.join();self.tmp.cleanup()
    def request(self,path,body=None,auth=True,origin=True,csrf=True,method=None,host=None):
        h={'Host':host or f'127.0.0.1:{self.port}'}
        if auth: h['Cookie']=self.cookie
        if body is not None:
            h['Content-Type']='application/json'
            if origin: h['Origin']=f'http://127.0.0.1:{self.port}'
            if csrf: h['X-CSRF-Token']=self.csrf
        c=http.client.HTTPConnection('127.0.0.1',self.port,timeout=5)
        c.request(method or ('POST' if body is not None else 'GET'),path,body=json.dumps(body) if body is not None else None,headers=h)
        r=c.getresponse();raw=r.read();status=r.status;headers=dict(r.getheaders());c.close()
        return status,json.loads(raw),headers
    def login(self):
        credentials=json.loads((self.folder/'credentials.json').read_text())
        code,data,h=self.request('/api/login',credentials)
        self.assertEqual(code,200);self.csrf=data['csrf'];self.cookie=h['Set-Cookie'].split(';')[0]
        self.assertIn('HttpOnly',h['Set-Cookie']);self.assertIn('SameSite=Strict',h['Set-Cookie'])
    def payload(self):
        actors=self.server.store.actors[:10]
        return {'date':'2026-10-03','version':0,'venues':[{'id':'zhongjie','city':'沈阳','sessions':[{'id':'evening','label':'晚场','groups':[{'members':[{'id':a['id'],'name':a['name']} for a in actors[i:i+2]]} for i in range(0,10,2)]}]}]}
    def save(self):
        p=self.payload();self.assertEqual(self.request('/api/draft',p)[0],200)
        return {'date':p['date'],'version':1,'revision':0}
    def test_01_unpublished_and_library(self):
        code,data,_=self.request('/api/lineup?date=2026-10-03');self.assertEqual(code,404);self.assertIn('error',data)
        code,data,_=self.request('/api/actors');self.assertEqual(code,200);self.assertGreater(len(data['actors']),40)
        self.assertTrue(all('credits' in a and 'highlights' in a for a in data['actors']))
        config=self.request('/api/config')[1];self.assertEqual([c['city'] for c in config['cities']],['沈阳','哈尔滨','北京','长春','南京','泰安','临沂','大连'])
    def test_02_permissions(self):
        self.assertEqual(self.request('/api/draft?date=2026-10-03')[0],401)
        self.assertEqual(self.request('/api/draft',self.payload())[0],401)
        self.login();self.assertEqual(self.request('/api/draft',self.payload(),csrf=False)[0],403)
        self.assertEqual(self.request('/api/draft',self.payload(),origin=False)[0],403)
        self.assertEqual(self.request('/api/actors',host='attacker.example')[0],403)
        self.assertEqual(self.request('/api/lineup?date=2026-10-03',{})[0],404)
        self.assertEqual(self.request('/api/logout',{})[0],200)
        self.assertEqual(self.request('/api/draft?date=2026-10-03')[0],401)
    def test_03_draft_isolation_publish_and_revision(self):
        self.login();p=self.save()
        self.assertEqual(self.request('/api/lineup?date=2026-10-03')[0],404)
        self.assertEqual(self.request('/api/preview',p)[0],200)
        self.assertEqual(self.request('/api/lineup?date=2026-10-03')[0],404)
        code,pub,_=self.request('/api/publish',p);self.assertEqual(code,200);self.assertEqual(pub['revision'],1)
        self.assertEqual(set(pub),{'schema_version','date','revision','venues'})
        change=self.payload();change['version']=1;change['venues'][0]['sessions'][0]['groups'].pop()
        self.assertEqual(self.request('/api/draft',change)[0],200)
        self.assertEqual(len(self.request('/api/lineup?date=2026-10-03')[1]['venues'][0]['sessions'][0]['groups']),5)
        self.assertEqual(self.request('/api/publish',p)[0],409)
        self.assertEqual(self.request('/api/publish',{'date':p['date'],'version':2,'revision':0})[0],409)
        self.assertEqual(self.request('/api/publish',{'date':p['date'],'version':2,'revision':1})[1]['revision'],2)
    def test_04_validation(self):
        self.login();base=self.payload()
        cases=[]
        p=copy.deepcopy(base);g=p['venues'][0]['sessions'][0]['groups'];g[1]['members'][0]=g[0]['members'][0];cases.append(p)
        p=copy.deepcopy(base);p['venues'][0]['sessions'][0]['groups']=[];cases.append(p)
        p=copy.deepcopy(base);p['venues'][0]['sessions'][0]['groups']*=3;cases.append(p)
        p=copy.deepcopy(base);p['venues'][0]['sessions'][0]['groups'][0]['members'].pop();cases.append(p)
        p=copy.deepcopy(base);p['venues'][0]['sessions'][0]['groups'][0]['members'][0]['name']='';cases.append(p)
        p=copy.deepcopy(base);p['date']='../../bad';cases.append(p)
        p=copy.deepcopy(base);p['date']='2026-02-30';cases.append(p)
        p=copy.deepcopy(base);p['venues'][0]['city']='大连';cases.append(p)
        for p in cases:self.assertEqual(self.request('/api/draft',p)[0],422)
        p=copy.deepcopy(base);p['venues'][0]['sessions'][0]['groups'][0]['members'][0]={'id':'','name':''}
        self.assertEqual(self.request('/api/draft',p)[0],200)
        self.assertEqual(self.request('/api/publish',{'date':p['date'],'version':1,'revision':0})[0],422)
    def test_05_persistence_and_private_files(self):
        self.login();p=self.save();self.request('/api/publish',p)
        self.server.shutdown();self.server.server_close();self.thread.join();self.start()
        self.assertEqual(self.request('/api/lineup?date=2026-10-03')[1]['revision'],1)
        self.assertEqual(self.request('/api/draft?date=2026-10-03')[0],401)
        self.login();self.assertEqual(self.request('/api/draft?date=2026-10-03')[1]['version'],1)
        if os.name!='nt':  # Windows uses inherited ACLs, not POSIX mode bits.
            self.assertEqual((self.folder/'credentials.json').stat().st_mode&0o777,0o600)
            self.assertEqual((self.folder/'state.json').stat().st_mode&0o777,0o600)
        for path in ['/runtime/credentials.json','/data/history.json','/../server.py']:
            self.assertEqual(self.request(path)[0],404)
    def test_06_previous_is_explicit(self):
        self.login();p='/api/previous?date=2026-10-03&city=zhongjie&session=evening'
        old=self.request(p)[1];self.assertEqual(old['source'],'historical_library');self.assertEqual(old['date'],'2026-09-29')
        self.assertEqual(self.request('/api/draft?date=2026-10-03')[1]['venues'],[])
        self.assertEqual(self.request('/api/previous?date=2026-10-03&city=dalian&session=evening')[0],404)
        pub=self.save();self.request('/api/publish',pub)
        self.assertEqual(self.request(p.replace('2026-10-03','2026-10-04'))[1]['source'],'published')
    def test_07_login_throttle_and_bad_json(self):
        for _ in range(5):self.assertEqual(self.request('/api/login',{'username':'admin','password':'wrong'})[0],401)
        self.assertEqual(self.request('/api/login',{'username':'admin','password':'wrong'})[0],429)
        c=http.client.HTTPConnection('127.0.0.1',self.port)
        c.request('POST','/api/login',body='{',headers={'Content-Type':'application/json','Origin':f'http://127.0.0.1:{self.port}'})
        r=c.getresponse();self.assertEqual(r.status,400);r.read();c.close()
    def test_08_atomic_failure_keeps_old_state(self):
        from unittest.mock import patch
        self.login();self.save();before=(self.folder/'state.json').read_bytes()
        p=self.payload();p['version']=1
        with patch('server.os.replace',side_effect=OSError('test disk error')):
            self.assertEqual(self.request('/api/draft',p)[0],500)
        self.assertEqual((self.folder/'state.json').read_bytes(),before)
        self.assertEqual(self.request('/api/draft?date=2026-10-03')[1]['version'],1)
        self.assertEqual(list(self.folder.glob('.write-*')),[])
    def test_09_concurrent_optimistic_lock(self):
        self.login();codes=[]
        def save(): codes.append(self.request('/api/draft',self.payload())[0])
        threads=[threading.Thread(target=save) for _ in range(2)]
        for t in threads:t.start()
        for t in threads:t.join()
        self.assertEqual(sorted(codes),[200,409])

    def actor_data(self, actor=None):
        return {'name':actor['name'] if actor else '测试新演员','credits':['作品甲 · 角色乙'],'user_highlights':[{'label':'舞台特色','text':'用户自填测试特色'}],'bio':'普通介绍','version':actor['version'] if actor else 0}
    def test_10_actor_create_select_restart(self):
        import uuid
        self.login();data=self.actor_data();code,a,_=self.request('/api/actors',data)
        self.assertEqual(code,201);uuid.UUID(a['id']);self.assertEqual(a['version'],1)
        self.assertEqual(a['highlights'],[]);self.assertTrue(a['user_edited']);self.assertIsNone(a['historical_profile'])
        p=self.payload();p['venues'][0]['sessions'][0]['groups'][0]['members'][0]={'id':a['id'],'name':a['name']}
        self.assertEqual(self.request('/api/draft',p)[0],200)
        self.assertEqual(self.request('/api/preview',{'date':p['date'],'version':1,'revision':0})[0],200)
        credentials=(self.folder/'credentials.json').read_bytes()
        self.server.shutdown();self.server.server_close();self.thread.join();self.start()
        self.assertEqual(next(x for x in self.request('/api/actors')[1]['actors'] if x['id']==a['id']),a)
        self.assertEqual((self.folder/'credentials.json').read_bytes(),credentials)
        self.login();self.assertEqual(self.request('/api/draft?date='+p['date'])[1]['venues'],p['venues'])
    def test_11_rename_snapshot_and_stale_draft(self):
        self.login();p=self.save();published=self.request('/api/publish',p)[1]
        a=self.server.store.actors[0];original=copy.deepcopy(a);body=self.actor_data(a);body['name']='改名测试甲'
        code,updated,_=self.request('/api/actors/'+a['id'],body);self.assertEqual(code,200)
        self.assertEqual(updated['id'],original['id']);self.assertEqual(updated['version'],2)
        self.assertEqual(updated['historical_profile'],original['historical_profile']);self.assertEqual(updated['highlights'],original['highlights']);self.assertEqual(updated['source_notes'],original['source_notes'])
        self.assertEqual(self.request('/api/lineup?date='+p['date'])[1],published)
        self.assertEqual(self.request('/api/preview',{**p,'revision':1})[0],409)
        self.assertEqual(self.request('/api/publish',{**p,'revision':1})[0],409)
        d=self.request('/api/draft?date='+p['date'])[1];self.assertEqual(d['version'],2)
        self.assertEqual(d['venues'][0]['sessions'][0]['groups'][0]['members'][0]['name'],'改名测试甲')
        stale=self.payload();stale['version']=2;stale['venues'][0]['sessions'][0]['groups'][0]['members'][0]={'id':original['id'],'name':original['name']}
        self.assertEqual(self.request('/api/draft',stale)[0],200)
        pre=self.request('/api/preview',{**p,'version':3,'revision':1})[1]
        self.assertEqual(pre['venues'][0]['sessions'][0]['groups'][0]['members'][0]['name'],'改名测试甲')
        old=self.request('/api/previous?date=2026-10-04&city=zhongjie&session=evening')[1]
        self.assertEqual(old['groups'][0]['members'][0]['name'],'改名测试甲')
        self.assertEqual(self.request('/api/lineup?date='+p['date'])[1],published)
        self.server.shutdown();self.server.server_close();self.thread.join();self.start()
        self.assertEqual(self.server.store.lookup[original['id']],updated)
    def test_12_actor_permissions_validation(self):
        body=self.actor_data();a=self.server.store.actors[0]
        for path,data in [('/api/actors',body),('/api/actors/'+a['id'],self.actor_data(a))]:
            self.assertEqual(self.request(path,data,auth=False)[0],401)
        self.login()
        for path,data in [('/api/actors',body),('/api/actors/'+a['id'],self.actor_data(a))]:
            self.assertEqual(self.request(path,data,csrf=False)[0],403)
            self.assertEqual(self.request(path,data,origin=False)[0],403)
        cases=[('name',''),('name','  '),('name','甲'*61),('name',[]),('name','甲\n乙'),('credits','bad'),('credits',[1]),('credits',['x'*301]),('credits',['x']*31),('user_highlights','bad'),('user_highlights',[{}]),('user_highlights',[{'label':'x','text':'y','evidence':[]}]),('user_highlights',[{'label':'x'*61,'text':'y'}]),('user_highlights',[{'label':'x','text':'y'*1001}]),('user_highlights',[{'label':'x','text':'y'}]*21),('bio',{}),('bio','x'*4001)]
        for field,value in cases:
            with self.subTest(field=field):self.assertEqual(self.request('/api/actors',{**body,field:value})[0],422)
        self.assertEqual(self.request('/api/actors',{**body,'id':'injected'})[0],422)
        self.assertEqual(self.request('/api/actors',{**body,'version':True})[0],409)
        self.assertEqual(self.request('/api/actors',{**body,'name':a['name']})[0],409)
        self.assertEqual(self.request('/api/actors/missing',{**body,'version':1})[0],404)
        self.assertEqual(self.request('/api/actors',body)[0],201)
        self.assertEqual(self.request('/api/actors',body)[0],409)
    def test_13_actor_conflict_atomic_failure(self):
        from unittest.mock import patch
        self.login();a=self.request('/api/actors',self.actor_data())[1];data=self.actor_data(a)
        codes=[]
        def change():codes.append(self.request('/api/actors/'+a['id'],data)[0])
        threads=[threading.Thread(target=change) for _ in range(2)]
        for t in threads:t.start()
        for t in threads:t.join()
        self.assertEqual(sorted(codes),[200,409]);current=copy.deepcopy(self.server.store.lookup[a['id']])
        before=(self.folder/'state.json').read_bytes();all_before=self.request('/api/actors')[1]
        with patch('server.os.replace',side_effect=OSError('simulated failure')):
            self.assertEqual(self.request('/api/actors/'+a['id'],{**self.actor_data(current),'name':'不应写入'})[0],500)
            self.assertEqual(self.request('/api/actors',{**self.actor_data(),'name':'不应新增'})[0],500)
        self.assertEqual((self.folder/'state.json').read_bytes(),before);self.assertEqual(self.request('/api/actors')[1],all_before)
        self.assertEqual(list(self.folder.glob('.write-*')),[])

    def test_14_profile_snapshot_and_version(self):
        self.login();p=self.save();self.assertEqual(self.request('/api/publish',p)[0],200)
        members=self.payload()['venues'][0]['sessions'][0]['groups'][0]['members']
        path='/api/device/profile?date=2026-10-03&revision=1&m0='+members[0]['id']+'&m1='+members[1]['id']
        code,profile,_=self.request(path);self.assertEqual(code,200);self.assertTrue(profile['pages'])
        actor=self.server.store.lookup[members[0]['id']]
        data=self.actor_data(actor);data['bio']='已经修改的介绍'
        self.assertEqual(self.request('/api/actors/'+actor['id'],data)[0],200)
        self.assertEqual(self.request(path)[1],profile)
        self.assertEqual(self.request(path.replace('revision=1','revision=2'))[0],409)
        self.assertEqual(self.request(path.replace(members[1]['id'],'missing'))[0],404)
        for page in profile['pages']:
            self.assertLessEqual(len(page['text'].split('\n')),7)
            self.assertTrue(all(len(line)<=11 for line in page['text'].split('\n')))

    def test_15_incompatible_publish_keeps_old(self):
        self.login();p=self.save();self.assertEqual(self.request('/api/publish',p)[0],200)
        old=self.request('/api/lineup?date=2026-10-03')[1]
        actor=self.server.store.actors[0];data=self.actor_data(actor);data['name']='新演员😀'
        self.assertEqual(self.request('/api/actors/'+actor['id'],data)[0],200)
        draft=self.request('/api/draft?date=2026-10-03')[1]
        publish={'date':draft['date'],'version':draft['version'],'revision':1}
        self.assertEqual(self.request('/api/preview',publish)[0],422)
        self.assertEqual(self.request('/api/publish',publish)[0],422)
        self.assertEqual(self.request('/api/lineup?date=2026-10-03')[1],old)
        actor=self.server.store.lookup[actor['id']];data=self.actor_data(actor);data['name']='一二三四五六七八九'
        self.assertEqual(self.request('/api/actors/'+actor['id'],data)[0],200)
        publish['version']+=1
        self.assertEqual(self.request('/api/publish',publish)[0],422)

    def test_16_public_origin(self):
        self.server.public_origin='https://cast.example.com';self.server.allowed_hosts.add('cast.example.com')
        self.assertEqual(self.request('/api/config',host='cast.example.com')[0],200)
        self.assertEqual(self.request('/api/config',host='evil.example.com')[0],403)
        self.assertEqual(self.request('/api/login',{},host='cast.example.com')[0],403)
        c=http.client.HTTPConnection('127.0.0.1',self.port,timeout=5)
        credentials=json.loads((self.folder/'credentials.json').read_text())
        c.request('POST','/api/login',json.dumps(credentials),{'Host':'cast.example.com','Origin':'https://cast.example.com','Content-Type':'application/json'})
        r=c.getresponse();self.assertEqual(r.status,200);r.read();c.close()

    def test_17_device_payload_budget(self):
        import device_contract
        self.login();p=self.save();self.request('/api/publish',p)
        lineup=self.request('/api/lineup?date=2026-10-03')[1]
        huge=copy.deepcopy(lineup)
        huge['venues']*=20
        with self.assertRaises(ValueError):device_contract.exports(huge,self.server.store.lookup)
        actor=copy.deepcopy(self.server.store.actors[0]);actor['bio']='新'*4000
        lookup={**self.server.store.lookup,actor['id']:actor}
        with self.assertRaises(ValueError):device_contract.exports(lineup,lookup)
        self.assertEqual(self.request('/api/lineup?date=2100-01-01')[0],422)

if __name__=='__main__':unittest.main(verbosity=2)
