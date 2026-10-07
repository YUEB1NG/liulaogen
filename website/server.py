#!/usr/bin/env python3
"""AI Passport cast editor: stdlib only, loopback by default."""
import argparse, copy, datetime as dt, hashlib, json, os, re, secrets
import tempfile, threading, time, uuid, unicodedata
from http.cookies import SimpleCookie
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit, parse_qs
import device_contract
from device_link import DeviceLink,DeviceProblem

ROOT = Path(__file__).resolve().parent
CITIES = list(zip(['zhongjie','haerbin','beijing','changchun','nanjing','taian','linyi','dalian'],
                  ['沈阳','哈尔滨','北京','长春','南京','泰安','临沂','大连']))
CITY = dict(CITIES)
SESSIONS = {'afternoon':'午场', 'evening':'晚场'}

class Problem(Exception):
    def __init__(self, status, message): self.status, self.message = status, message

def require(ok, message, status=422):
    if not ok: raise Problem(status, message)

def date_value(value):
    require(isinstance(value,str) and re.fullmatch(r'\d{4}-\d{2}-\d{2}',value), '日期格式应为 YYYY-MM-DD')
    try:
        day=dt.date.fromisoformat(value)
        require(2020<=day.year<=2099,'设备支持 2020 至 2099 年')
    except ValueError: raise Problem(422,'无效日期')
    return value

def atomic(path, obj):
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd, name = tempfile.mkstemp(dir=path.parent, prefix='.write-')
    try:
        with os.fdopen(fd,'w',encoding='utf8') as f:
            json.dump(obj,f,ensure_ascii=False,separators=(',',':')); f.flush(); os.fsync(f.fileno())
        os.replace(name,path)
        # Windows cannot open a directory with os.open. The file is flushed
        # before replacement on both platforms; POSIX also flushes its directory.
        if os.name != 'nt':
            fd = os.open(path.parent,os.O_RDONLY)
            try: os.fsync(fd)
            finally: os.close(fd)
    finally:
        if os.path.exists(name): os.unlink(name)

def actor_id(name): return 'a_' + hashlib.sha256(name.encode()).hexdigest()[:16]

def import_history(path):
    raw=json.loads(path.read_text('utf8')); actors={}; history={}
    for v in raw['venues']:
        vid='taian' if v['id']=='taian-afternoon' else v['id']
        sid='afternoon' if v['id']=='taian-afternoon' else 'evening'
        groups=[]
        for card in v.get('cards',[]):
            names=[x.strip() for x in card['names'].split('·') if x.strip()]
            members=[]
            for name in names:
                aid=actor_id(name)
                a=actors.setdefault(aid,{'id':aid,'name':name,'credits':[], 'highlights':[], 'source_notes':[]})
                for line in card.get('credits','').splitlines():
                    if line.startswith(name+'｜') and line not in a['credits']: a['credits'].append(line)
                # Card paragraphs may concern both partners. Keep explicit attribution context.
                for h in card.get('highlights',[]):
                    if name in h.get('text',''):
                        item={**h,'context_names':names,'source_date':raw['date']}
                        if item not in a['highlights']: a['highlights'].append(item)
                note=card.get('notes','')
                if note and note not in a['source_notes']: a['source_notes'].append(note)
                members.append({'id':aid,'name':name})
            if len(members)==2: groups.append({'members':members})
        if groups: history[(vid,sid)]={'date':raw['date'],'groups':groups}
    return list(actors.values()), history

