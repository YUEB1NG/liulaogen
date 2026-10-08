import copy,datetime as dt,hashlib,json,unittest
import test_api
from unittest.mock import patch
import device_contract

class Features(unittest.TestCase):
    setUp=test_api.API.setUp;start=test_api.API.start;tearDown=test_api.API.tearDown;request=test_api.API.request
    login=test_api.API.login;payload=test_api.API.payload;save=test_api.API.save;actor_data=test_api.API.actor_data

    def test_city_time_history_and_stale_edit(self):
        self.assertEqual(self.request('/api/config',{})[0],401)
        self.login();pub=self.save();self.assertEqual(self.request('/api/publish',pub)[0],200)
        settings=self.request('/api/config')[1];settings.pop('sessions')
        settings['cities'].append({'id':'tianjin','city':'天津','times':{'afternoon':'14:30','evening':'19:00'}})
        self.assertEqual(self.request('/api/config',settings)[0],200)
        self.assertEqual(self.request('/api/config',settings)[0],409)
        p=self.payload();p['date']='2026-10-04';p['venues'][0].update(id='tianjin',city='天津')
        self.assertEqual(self.request('/api/draft',p)[0],200)
        code,result,_=self.request('/api/publish',{'date':p['date'],'version':1,'revision':0})
        self.assertEqual(code,200);self.assertEqual(result['venues'][0]['sessions'][0]['time'],'19:00')
        settings=self.request('/api/config')[1];settings.pop('sessions');settings['cities']=settings['cities'][:-1]
        self.assertEqual(self.request('/api/config',settings)[0],200)
        self.assertEqual(self.request('/api/draft?date='+p['date'])[1]['venues'],[])
        self.assertEqual(self.request('/api/lineup?date='+p['date'])[1],result)
        settings=self.request('/api/config')[1];settings.pop('sessions');settings['cities'][0]['times']['evening']='24:00'
        self.assertEqual(self.request('/api/config',settings)[0],422)

    def test_previous_calendar_day(self):
        self.login();pub=self.save();self.request('/api/publish',pub)
        url='/api/changes?city=zhongjie&session=evening&date='
        self.assertTrue(self.request(url+'2026-10-04')[1]['available'])
        self.assertFalse(self.request(url+'2026-10-05')[1]['available'])
        self.assertEqual(len(self.request(url+'2026-10-04')[1]['previous']),10)

    def test_import_sources_preserve_edits_and_glyphs(self):
        self.login();st=self.server.store
        sourced=[a for a in st.actors if a.get('catalog_source')]
        self.assertEqual(len(sourced),179)
        self.assertTrue(any('待核' in json.dumps(a,ensure_ascii=False) for a in sourced))
        actor=next(a for a in st.actors if a['name']=='王小利')
        self.assertIn('王文利',actor['aliases'])
        data=self.actor_data(actor);data['bio']='管理员保留的介绍'
        self.assertEqual(self.request('/api/actors/'+actor['id'],data)[0],200)
        import catalog
        state=copy.deepcopy(st.state);state['imports']=[];catalog.migrate(state,[])
        self.assertEqual(state['actors'][actor['id']]['bio'],data['bio'])
        self.assertEqual(len(state['actors']),len(st.actors))
        # Every imported name/credit/bio passes the real device glyph and page constraints.
        for a in sourced:
            lines=device_contract.pages('个人简介',a['name']+'\n'+a['bio']+'\n'+'\n'.join(a['credits']),13)
            self.assertTrue(all(len(row)<=13 for p in lines for row in p['text'].split('\n')))

    def test_archive_is_frozen_deduplicated_and_full_window(self):
        self.login();pub=self.save();self.request('/api/publish',pub)
        st=self.server.store;key='2026-10-03:1';archive_hash=st.state['archive_versions'][key]
        code,archive,_=self.request('/api/device/blob?id='+archive_hash);self.assertEqual(code,200)
        self.assertEqual(hashlib.sha256(device_contract.encode(archive)).hexdigest(),archive_hash)
        self.assertEqual(len(archive['profiles']),5)
        data=self.payload();data['date']='2026-10-04';self.request('/api/draft',data)
        self.request('/api/publish',{'date':data['date'],'version':1,'revision':0})
        second=st.state['device_blobs'][st.state['archive_versions']['2026-10-04:1']]
        self.assertEqual(archive['profiles'],second['profiles'])
        when=dt.datetime(2026,10,8,tzinfo=dt.timezone(dt.timedelta(hours=8)))
        class Clock(dt.datetime):
            @classmethod
            def now(cls,tz=None):return when if tz else when.replace(tzinfo=None)
        with patch('server.dt.datetime',Clock):
            code,window,_=self.request('/api/device/window?date=2026-10-03&revision=1')
        self.assertEqual(code,200);self.assertEqual(len(window['days']),15)
        self.assertEqual(window['days'][0]['date'],'2026-10-01');self.assertEqual(window['days'][-1]['date'],'2026-10-15')
        self.assertEqual(window['days'][2]['archive'],archive_hash)
        for blob in st.state['device_blobs'].values():self.assertLessEqual(len(device_contract.encode(blob)),8192)

    def test_device_batch_remarks_and_exact_all_delete(self):
        self.login();st=self.server.store;link=st.device_link;ids=['a'*32,'b'*32]
        state=copy.deepcopy(st.state);state['linked_devices']={d:{'name':'Passport','token_hash':'x','protocol':2} for d in ids};st.commit(state)
        import time
        link.seen[ids[0]]=time.monotonic()
        self.assertEqual(self.request('/api/devices/rename',{'device':ids[0],'name':'随身一号'})[0],200)
        self.assertEqual(self.request('/api/devices')[1]['devices'][0]['name'],'随身一号')
        pub=self.save();self.request('/api/publish',pub)
        code,data,_=self.request('/api/devices/batch',{'action':'upload','devices':ids,'date':pub['date'],'revision':1})
        self.assertEqual(code,200);self.assertEqual([r['ok'] for r in data['results']],[True,False])
        self.assertEqual(self.request('/api/devices/batch',{'action':'forget','devices':ids+['c'*32]})[0],409)
        self.assertEqual(len(link.records()),2)
        self.assertEqual(self.request('/api/devices/batch',{'action':'forget','devices':ids})[0],200)
        self.assertEqual(link.records(),{})

    def test_storage_backup_and_hosting_dates(self):
        self.assertEqual(self.request('/api/storage')[0],401);self.assertEqual(self.request('/api/backup')[0],401)
        self.login();self.save()
        (self.folder/'hosting.json').write_text(json.dumps({'provider':'PythonAnywhere','expires_at':'2026-11-07T00:00:00Z','period_start':'2026-10-07T00:00:00Z','secret':'not-for-response'}),'utf8')
        data=self.request('/api/storage')[1]
        self.assertGreater(data['used_bytes'],0);self.assertEqual(data['draft_dates'],1)
        self.assertNotIn('secret',data['hosting']);self.assertEqual(data['hosting']['expires_at'],'2026-11-07T00:00:00Z')
        backup=self.request('/api/backup')[1]['state'];self.assertIn('actors',backup);self.assertIn('drafts',backup)
        self.assertNotIn('linked_devices',backup);self.assertNotIn('credentials',backup)

    def test_legacy_device_requires_firmware_upgrade(self):
        import test_device_link
        self.login();client=test_device_link.DeviceAPI
        status,_=client.device_request(self,'/api/device/heartbeat',
            {'id':client.device,'code':'ABCDEF12','ack':None},client.bearer)
        self.assertEqual(status,200)
        self.assertEqual(self.request('/api/devices/claim',{'code':'ABCDEF12'})[0],200)
        pub=self.save();self.request('/api/publish',pub)
        code,body,_=self.request('/api/devices/upload',{'device':client.device,'date':pub['date'],'revision':1})
        self.assertEqual(code,409);self.assertIn('r5',body['error']['message'])

if __name__=='__main__':unittest.main()
