"""Build content-addressed fifteen-day fixtures for the production cache reader."""
import datetime, hashlib, json, subprocess, sys
from pathlib import Path

root=Path(__file__).resolve().parents[1]
fixtures=root/'build/host/archive-fixtures'
cache=root/'build/host/archive'
fixtures.mkdir(parents=True,exist_ok=True);cache.mkdir(parents=True,exist_ok=True)
# Only remove explicitly generated test files in these two resolved test directories.
for folder in (fixtures,cache):
    assert folder.resolve().is_relative_to((root/'build/host').resolve())
    for file in folder.iterdir():
        if file.is_file():file.unlink()
def put(obj):
    raw=json.dumps(obj,ensure_ascii=False,separators=(',',':')).encode()
    digest=hashlib.sha256(raw).hexdigest();(fixtures/digest).write_bytes(raw);return digest
for version in range(3):
    rev=version+1
    profile={'members':['a','b'] if version<2 else ['wrong','b'],
             'pages':[{'title':'个人简介','text':'\n'.join([('字' if version==0 else '文')*13]*7)}]}
    ph=put(profile);days=[]
    for offset in range(15):
        date=(datetime.date(2026,10,1)+datetime.timedelta(days=offset)).isoformat()
        lineup={'schema_version':2,'date':date,'revision':rev,'cities':[{'id':'tianjin','city':'天津'}],
                'venues':[{'id':'tianjin','city':'天津','sessions':[{'id':'evening','label':'晚场','time':'19:00',
                'groups':[{'members':[{'id':'a','name':'演员甲'},{'id':'b','name':'演员乙'}]}]}]}]}
        archive=put({'date':date,'revision':rev,'lineup':put(lineup),'profiles':[ph]})
        days.append({'date':date,'revision':rev,'archive':archive})
    window={'today':'2026-10-08','days':days};wh=put(window)
    (fixtures/f'window{version}.json').write_bytes((fixtures/wh).read_bytes())
subprocess.run([sys.argv[1]],cwd=root,check=True)