class Store:
    def __init__(self, folder, source):
        self.folder=Path(folder); self.folder.mkdir(parents=True,exist_ok=True,mode=0o700)
        self.path=self.folder/'state.json'; self.lock=threading.RLock()
        self.actors,self.history=import_history(Path(source)); self.lookup={a['id']:a for a in self.actors}
        self.state=json.loads(self.path.read_text('utf8')) if self.path.exists() else {'drafts':{},'published':{},'audit':[]}
        if 'actors' not in self.state:
            self.state['actors']={a['id']:{**a,'version':1,'bio':'','user_highlights':[],
                'user_edited':False,'origin':'historical_library','aliases':[],
                'historical_profile':copy.deepcopy(a)} for a in self.actors}
        self.refresh_actors()
        cred=self.folder/'credentials.json'
        if not cred.exists():
            password=os.environ.get('CAST_ADMIN_PASSWORD') or secrets.token_urlsafe(24)
            if len(password)<16:raise ValueError('Initial administrator password must have at least 16 characters')
            atomic(cred,{'username':'admin','password':password})
        os.chmod(cred,0o600); self.credentials=json.loads(cred.read_text('utf8'))
        self.sessions={}; self.attempts={}
        self.device_link=DeviceLink(self)
    def refresh_actors(self):
        self.actors=list(self.state['actors'].values()); self.lookup={a['id']:a for a in self.actors}
    def canonical(self, value):
        value=copy.deepcopy(value)
        def walk(x):
            if isinstance(x,dict):
                if x.get('id') in self.lookup and 'name' in x: x['name']=self.lookup[x['id']]['name']
                for v in x.values(): walk(v)
            elif isinstance(x,list):
                for v in x: walk(v)
        walk(value); return value
    def draft(self,date):
        return self.canonical(self.state['drafts'].get(date,{'date':date,'version':0,'venues':[]}))
    def save_actor(self, data, aid=None):
        allowed={'name','credits','user_highlights','bio','version'}
        require(set(data)==allowed,'资料字段不完整或含不允许修改的字段（ID不可编辑）')
        def text(v,limit,required=False):
            require(isinstance(v,str),'资料字段必须是文本')
            require(len(v)<=limit,f'文本不能超过{limit}字'); v=v.strip(); require(bool(v) or not required,'必填文本不能为空')
            require(not any(ord(c)<32 and c not in '\n\t' for c in v),'文本含无效控制字符')
            return v
        name=text(data['name'],60,True)
        require('\n' not in name and '\t' not in name,'姓名不可含换行或制表符')
        bio=text(data['bio'],4000)
        credits=data['credits']; highlights=data['user_highlights']
        require(isinstance(credits,list) and len(credits)<=30,'作品角色须为数组，最多30条')
        credits=[text(v,300,True) for v in credits]
        require(isinstance(highlights,list) and len(highlights)<=20,'舞台特色须为数组，最多20条')
        clean=[]
        for h in highlights:
            require(isinstance(h,dict) and set(h)=={'label','text'},'舞台特色仅接受label与text，不接受伪造证据')
            clean.append({'label':text(h['label'],60,True),'text':text(h['text'],1000,True)})
        old=self.lookup.get(aid) if aid else None
        require(aid is None or old is not None,'演员不存在',404)
        require(type(data['version']) is int and data['version']==(old['version'] if old else 0),'演员资料已被其他窗口修改，请重新加载后核对',409)
        key=lambda n:unicodedata.normalize('NFKC',n).casefold()
        require(not any(a['id']!=aid and key(name) in [key(n) for n in [a['name']]+a['aliases']] for a in self.actors),'同名演员或历史姓名已存在，请使用可区分的姓名；不会自动合并',409)
        actor=copy.deepcopy(old) if old else {'id':str(uuid.uuid4()),'version':0,'highlights':[], 'source_notes':[], 'aliases':[], 'origin':'user_entry','historical_profile':None}
        if old and old['name']!=name and old['name'] not in actor['aliases']: actor['aliases'].append(old['name'])
        actor.update(name=name,credits=credits,bio=bio,user_highlights=clean,user_edited=True,version=actor['version']+1,updated_at=dt.datetime.now(dt.timezone.utc).isoformat())
        state=copy.deepcopy(self.state); state['actors'][actor['id']]=actor
        # Invalidate previews/editors referencing this actor; published snapshots remain untouched.
        for d in state['drafts'].values():
            if any(m.get('id')==actor['id'] for v in d['venues'] for s in v['sessions'] for g in s['groups'] for m in g['members']): d['version']+=1
        state['audit'].append({'action':'actor_update' if old else 'actor_create','id':actor['id'],'version':actor['version'],'at':actor['updated_at'],'actor':'admin'})
        self.commit(state); self.refresh_actors(); return actor
    def revision(self,date): return self.state['published'].get(date,{}).get('revision',0)
    def commit(self,state): atomic(self.path,state); self.state=state
    def validate(self,venues,complete):
        require(isinstance(venues,list) and len(venues)<=8,'城市列表无效')
        require(not complete or bool(venues),'至少安排一个城市后才可发布')
        seen_v=set(); out=[]
        for v in venues:
            require(isinstance(v,dict),'城市对象无效'); vid=v.get('id')
            require(isinstance(vid,str) and vid in CITY and vid not in seen_v,'城市无效或重复'); seen_v.add(vid)
            require(v.get('city')==CITY[vid],'城市名称与ID不一致')
            ss=v.get('sessions'); require(isinstance(ss,list) and 1<=len(ss)<=2,'每城须有1至2场')
            seen_s=set(); clean=[]
            for s in ss:
                require(isinstance(s,dict),'场次对象无效'); sid=s.get('id')
                require(isinstance(sid,str) and sid in SESSIONS and sid not in seen_s,'场次无效或重复'); seen_s.add(sid)
                require(s.get('label')==SESSIONS[sid],'场次名称与ID不一致')
                groups=s.get('groups'); require(isinstance(groups,list) and 1<=len(groups)<=12,'每场须有1至12组，默认5组')
                used=set(); gs=[]
                for g in groups:
                    require(isinstance(g,dict),'组对象无效'); mm=g.get('members')
                    require(isinstance(mm,list) and len(mm)==2,'每组必须恰好2位演员'); members=[]
                    for m in mm:
                        require(isinstance(m,dict),'演员对象无效')
                        if not m or m=={'id':'','name':''}:
                            require(not complete,'存在缺名演员，请补全所有组'); members.append({'id':'','name':''}); continue
                        aid=m.get('id'); require(isinstance(aid,str) and aid in self.lookup,'请从演员库选择有效演员')
                        require(isinstance(m.get('name'),str) and m['name'] in [self.lookup[aid]['name']]+self.lookup[aid]['aliases'],'演员姓名与ID不一致或缺名')
                        require(aid not in used,'同一场次演员不可重复'); used.add(aid)
                        members.append({'id':aid,'name':self.lookup[aid]['name']})
                    gs.append({'members':members})
                clean.append({'id':sid,'label':SESSIONS[sid],'groups':gs})
            out.append({'id':vid,'city':CITY[vid],'sessions':sorted(clean,key=lambda s:list(SESSIONS).index(s['id']))})
        return sorted(out,key=lambda v:list(CITY).index(v['id']))

