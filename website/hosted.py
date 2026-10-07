"""Single-process hosting entry point behind a trusted TLS reverse proxy."""
import os
from pathlib import Path
from urllib.parse import urlsplit
from server import make_server,ROOT

def settings(env):
    origin=env.get('CAST_PUBLIC_ORIGIN') or env.get('RENDER_EXTERNAL_URL','')
    u=urlsplit(origin)
    if u.scheme!='https' or not u.hostname or u.path or u.query or u.fragment or u.username or u.password:
        raise ValueError('A public HTTPS root origin is required')
    folder=Path(env.get('CAST_DATA_DIR',''))
    if not env.get('CAST_DATA_DIR') or not folder.is_absolute():
        raise ValueError('CAST_DATA_DIR must name the absolute persistent-data directory')
    port=int(env.get('PORT','10000'))
    if not 1<=port<=65535:raise ValueError('PORT is outside 1..65535')
    return dict(folder=folder,source=ROOT/'data/history.json',host='0.0.0.0',port=port,public_origin=origin)

def main():
    server=make_server(**settings(os.environ))
    print('liulaogen server ready; credentials and device data stay in persistent storage.',flush=True)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close()

if __name__=='__main__':main()
