#!/usr/bin/env python3
"""Check the shipped glyph map, bitmap bounds, hashes and application bindings."""
import ast
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def check():
    fonts = ROOT / 'assets/fonts'
    manifest = json.loads((fonts / 'cast-font-manifest.json').read_text(encoding='utf-8'))
    for name, expected in manifest['sha256'].items():
        assert hashlib.sha256((fonts / name).read_bytes()).hexdigest() == expected, name
    inventory = set(map(ord, (fonts / 'cast-glyphs.txt').read_text(encoding='utf-8').rstrip('\n')))
    assert inventory == set(manifest['codepoints'])
    source = (fonts / 'cast_font_16.c').read_text(encoding='utf-8')
    offsets = re.search(r'unicode_list\[\]\s*=\s*\{([^}]+)', source)[1]
    cmap = [32 + int(n) for n in re.findall(r'\d+', offsets)]
    assert cmap == sorted(inventory), 'generated LVGL cmap differs from inventory'
    assert 0x9F98 not in cmap, 'negative coverage control must remain unsupported'
    supported = (ROOT / 'main/cast_supported.h').read_text(encoding='utf-8')
    assert set(map(int, re.findall(r'\d+', supported))) == inventory
    bitmap = re.search(r'glyph_bitmap\[\]\s*=\s*\{(.*?)\};', source, re.S)[1]
    bitmap_bytes = len(re.findall(r'0x[0-9a-fA-F]{2}', bitmap))
    descriptors = re.findall(r'\.bitmap_index=(\d+), \.adv_w=(\d+), \.box_w=(\d+), \.box_h=(\d+)', source)
    assert len(descriptors) == len(cmap)
    for index, advance, width, height in descriptors:
        assert int(index) + (int(width) * int(height) + 1) // 2 <= bitmap_bytes
        assert int(advance) > 0
    required = set()
    for name in ('main.c', 'cast_state.c', 'cast_data.c'):
        code = (ROOT / 'main' / name).read_text(encoding='utf-8')
        for literal in re.findall(r'"(?:[^"\\]|\\.)*"', code):
            required.update(ord(c) for c in ast.literal_eval(literal) if ord(c) >= 32)
    missing = required - inventory
    assert not missing, 'Missing glyphs: ' + ', '.join(f'U+{cp:04X}' for cp in sorted(missing))
    main = (ROOT / 'main/main.c').read_text(encoding='utf-8')
    assert main.count('lv_label_create(') == 1, 'audit any additional label creation path'
    assert 'lv_obj_set_style_text_font(o, &cast_font_16, 0)' in main
    assert 'assets/fonts/cast_font_16.c' in (ROOT / 'main/CMakeLists.txt').read_text()
    print(f'Cast font: PASS ({len(inventory)} glyphs, {len(required)} required, '
          f'{bitmap_bytes} bitmap bytes; unknown-glyph negative control PASS)')


if __name__ == '__main__':
    check()
