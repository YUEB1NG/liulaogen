import copy,json,subprocess,sys,tempfile
from pathlib import Path
base={'schema_version':1,'date':'2026-10-03','revision':1,'members':['a','b'],'pages':[{'title':'资料说明','text':'新演员\n作品与角色'}]}
cases=[(base,True)]
for key,values in {'date':['2026-10-04',None],'revision':[0,2,1.1,'1'],'schema_version':[2,None],'members':[['b','a'],['a'],['a','b','c']],'pages':[[],[base['pages'][0]]*25]}.items():
    for value in values:
        x=copy.deepcopy(base);x[key]=value;cases.append((x,False))
for key,values in {'title':['龘','x'*25,'x'*9,'资料\n说明',''],'text':['x'*14,'\n'.join(['字']*8),'龘','\x00','😀','\t','']}.items():
    for value in values:
        x=copy.deepcopy(base);x['pages'][0][key]=value;cases.append((x,False))
x=copy.deepcopy(base);x['pages']=[{'title':'资料说明','text':'字'*13+'\n'+('文'*13+'\n')*5+'字'*13}]*24;cases.append((x,True))
cases.extend([(json.dumps(base).replace('"revision": 1','"revision": 1,"revision": 1'),False),(json.dumps(base)+'{}',False),('['*500+']'*500,False)])
with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[1]/'build/host') as folder:
    path=Path(folder)/'profile.json'
    for value,expected in cases:
        raw=value if isinstance(value,str) else json.dumps(value,ensure_ascii=False,separators=(',',':'))
        path.write_text(raw,encoding='utf8')
        result=subprocess.run([sys.argv[1],str(path)],capture_output=True)
        assert result.returncode==(0 if expected else 2),(raw[:100],result.returncode,result.stderr)
print(f'Profile wire parser: PASS ({len(cases)} cases including revision/IDs, glyphs, layout, duplicate keys and limits)')
