"""Authenticated device presence, explicit pairing and acknowledged lineup delivery."""
import copy,hashlib,re,secrets,time,uuid

class DeviceProblem(Exception):
    def __init__(self,status,message):self.status,self.message=status,message
def check(ok,message,status=422):
    if not ok:raise DeviceProblem(status,message)
def hex_string(value,length):
    return isinstance(value,str) and re.fullmatch('[0-9a-f]{'+str(length)+'}',value) is not None

class DeviceLink:
    def __init__(self,store):
        self.store=store;self.pending={};self.seen={};self.attempts=[]
    def records(self):return self.store.state.get('linked_devices',{})
    def token_hash(self,authorization):
        check(isinstance(authorization,str) and authorization.startswith('Bearer '),'设备凭据无效',401)
        token=authorization[7:];check(hex_string(token,64),'设备凭据无效',401)
        return hashlib.sha256(token.encode()).hexdigest()
    def authenticate(self,device,authorization):
        check(hex_string(device,32),'设备标识无效')
        record=self.records().get(device);digest=self.token_hash(authorization)
        check(record is not None and secrets.compare_digest(record['token_hash'],digest),'设备未配对或凭据无效',403)
        return record
    def heartbeat(self,data,authorization):
        check(set(data) in ({'id','code','ack'},{'id','code','ack','protocol'}),'设备请求字段无效')
        protocol=data.get('protocol',1);check(type(protocol) is int and protocol in (1,2),'设备协议无效')
        device,code,ack=data['id'],data['code'],data['ack'];check(hex_string(device,32),'设备标识无效')
        check(isinstance(code,str) and (not code or re.fullmatch('[0-9A-F]{8}',code)),'配对码无效')
        check(ack is None or (isinstance(ack,dict) and set(ack)=={'id','result'} and hex_string(ack['id'],32) and ack['result'] in ('saved','failed')),'回执无效')
        digest=self.token_hash(authorization);now=time.monotonic();record=self.records().get(device)
        self.pending={k:v for k,v in self.pending.items() if v['expires']>now}
        if record:
            check(secrets.compare_digest(record['token_hash'],digest),'设备凭据无效',403)
            self.seen[device]=now
            if record.get('protocol',1)!=protocol:
                state=copy.deepcopy(self.store.state);state['linked_devices'][device]['protocol']=protocol;self.store.commit(state)
            job=self.store.state.get('delivery_jobs',{}).get(device)
            if job and job['status']=='queued' and protocol>=2:
                state=copy.deepcopy(self.store.state);state['delivery_jobs'][device]['status']='transferring';self.store.commit(state);job=state['delivery_jobs'][device]
            if ack and job and ack['id']==job['id'] and job['status'] in ('queued','transferring'):
                state=copy.deepcopy(self.store.state);state['delivery_jobs'][device]['status']=ack['result'];self.store.commit(state)
                job=state['delivery_jobs'][device]
            queued=job and job['status'] in ('queued','transferring')
            return {'paired':True,'job':{k:job[k] for k in ('id','date','revision')} if queued else None}
        check(bool(code),'请在设备上申请新的配对码',409)
        old=self.pending.get(device)
        if old:check(secrets.compare_digest(old['token_hash'],digest),'设备凭据无效',403)
        check(old is not None or len(self.pending)<64,'待配对设备过多',429)
        if not old or old['code']!=code:self.pending[device]={'token_hash':digest,'code':code,'expires':now+300}
        self.pending[device]['last_seen']=now
        self.pending[device]['protocol']=protocol
        return {'paired':False,'job':None}
    def claim(self,data):
        check(set(data)=={'code'} and isinstance(data['code'],str),'请输入设备配对码')
        now=time.monotonic();self.attempts=[t for t in self.attempts if now-t<60]
        check(len(self.attempts)<10,'配对尝试过多，请稍后再试',429);self.attempts.append(now)
        code=data['code'].strip().upper();check(re.fullmatch('[0-9A-F]{8}',code),'配对码应为八位字符')
        found=[(k,v) for k,v in self.pending.items() if v['expires']>now and now-v['last_seen']<45 and secrets.compare_digest(v['code'],code)]
        check(len(found)==1,'配对码不存在、已过期或冲突，请在设备重新申请',409)
        device,entry=found[0];check(len(self.records())<32,'设备数量已达上限',429)
        state=copy.deepcopy(self.store.state)
        state.setdefault('linked_devices',{})[device]={'token_hash':entry['token_hash'],'name':'Passport '+device[:6],'protocol':entry.get('protocol',1)}
        self.store.commit(state);self.seen[device]=entry['last_seen'];del self.pending[device]
        return {'id':device,'name':state['linked_devices'][device]['name']}
    def listing(self):
        now=time.monotonic();out=[]
        for device,record in self.records().items():
            job=self.store.state.get('delivery_jobs',{}).get(device)
            out.append({'id':device,'name':record['name'],'protocol':record.get('protocol',1),'online':device in self.seen and now-self.seen[device]<45,
                'delivery':{k:job[k] for k in ('id','date','revision','status')} if job else None})
        return {'devices':out}
    def upload(self,data):
        check(set(data)=={'device','date','revision'},'上传字段无效')
        device=data['device'];check(isinstance(device,str) and device in self.records(),'设备未配对',404)
        check(device in self.seen and time.monotonic()-self.seen[device]<45,'设备离线，请先连接 Wi-Fi',409)
        check(isinstance(data['date'],str),'日期无效')
        lineup=self.store.state['published'].get(data['date']);check(lineup is not None,'请先发布该日期的名单',409)
        check(lineup['schema_version']<=self.records()[device].get('protocol',1),'请先将这台设备升级到 r5 固件，再上传新名单',409)
        check(type(data['revision']) is int and data['revision']==lineup['revision'],'发布版本已变化，请刷新后再上传',409)
        job=self.store.state.get('delivery_jobs',{}).get(device)
        check(not job or job['status'] in ('saved','failed'),'该设备仍有上传任务，请等待回执',409)
        job={'id':uuid.uuid4().hex,'date':lineup['date'],'revision':lineup['revision'],'status':'queued','lineup':copy.deepcopy(lineup)}
        state=copy.deepcopy(self.store.state);state.setdefault('delivery_jobs',{})[device]=job;self.store.commit(state)
        return {k:job[k] for k in ('id','date','revision','status')}
    def content(self,device,job_id,authorization):
        self.authenticate(device,authorization);check(hex_string(job_id,32),'任务标识无效')
        job=self.store.state.get('delivery_jobs',{}).get(device)
        check(job and job['id']==job_id,'上传任务不存在',404)
        if job['status']=='queued':
            state=copy.deepcopy(self.store.state);state['delivery_jobs'][device]['status']='transferring';self.store.commit(state)
        return job['lineup']
    def forget(self,data):
        check(set(data)=={'device'} and isinstance(data['device'],str),'设备标识无效')
        device=data['device'];check(device in self.records(),'设备不存在',404)
        state=copy.deepcopy(self.store.state);del state['linked_devices'][device]
        state.get('delivery_jobs',{}).pop(device,None);self.store.commit(state);self.seen.pop(device,None)
        return {'ok':True}
    def rename(self,data):
        check(set(data)=={'device','name'},'备注字段无效')
        device,name=data['device'],data['name']
        check(isinstance(device,str) and device in self.records(),'设备不存在',404)
        check(isinstance(name,str) and 1<=len(name.strip())<=60 and
              not any(ord(c)<32 for c in name),'备注须为 1 至 60 字')
        state=copy.deepcopy(self.store.state);state['linked_devices'][device]['name']=name.strip()
        self.store.commit(state);return {'ok':True}
    def batch(self,data):
        action=data.get('action');devices=data.get('devices')
        expected={'action','devices','date','revision'} if action=='upload' else {'action','devices'}
        check(action in ('upload','forget') and set(data)==expected,'批量操作字段无效')
        check(isinstance(devices,list) and 1<=len(devices)<=32 and
              all(isinstance(d,str) for d in devices) and len(set(devices))==len(devices),'请选择不重复的设备')
        # Explicit IDs bind a destructive all-selection to the list the user actually saw.
        if action=='forget':
            check(all(d in self.records() for d in devices),'设备列表已变化，请刷新后重试',409)
            state=copy.deepcopy(self.store.state)
            for d in devices:
                del state['linked_devices'][d];state.get('delivery_jobs',{}).pop(d,None)
            self.store.commit(state)
            for d in devices:self.seen.pop(d,None)
            return {'results':[{'device':d,'ok':True} for d in devices]}
        results=[]
        for d in devices:
            try:
                result=self.upload({'device':d,'date':data['date'],'revision':data['revision']})
                results.append({'device':d,'ok':True,'delivery':result})
            except DeviceProblem as e:results.append({'device':d,'ok':False,'error':e.message})
        return {'results':results}
