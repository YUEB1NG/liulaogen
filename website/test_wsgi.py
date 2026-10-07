"""Run the existing full HTTP API/device suites through an actual WSGI server."""
import threading
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

if __name__=='__main__':
    import unittest
    unittest.main()
