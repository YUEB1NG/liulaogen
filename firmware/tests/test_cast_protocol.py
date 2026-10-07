"""Exercise the compiled parser with realistic and hostile wire payloads."""
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

exe = sys.argv[1]
cases = 0


def check(payload, expected):
    global cases
    raw = json.dumps(payload, ensure_ascii=False).encode() if isinstance(payload, dict) else payload
    with tempfile.NamedTemporaryFile(delete=False) as file:
        file.write(raw)
        name = file.name
    try:
        result = subprocess.run([exe, name], capture_output=True)
        assert result.returncode == (0 if expected else 2), (raw[:150], result.returncode, result.stderr)
        cases += 1
    finally:
        os.unlink(name)


base = {'schema_version': 1, 'date': '2026-10-03', 'revision': 1, 'venues': [
    {'id': 'zhongjie', 'city': '沈阳', 'sessions': [
        {'id': 'evening', 'label': '晚场', 'groups': [
            {'members': [{'id': 'a', 'name': '演员'}, {'id': 'b', 'name': '作品'}]}
        ]}
    ]}
]}
check(base, True)
uuid_actor = copy.deepcopy(base)
uuid_actor['venues'][0]['sessions'][0]['groups'][0]['members'][0]['id'] = '01234567-89ab-4cde-8f01-23456789abcd'
check(uuid_actor, True)
for count in (0, 1, 5, 6, 12, 13):
    obj = copy.deepcopy(base)
    obj['venues'][0]['sessions'][0]['groups'] = [
        {'members': [{'id': f'a{i}', 'name': '演员'}, {'id': f'b{i}', 'name': '作品'}]}
        for i in range(count)]
    check(obj, count <= 12)
for key, values in {'revision': [0, -1, 1.5, 4294967296, '1', None],
                    'schema_version': [0, 2, '1'],
                    'date': ['2026-10-04', '2026-02-29', '', None],
                    'venues': [None, {}, [None]]}.items():
    for value in values:
        obj = copy.deepcopy(base)
        obj[key] = value
        check(obj, False)
for path, values in [
    (('id',), ['beijing', 'bad id', '', 'a' * 33]),
    (('city',), ['北京', '不存在', None]),
    (('sessions', 0, 'id'), ['night', '']),
    (('sessions', 0, 'label'), ['龘', '\n', 'x' * 25]),
    (('sessions', 0, 'groups', 0, 'members', 1, 'id'), ['a', 'bad id', '龘', 'x' * 37]),
    (('sessions', 0, 'groups', 0, 'members', 0, 'name'), ['龘', 'x' * 25, '', '\x00', '\n']),
    (('sessions', 0, 'groups', 0, 'members'), [[], [None], [None, None], [None] * 3]),
]:
    for value in values:
        obj = copy.deepcopy(base)
        target = obj['venues'][0]
        for key in path[:-1]:
            target = target[key]
        target[path[-1]] = value
        check(obj, False)
raw = json.dumps(base, ensure_ascii=False).encode()
for invalid in [raw + b'garbage', raw + b'\0', raw[:-1], b'[' * 11 + b']' * 11,
                raw.replace(b'"revision": 1', b'"revision": 1, "revision": 2'),
                raw.replace('演员'.encode(), b'\xc0\xaf'),
                raw.replace(b'"revision": 1', b'"revision": 1e999'), b' ' * 8193]:
    check(invalid, False)
check(raw + b'\n\t ', True)
check(raw.replace('演员'.encode(), b'\\u6f14\\u5458'), True)
print(f'Cast protocol adversarial/limits: PASS ({cases} cases)')
