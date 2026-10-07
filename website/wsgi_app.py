"""WSGI adapter for the existing API; no background HTTP server or dependencies."""
import io,os
from email.message import Message
from http.server import BaseHTTPRequestHandler
from types import SimpleNamespace
from urllib.parse import quote,urlsplit
from pathlib import Path
from server import Handler,Store,ROOT

class Request(Handler):
    def __init__(self,context,environ):
        self.server=context
        self.command=environ.get('REQUEST_METHOD','GET')
        self.path=quote(environ.get('PATH_INFO','/').encode('latin1').decode('utf8'),safe='/')
        if environ.get('QUERY_STRING'):self.path+='?'+environ['QUERY_STRING']
        self.client_address=(environ.get('REMOTE_ADDR','unknown'),0)
        self.headers=Message()
        for name,value in environ.items():
            # uWSGI may expose these aliases as well as the canonical WSGI
            # fields. They describe one header, not duplicate client headers.
            if name.startswith('HTTP_') and name not in ('HTTP_CONTENT_TYPE','HTTP_CONTENT_LENGTH'):
                self.headers[name[5:].replace('_','-')]=value
        for name in ('CONTENT_TYPE','CONTENT_LENGTH'):
            if environ.get(name):self.headers[name.replace('_','-')]=environ[name]
        self.rfile=environ['wsgi.input'];self.wfile=io.BytesIO()
        self.response_status=500;self.response_headers=[]
    def send_response(self,code,message=None):self.response_status=code
    def send_header(self,name,value):self.response_headers.append((name,str(value)))
    def end_headers(self):pass

class Application:
    def __init__(self,folder,origin,source=ROOT/'data/history.json'):
        u=urlsplit(origin)
        if u.scheme!='https' or not u.hostname or u.path or u.query or u.fragment or u.username or u.password:
            raise ValueError('WSGI requires a public HTTPS root origin')
        if not Path(folder).is_absolute():raise ValueError('Data directory must be absolute and private')
        self.context=SimpleNamespace(store=Store(folder,source),public_origin=origin,secure_cookie=True,allowed_hosts={u.netloc})
    def __call__(self,environ,start_response):
        request=Request(self.context,environ)
        if request.command in ('GET','POST'):request.dispatch(request.command=='POST')
        else:request.reply(405,{'error':{'code':405,'message':'Unsupported method'}})
        code=request.response_status;reason=BaseHTTPRequestHandler.responses.get(code,('Unknown',''))[0]
        start_response(f'{code} {reason}',request.response_headers)
        return [request.wfile.getvalue()]

def from_environment():
    return Application(os.environ['CAST_DATA_DIR'],os.environ['CAST_PUBLIC_ORIGIN'])