class Handler(BaseHTTPRequestHandler):
    server_version='CastLocal/1'
    def setup(self):
        super().setup()
        self.connection.settimeout(8)
    def log_message(self,*args): pass  # Never log credentials, bodies, cookies or tokens.
    @property
    def store(self): return self.server.store
    def reply(self,status,obj,cookie=None):
        body=json.dumps(obj,ensure_ascii=False,separators=(',',':')).encode(); self.send_response(status)
        self.headers_common('application/json; charset=utf-8')
        if cookie: self.send_header('Set-Cookie',cookie)
        self.send_header('Content-Length',str(len(body))); self.end_headers(); self.wfile.write(body)
    def headers_common(self,ctype):
        self.send_header('Content-Type',ctype); self.send_header('Cache-Control','no-store')
        self.send_header('X-Content-Type-Options','nosniff'); self.send_header('X-Frame-Options','DENY')
        self.send_header('Referrer-Policy','no-referrer')
        self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'")
    def guard_host(self):
        host=self.headers.get('Host','')
        require(host in self.server.allowed_hosts,'Host 不受信任',403)
        return host
    def session(self):
        cookie=SimpleCookie()
        try: cookie.load(self.headers.get('Cookie',''))
        except Exception: raise Problem(401,'请先登录')
        token=cookie.get('cast_session'); token=token.value if token else ''
        session=self.store.sessions.get(token)
        require(session is not None and session['expires']>time.time(),'请先登录',401)
        return token, session
    def do_GET(self): self.dispatch(False)
    def do_POST(self): self.dispatch(True)
    def dispatch(self,post):
        try:
            # Read a bounded body before rejecting Origin/Host. Closing with an
            # unread body resets TCP on Windows, hiding the actual error. Never
            # hold the state lock while waiting for a slow client.
            data=None
            if post:
                require(not self.headers.get('Transfer-Encoding'),'不支持分块请求',400)
                lengths=self.headers.get_all('Content-Length',[])
                require(len(lengths)==1,'无效请求长度',400)
                try: size=int(lengths[0])
                except ValueError: raise Problem(400,'无效请求长度')
                require(0<size<=262144,'请求体为空或过大',413)
                raw=self.rfile.read(size)
                require(len(raw)==size,'请求体不完整',400)
            host=self.guard_host(); u=urlsplit(self.path); q=parse_qs(u.query)
            if post:
                expected=self.server.public_origin or ('https' if self.server.secure_cookie else 'http')+'://'+host
                if u.path!='/api/device/heartbeat':require(self.headers.get('Origin')==expected,'跨站请求已拒绝',403)
                else:require(len(raw)<=2048,'设备请求过大',413)
                require(self.headers.get('Content-Type','').split(';')[0]=='application/json','仅接受 application/json',415)
                try: data=json.loads(raw)
                except (ValueError,UnicodeError): raise Problem(400,'JSON 无效')
                require(isinstance(data,dict),'请求体应为对象',400)
            with self.store.lock:
                if post:
                    self.post(u.path,data)
                else: self.get(u.path,q)
        except (Problem,DeviceProblem) as e: self.reply(e.status,{'error':{'code':e.status,'message':e.message}})
        except (OSError,KeyError,TypeError,ValueError): self.reply(500,{'error':{'code':500,'message':'本地存储或请求处理失败，未确认成功'}})
    def get(self,path,q):
        st=self.store
        if path=='/healthz':return self.reply(200,{'ok':True})
        if path=='/api/devices':self.session();return self.reply(200,st.device_link.listing())
        if path=='/api/device/content':
            require(set(q)=={'id','job'} and all(len(v)==1 for v in q.values()),'设备参数无效')
            require(len(self.headers.get_all('Authorization',[]))==1,'设备凭据无效',401)
            return self.reply(200,st.device_link.content(q['id'][0],q['job'][0],self.headers.get('Authorization')))
        if path=='/api/actors': return self.reply(200,{'schema_version':1,'actors':st.actors,'notice':'历史资料与用户自填资料分开保留，不代表今日阵容或节目承诺'})
        if path=='/api/config': return self.reply(200,{'cities':[{'id':i,'city':c} for i,c in CITIES],'sessions':[{'id':i,'label':s} for i,s in SESSIONS.items()]})
        if path=='/api/lineup':
            date=date_value(q.get('date',[''])[0]); lineup=st.state['published'].get(date)
            require(lineup is not None,'该日期尚未发布',404); return self.reply(200,lineup)
        if path=='/api/device/profile':
            date=date_value(q.get('date',[''])[0])
            revision=q.get('revision',[''])[0]
            require(revision.isdecimal(),'版本无效')
            record=st.state.get('device_profiles',{}).get(date)
            require(record is not None,'该版本未发布设备资料',404)
            require(record['revision']==int(revision),'版本已更新，请先更新阵容',409)
            key=q.get('m0',[''])[0]+'|'+q.get('m1',[''])[0]
            profile=record['profiles'].get(key)
            require(profile is not None,'该组合不在已发布名单中',404)
            return self.reply(200,profile)
        if path=='/api/session':
            _,s=self.session(); return self.reply(200,{'username':'admin','csrf':s['csrf']})
        if path in ['/api/draft','/api/previous']:
            self.session(); date=date_value(q.get('date',[''])[0])
            if path=='/api/draft': return self.reply(200,{**st.draft(date),'revision':st.revision(date)})
            vid=q.get('city',[''])[0]; sid=q.get('session',[''])[0]
            require(vid in CITY and sid in SESSIONS,'城市或场次无效')
            for day in sorted(st.state['published'],reverse=True):
                if day>=date: continue
                for v in st.state['published'][day]['venues']:
                    if v['id']==vid:
                        for s in v['sessions']:
                            if s['id']==sid: return self.reply(200,{'source':'published','date':day,'groups':st.canonical(s['groups'])})
            old=st.history.get((vid,sid))
            require(old is not None and old['date']<date,'没有可复制的此前阵容',404)
            return self.reply(200,{'source':'historical_library',**st.canonical(old)})
        files={'/':('index.html','text/html; charset=utf-8'),'/app.js':('app.js','text/javascript; charset=utf-8'),'/style.css':('style.css','text/css; charset=utf-8')}
        require(path in files,'接口或文件不存在',404)
        name,ctype=files[path]; body=(ROOT/'static'/name).read_bytes()
        self.send_response(200); self.headers_common(ctype); self.send_header('Content-Length',str(len(body))); self.end_headers(); self.wfile.write(body)
    def post(self,path,data):
        st=self.store
        if path=='/api/device/heartbeat':
            require(len(self.headers.get_all('Authorization',[]))==1,'设备凭据无效',401)
            return self.reply(200,st.device_link.heartbeat(data,self.headers.get('Authorization')))
        if path=='/api/login':
            ip=self.client_address[0]; now=time.time(); tries=[t for t in st.attempts.get(ip,[]) if now-t<60]; st.attempts[ip]=tries
            require(len(tries)<5,'尝试过多，请60秒后重试',429); tries.append(now)
            name=data.get('username'); password=data.get('password')
            require(isinstance(name,str) and isinstance(password,str),'凭据无效',401)
            valid=secrets.compare_digest(password.encode(),st.credentials['password'].encode())
            require(valid and name==st.credentials['username'],'用户名或密码错误',401)
            st.sessions={k:v for k,v in st.sessions.items() if v['expires']>now}
            require(len(st.sessions)<100,'会话数已达上限',429)
            token=secrets.token_urlsafe(32); csrf=secrets.token_urlsafe(32); st.sessions[token]={'csrf':csrf,'expires':now+28800}
            return self.reply(200,{'csrf':csrf},'cast_session='+token+'; Path=/; HttpOnly; SameSite=Strict; Max-Age=28800'+('; Secure' if self.server.secure_cookie else ''))
        token,s=self.session()
        require(secrets.compare_digest(self.headers.get('X-CSRF-Token','').encode(),s['csrf'].encode()),'CSRF 校验失败',403)
        if path=='/api/devices/claim':return self.reply(200,st.device_link.claim(data))
        if path=='/api/devices/upload':return self.reply(202,st.device_link.upload(data))
        if path=='/api/devices/forget':return self.reply(200,st.device_link.forget(data))
        if path=='/api/logout':
            del st.sessions[token]; return self.reply(200,{'ok':True},'cast_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0')
        if path=='/api/actors': return self.reply(201,st.save_actor(data))
        if path.startswith('/api/actors/'):
            return self.reply(200,st.save_actor(data,path[len('/api/actors/'):]))
        require(path in ['/api/draft','/api/preview','/api/publish'],'接口不存在',404)
        date=date_value(data.get('date')); draft=st.draft(date)
        require(type(data.get('version')) is int and data['version']==draft['version'],'草稿已被其他窗口修改，请重新加载',409)
        if path=='/api/draft':
            clean=st.validate(data.get('venues'),False)
            draft={'date':date,'version':draft['version']+1,'venues':clean}
            state=copy.deepcopy(st.state); state['drafts'][date]=draft; st.commit(state)
            return self.reply(200,{**draft,'revision':st.revision(date)})
        require(type(data.get('revision')) is int and data['revision']==st.revision(date),'发布版本已变化，请重新预览',409)
        clean=st.validate(draft['venues'],True)
        lineup={'schema_version':1,'date':date,'revision':st.revision(date)+1,'venues':clean}
        try: profiles=device_contract.exports(lineup,st.lookup)
        except ValueError as e: raise Problem(422,str(e))
        if path=='/api/preview': return self.reply(200,lineup)
        state=copy.deepcopy(st.state); state['published'][date]=lineup
        state.setdefault('device_profiles',{})[date]={'revision':lineup['revision'],'profiles':profiles}
        state['audit'].append({'date':date,'revision':lineup['revision'],'draft_version':draft['version'],'at':dt.datetime.now(dt.timezone.utc).isoformat(),'actor':'admin'})
        st.commit(state); self.reply(200,lineup)

