"""The device export contract is checked before a publication is committed."""
import json
from pathlib import Path

MAX_BODY=8192
MAX_PAGES=24
GLYPHS=set(json.loads((Path(__file__).parent/'data/device-glyphs.json').read_text('utf8')))

def encode(value):
    return json.dumps(value,ensure_ascii=False,separators=(',',':')).encode('utf8')

def text(value,label,limit=None):
    if limit and len(value.encode('utf8'))>limit:
        raise ValueError(f'{label}超出设备 {limit} 字节限制（汉字通常占 3 字节）')
    missing=sorted({c for c in value if c!='\n' and ord(c) not in GLYPHS})
    if missing: raise ValueError(f'{label}含设备字库未收录文字：'+''.join(missing[:16]))

def pages(title,body):
    text(title,'资料标题',24);text(body,'演员资料')
    if len(title)>8: raise ValueError('设备资料标题最多 8 字，请缩短舞台特色标题')
    lines=[]
    for line in body.split('\n'):
        lines.extend([line[i:i+11] for i in range(0,len(line),11)] or [''])
    return [{'title':title,'text':'\n'.join(lines[i:i+7])} for i in range(0,len(lines),7)]

def exports(lineup,lookup):
    count=0;profiles={}
    for venue in lineup['venues']:
        for session in venue['sessions']:
            for group in session['groups']:
                count+=1;content=[]
                for member in group['members']:
                    text(member['name'],'演员姓名',24)
                    actor=lookup[member['id']]
                    sections=[]
                    if actor['credits']: sections.append(('作品与角色','\n'.join(actor['credits'])))
                    if actor['bio']: sections.append(('个人简介',actor['bio']))
                    for item in actor['user_highlights']: sections.append((item['label'],item['text']))
                    if not actor['user_edited']:
                        for item in actor['highlights']: sections.append((item['label'],item['text']))
                    if not sections: sections=[('资料说明','资料待补充')]
                    for title,body in sections:
                        content.extend(pages(title,member['name']+'\n'+body))
                if len(content)>MAX_PAGES:
                    raise ValueError(' · '.join(m['name'] for m in group['members'])+f' 的资料超过设备 {MAX_PAGES} 页，请精简资料后发布')
                ids=[m['id'] for m in group['members']]
                profile={'schema_version':1,'date':lineup['date'],'revision':lineup['revision'],'members':ids,'pages':content}
                if len(encode(profile))>MAX_BODY: raise ValueError('演员资料超过设备 8192 字节容量，请精简')
                profiles['|'.join(ids)]=profile
    if count>80: raise ValueError('当天阵容超过设备 80 组容量')
    if len(encode(lineup))>MAX_BODY: raise ValueError('阵容超过设备 8192 字节容量，请减少场次或组数')
    return profiles
