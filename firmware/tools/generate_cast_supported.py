from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'assets/fonts/cast-glyphs.txt').read_text().rstrip('\n')
(root/'main/cast_supported.h').write_text('/* Generated from cast-glyphs.txt */\nstatic const unsigned int cast_supported[]={'+','.join(str(ord(c)) for c in s)+'};\n')
