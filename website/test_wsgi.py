"""Run the existing full HTTP API/device suites through an actual WSGI server."""
import io,json,tempfile,threading,unittest
from pathlib import Path
from wsgiref.simple_server import make_server,WSGIRequestHandler
import test_api,test_device_link
from wsgi_app import Application
from server import ROOT

class Quiet(WSGIRequestHandler):
    def log_message(self,*args):pass

class WSGIStart:
    def start(self):
        app=Application(self.folder.resolve(),'https://unit.example',ROOT/'data/history.json')
        self.server=make_server('127.0.0.1',0,app,handler_class=Quiet)
        self.port=self.server.server_port
        self.server.store=app.context.store
        self.server.allowed_hosts={f'127.0.0.1:{self.port}',f'localhost:{self.port}'}
        self.server.public_origin=None;self.server.secure_cookie=False
        app.context=self.server
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start()

class API(WSGIStart,test_api.API):pass
class Device(WSGIStart,test_device_link.DeviceAPI):pass

class HostedEnvironment(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.app=Application(Path(self.tmp.name).resolve(),'https://unit.example')
    def request(self,size,body):
        env={'REQUEST_METHOD':'POST','PATH_INFO':'/api/login','HTTP_HOST':'unit.example',
             'HTTP_ORIGIN':'https://unit.example','CONTENT_TYPE':'application/json',
             'CONTENT_LENGTH':str(size),'HTTP_CONTENT_TYPE':'application/json',
             'HTTP_CONTENT_LENGTH':str(size),'wsgi.input':body}
        response=[]
        raw=b''.join(self.app(env,lambda status,headers:response.append((status,headers))))
        return response[0],json.loads(raw)
    def test_uwsgi_content_header_aliases_allow_login(self):
        body=json.dumps(self.app.context.store.credentials).encode()
        (status,headers),data=self.request(len(body),io.BytesIO(body))
        self.assertEqual(status,'200 OK')
        self.assertIn('csrf',data)
        self.assertTrue(any(k=='Set-Cookie' and 'Secure' in v for k,v in headers))
    def test_uwsgi_aliases_keep_body_size_limit(self):
        class Unreadable:
            def read(self,*args):raise AssertionError('Oversized body must not be read')
        (status,_),_=self.request(262145,Unreadable())
        self.assertTrue(status.startswith('413 '))

if __name__=='__main__':
    import unittest
    unittest.main()
