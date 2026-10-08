"""Persistent city settings and additive, id-preserving source imports."""
import copy
import hashlib
import json
import re
from pathlib import Path


def migrate(state, cities):
    state.setdefault('settings', {'version': 1, 'cities': [
        {'id': key, 'city': name, 'times': {'afternoon': '', 'evening': ''}}
        for key, name in cities]})
    source = Path(__file__).parent / 'data' / 'actors-20261008.json'
    if not source.exists():
        return
    raw = source.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest in state.get('imports', []):
        return
    by_name = {a['name']: a for a in state['actors'].values()}
    changed=set()
    for record in json.loads(raw)['actors']:
        name = record['name']
        actor = by_name.get(name)
        if actor is None:
            aid = 'a_' + hashlib.sha256(name.encode()).hexdigest()[:16]
            actor = {'id': aid, 'name': name, 'version': 1, 'credits': [],
                     'bio': '', 'highlights': [], 'user_highlights': [],
                     'aliases': [], 'source_notes': [], 'user_edited': False,
                     'origin': 'uploaded_catalog', 'historical_profile': None}
            state['actors'][aid] = actor
            by_name[name] = actor
        actor['catalog_source'] = copy.deepcopy(record)
        # Never replace an administrator's wording or merge ambiguous people.
        if not actor['user_edited']:
            if actor['bio']!=record['bio'] or actor['credits']!=record['credits']:
                actor['version']+=1;changed.add(actor['id'])
            actor['bio'] = record['bio'] or actor['bio']
            actor['credits'] = record['credits'] or actor['credits']
        actor['aliases'] = list(dict.fromkeys(actor['aliases'] + record['aliases']))
    state.setdefault('imports', []).append(digest)
    for draft in state['drafts'].values():
        if any(m['id'] in changed for v in draft['venues'] for s in v['sessions']
               for g in s['groups'] for m in g['members']):draft['version']+=1


def archive_existing(state):
    """Upgrade frozen legacy exports using their own content, never today's actor edits."""
    import device_contract
    for date,lineup in state['published'].items():
        key=date+':'+str(lineup['revision'])
        state.setdefault('publication_history',{}).setdefault(date,[copy.deepcopy(lineup)])
        record=state.get('device_profiles',{}).get(date)
        if not record or record['revision']!=lineup['revision']:continue
        state.setdefault('profile_versions',{}).setdefault(key,copy.deepcopy(record))
        record=state['profile_versions'][key]
        if 'profiles13' not in record:
            converted={}
            for pair,profile in record['profiles'].items():
                p=copy.deepcopy(profile);sections=[]
                for page in p['pages']:
                    if page['title'] not in ('作品与角色','个人简介','资料说明'):continue
                    # Eleven-character rows were soft wrapped by the previous exporter.
                    body=''.join(line+('' if len(line)==11 else '\n') for line in page['text'].split('\n')).rstrip('\n')
                    sections.extend(device_contract.pages(page['title'],body,13))
                p['pages']=sections or [{'title':'资料说明','text':'资料待补充'}]
                converted[pair]=p
            record['profiles13']=converted
        if key not in state.setdefault('archive_versions',{}):
            state['archive_versions'][key]=device_contract.archive(lineup,record['profiles13'],state.setdefault('device_blobs',{}))


def clean_settings(data, current, require):
    require(set(data) == {'version', 'cities'}, '设置字段无效')
    require(type(data['version']) is int and data['version'] == current['version'],
            '城市设置已变更，请重新打开设置', 409)
    require(isinstance(data['cities'], list) and 1 <= len(data['cities']) <= 16,
            '请保留 1 至 16 个城市')
    cities, ids, names = [], set(), set()
    for c in data['cities']:
        require(isinstance(c, dict) and set(c) == {'id', 'city', 'times'}, '城市字段无效')
        key, name, times = c['id'], c['city'], c['times']
        require(isinstance(key, str) and re.fullmatch(r'[a-zA-Z0-9_-]{1,32}', key), '城市 ID 无效')
        require(isinstance(name, str) and 1 <= len(name.strip()) <= 8 and
                not any(ord(x) < 32 for x in name), '城市名称须为 1 至 8 字')
        name = name.strip()
        require(key not in ids and name not in names, '城市不可重复')
        require(isinstance(times, dict) and set(times) == {'afternoon', 'evening'}, '场次时间无效')
        require(all(isinstance(t, str) and (t == '' or re.fullmatch(r'(?:[01]\d|2[0-3]):[0-5]\d', t))
                    for t in times.values()), '时间格式应为 HH:MM')
        ids.add(key); names.add(name)
        cities.append({'id': key, 'city': name, 'times': dict(times)})
    return {'version': current['version'] + 1, 'cities': cities}
