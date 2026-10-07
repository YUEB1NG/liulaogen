import copy,http.client,json,unittest
from unittest.mock import patch
import test_api

class DeviceAPI(unittest.TestCase):
    setUp=test_api.API.setUp
    start=test_api.API.start
    tearDown=test_api.API.tearDown
    request=test_api.API.request
    login=test_api.API.login
    payload=test_api.API.payload
    save=test_api.API.save
    device='a'*32
    bearer='Bearer '+'b'*64
    def device_request(self,path,body=None,bearer=None):
        headers={'Authorization':bearer or self.bearer,'Content-Type':'application/json'}
        c=http.client.HTTPConnection('127.0.0.1',self.port,timeout=5)
        c.request('POST' if body is not None else 'GET',path,json.dumps(body) if body is not None else None,headers)
        r=c.getresponse();status=r.status;data=json.loads(r.read());c.close();return status,data
    def beat(self,ack=None,code='A123B456',bearer=None):
        return self.device_request('/api/device/heartbeat',{'id':self.device,'code':code,'ack':ack},bearer)
    def pair(self):
        self.login();self.assertEqual(self.beat()[0],200)
        self.assertEqual(self.request('/api/devices/claim',{'code':'a123b456'})[0],200)
    def upload(self):
        p=self.save();self.assertEqual(self.request('/api/publish',p)[0],200)
        return self.request('/api/devices/upload',{'device':self.device,'date':p['date'],'revision':1})
    def test_roundtrip_requires_saved_ack(self):
        self.pair();status,job,_=self.upload();self.assertEqual(status,202)
        self.assertEqual(self.beat()[1]['job']['id'],job['id'])
        listing=lambda:self.request('/api/devices')[1]['devices'][0]
        self.assertTrue(listing()['online']);self.assertEqual(listing()['delivery']['status'],'queued')
        content=f'/api/device/content?id={self.device}&job={job["id"]}'
        self.assertEqual(self.device_request(content,bearer='Bearer '+'c'*64)[0],403)
        code,lineup=self.device_request(content);self.assertEqual(code,200);self.assertEqual(lineup['revision'],1)
        self.assertEqual(listing()['delivery']['status'],'transferring')
        self.beat({'id':'0'*32,'result':'saved'});self.assertEqual(listing()['delivery']['status'],'transferring')
        self.assertIsNone(self.beat({'id':job['id'],'result':'saved'})[1]['job'])
        self.assertEqual(listing()['delivery']['status'],'saved')
        self.beat({'id':job['id'],'result':'failed'});self.assertEqual(listing()['delivery']['status'],'saved')
        self.assertNotIn(self.bearer[7:],(self.folder/'state.json').read_text())
    def test_pair_permissions_and_bad_requests(self):
        self.assertEqual(self.request('/api/devices')[0],401)
        self.assertEqual(self.request('/api/devices/claim',{'code':'A123B456'})[0],401)
        self.login();self.assertEqual(self.request('/api/devices/claim',{'code':'A123B456'},csrf=False)[0],403)
        self.assertEqual(self.request('/api/devices/claim',{'code':'A123B456'})[0],409)
        self.assertEqual(self.beat(code='')[0],409)
        self.assertEqual(self.beat(bearer='Bearer wrong')[0],401)
        self.assertEqual(self.device_request('/api/device/heartbeat',{'id':self.device})[0],422)
        self.assertEqual(self.beat()[0],200)
        self.assertEqual(self.beat(bearer='Bearer '+'c'*64)[0],403)
        self.assertEqual(self.request('/api/devices/claim',{'code':'A123B456'})[0],200)
        self.assertEqual(self.beat(bearer='Bearer '+'c'*64)[0],403)
    def test_expired_pair_and_presence(self):
        self.login()
        with patch('device_link.time.monotonic',return_value=100):self.beat()
        with patch('device_link.time.monotonic',return_value=401):self.assertEqual(self.request('/api/devices/claim',{'code':'A123B456'})[0],409)
        with patch('device_link.time.monotonic',return_value=500):
            self.beat();self.assertEqual(self.request('/api/devices/claim',{'code':'A123B456'})[0],200)
        with patch('device_link.time.monotonic',return_value=546):
            self.assertFalse(self.request('/api/devices')[1]['devices'][0]['online'])
            self.assertEqual(self.upload()[0],409)
    def test_frozen_job_conflict_failed_and_restart(self):
        self.pair();_,job,_=self.upload()
        data={'device':self.device,'date':'2026-10-03','revision':1}
        self.assertEqual(self.request('/api/devices/upload',data)[0],409)
        self.assertEqual(self.request('/api/publish',{'date':data['date'],'version':1,'revision':1})[0],200)
        self.assertEqual(self.device_request(f'/api/device/content?id={self.device}&job={job["id"]}')[1]['revision'],1)
        self.beat({'id':job['id'],'result':'failed'})
        self.assertEqual(self.request('/api/devices/upload',data)[0],409)
        data['revision']=2;self.assertEqual(self.request('/api/devices/upload',data)[0],202)
        self.server.shutdown();self.server.server_close();self.thread.join();self.start();self.login()
        self.assertFalse(self.request('/api/devices')[1]['devices'][0]['online'])
        self.assertEqual(self.beat()[1]['job']['revision'],2)
        self.assertEqual(self.request('/api/devices/forget',{'device':self.device})[0],200)
        self.assertEqual(self.beat(code='')[0],409);self.assertEqual(self.request('/api/devices')[1]['devices'],[])
    def test_atomic_upload_ack_failures(self):
        self.pair();_,job,_=self.upload();before=(self.folder/'state.json').read_bytes()
        with patch('server.os.replace',side_effect=OSError('injected')):
            self.assertEqual(self.beat({'id':job['id'],'result':'saved'})[0],500)
        self.assertEqual((self.folder/'state.json').read_bytes(),before)
        self.assertEqual(self.request('/api/devices')[1]['devices'][0]['delivery']['status'],'queued')
        self.assertIsNone(self.beat({'id':job['id'],'result':'saved'})[1]['job'])

if __name__=='__main__':unittest.main()
