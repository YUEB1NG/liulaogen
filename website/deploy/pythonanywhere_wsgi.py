"""Copy into PythonAnywhere's WSGI configuration; replace YOUR_USERNAME below."""
import os,sys
from pathlib import Path
username='YOUR_USERNAME'
root=Path('/home')/username/'liulaogen'
sys.path.insert(0,str(root/'website'))
os.environ['CAST_PUBLIC_ORIGIN']=f'https://{username}.pythonanywhere.com'
os.environ['CAST_DATA_DIR']=str(Path('/home')/username/'.liulaogen-data')
from wsgi_app import from_environment
application=from_environment()