def make_server(folder,source,host='127.0.0.1',port=8766,secure_cookie=False,public_origin=None,allowed_hosts=()):
    if public_origin:
        u=urlsplit(public_origin)
        if u.scheme not in ('http','https') or not u.hostname or u.path or u.query or u.fragment or u.username or u.password:
            raise ValueError('public-origin must be an HTTP(S) origin without a path')
        secure_cookie=u.scheme=='https'
    store=Store(folder,source)
    server=ThreadingHTTPServer((host,port),Handler); server.store=store; server.secure_cookie=secure_cookie; server.public_origin=public_origin
    p=server.server_port; server.allowed_hosts={f'127.0.0.1:{p}',f'localhost:{p}'}
    if host not in ['127.0.0.1','localhost']: server.allowed_hosts.add(f'{host}:{p}')
    server.allowed_hosts.update(allowed_hosts)
    if public_origin: server.allowed_hosts.add(urlsplit(public_origin).netloc)
    return server

if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--host',default='127.0.0.1'); parser.add_argument('--port',type=int,default=8766)
    parser.add_argument('--data-dir',default=str(ROOT/'runtime')); parser.add_argument('--source',default=str(ROOT/'data/history.json')); parser.add_argument('--secure-cookie',action='store_true')
    parser.add_argument('--public-origin'); parser.add_argument('--allow-host',action='append',default=[])
    args=parser.parse_args(); server=make_server(args.data_dir,args.source,args.host,args.port,args.secure_cookie,args.public_origin,args.allow_host)
    print(f'Cast editor: http://{args.host}:{server.server_port}; credentials file: {Path(args.data_dir)/"credentials.json"}',flush=True)
    try: server.serve_forever()
    except KeyboardInterrupt: pass
    finally: server.server_close()
