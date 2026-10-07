import json,os,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from hosted import settings
from server import Store,ROOT
import test_api

class Hosting(unittest.TestCase):
    def test_settings_require_https_and_persistent_path(self):
        env={'CAST_DATA_DIR':str(Path(tempfile.gettempdir()).resolve()),'RENDER_EXTERNAL_URL':'https://sample.onrender.com','PORT':'10000'}
        result=settings(env);self.assertEqual(result['public_origin'],env['RENDER_EXTERNAL_URL']);self.assertEqual(result['port'],10000)
        for change in ({'RENDER_EXTERNAL_URL':''},{'RENDER_EXTERNAL_URL':'http://sample.onrender.com'},
                       {'RENDER_EXTERNAL_URL':'https://sample.onrender.com/path'},{'CAST_DATA_DIR':'relative'},{'PORT':'0'}):
            with self.assertRaises(ValueError):settings({**env,**change})
        self.assertEqual(settings({**env,'CAST_PUBLIC_ORIGIN':'https://custom.example'})['public_origin'],'https://custom.example')
    def test_initial_secret_and_restart_do_not_reset_credentials(self):
        with tempfile.TemporaryDirectory() as tmp:
            with patch.dict(os.environ,{'CAST_ADMIN_PASSWORD':'first-test-secret-123456'}):
                store=Store(tmp,ROOT/'data/history.json');self.assertEqual(store.credentials['password'],'first-test-secret-123456')
            with patch.dict(os.environ,{'CAST_ADMIN_PASSWORD':'different-test-secret-789'}):
                again=Store(tmp,ROOT/'data/history.json');self.assertEqual(again.credentials,store.credentials)
        with tempfile.TemporaryDirectory() as tmp:
            with patch.dict(os.environ,{'CAST_ADMIN_PASSWORD':'short'}):
                with self.assertRaises(ValueError):Store(tmp,ROOT/'data/history.json')
            self.assertFalse((Path(tmp)/'credentials.json').exists())

class Health(unittest.TestCase):
    setUp=test_api.API.setUp
    start=test_api.API.start
    tearDown=test_api.API.tearDown
    request=test_api.API.request
    def test_health_discloses_no_runtime_data(self):
        self.assertEqual(self.request('/healthz',auth=False)[:2],(200,{'ok':True}))
        self.assertEqual(self.request('/healthz',host='untrusted.example')[0],403)

if __name__=='__main__':unittest.main()
