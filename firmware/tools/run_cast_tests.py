#!/usr/bin/env python3
"""Compile application host tests using the ESP-IDF 5.5.3 cJSON revision."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
PIN = 'c859b25da02955fef659d658b8f324b5cde87be3'
HASHES = {
    'cJSON.c': '298581a04a36c0165da4b0aade235c23088cb2faa58651d720ea2f3706ed0b0d',
    'cJSON.h': '25b0145150d500498e4d209cec69c18c42cf818bffcc54690be3b895a2a16dee',
}


def run(*args):
    subprocess.run(list(map(str, args)), cwd=ROOT, check=True)


def main():
    out = ROOT / 'build/host'
    out.mkdir(parents=True, exist_ok=True)
    if '--font-runtime' in sys.argv:
        lvgl = ROOT / 'managed_components/lvgl__lvgl'
        exe = out / ('test_cast_font_runtime.exe' if os.name == 'nt' else 'test_cast_font_runtime')
        run(os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-DLV_CONF_SKIP', '-DLV_USE_FONT_COMPRESSED=0', '-DLV_USE_LOG=0',
            '-DLV_USE_ASSERT_NULL=0', '-DLV_USE_ASSERT_MALLOC=0', '-Imain', f'-I{lvgl}',
            'tests/test_cast_font_runtime.c', 'assets/fonts/cast_font_16.c', 'main/cast_data.c', 'main/cast_input.c',
            lvgl / 'src/font/lv_font.c', lvgl / 'src/font/fmt_txt/lv_font_fmt_txt.c',
            lvgl / 'src/font/lv_font_montserrat_14.c', lvgl / 'src/misc/lv_text.c', '-o', exe)
        run(exe)
        return
    idf = os.environ.get('IDF_PATH')
    dep = Path(idf) / 'components/json/cJSON' if idf else out / 'cJSON'
    dep.mkdir(parents=True, exist_ok=True)
    for name, digest in HASHES.items():
        file = dep / name
        if not file.exists() and not idf:
            with urllib.request.urlopen(f'https://raw.githubusercontent.com/DaveGamble/cJSON/{PIN}/{name}', timeout=60) as response:
                data = response.read()
            assert hashlib.sha256(data).hexdigest() == digest
            file.write_bytes(data)
        # Git on Windows may check C sources out with CRLF.
        assert hashlib.sha256(file.read_bytes().replace(b'\r\n', b'\n')).hexdigest() == digest, name
    cc = os.environ.get('CC', 'cc')
    suffix = '.exe' if os.name == 'nt' else ''
    startup = out / f'test_cast_startup{suffix}'
    run(cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
        '-Itests/app_stubs', '-Itests/demo_stubs', '-Imain',
        'tests/test_cast_startup.c', 'main/cast_state.c', 'main/cast_data.c', '-o', startup)
    run(startup)
    startup_wifi = out / f'test_cast_startup_wifi{suffix}'
    run(cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-DCONFIG_CAST_WIFI_PORTAL=1',
        '-Itests/app_stubs', '-Itests/demo_stubs', '-Imain',
        'tests/test_cast_startup.c', 'main/cast_state.c', 'main/cast_data.c', 'main/cast_input.c', '-o', startup_wifi)
    run(startup_wifi)
    common = [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-Imain', f'-I{dep}']
    text_input = out / f'test_cast_input{suffix}'
    run(*common, 'tests/test_cast_input.c', 'main/cast_input.c', '-o', text_input)
    run(text_input)
    remote = out / f'test_cast_remote{suffix}'
    run(*common, '-Itests/network_stubs', 'tests/test_cast_remote.c', 'main/cast_remote_protocol.c',
        'main/cast_net_config.c', 'main/cast_protocol.c', 'main/cast_data.c', dep / 'cJSON.c', '-o', remote)
    run(remote)
    net = out / f'test_cast_net_config{suffix}'
    run(*common, 'tests/test_cast_net_config.c', 'main/cast_net_config.c', '-o', net)
    run(net)
    network = out / f'test_cast_network{suffix}'
    run(*common, '-Itests/network_stubs', 'tests/test_cast_network.c',
        'main/cast_net_config.c', dep / 'cJSON.c', '-o', network)
    run(network)
    profile = out / f'test_cast_profile{suffix}'
    run(*common, 'tests/test_cast_profile.c', 'main/cast_profile_online.c', 'main/cast_protocol.c', 'main/cast_data.c', dep / 'cJSON.c', '-o', profile)
    run(sys.executable, 'tests/test_cast_profile.py', profile)
    for name in ('state', 'online'):
        exe = out / f'test_cast_{name}{suffix}'
        sources = [f'tests/test_cast_{name}.c', 'main/cast_state.c', 'main/cast_data.c']
        if name == 'online':
            sources += ['main/cast_protocol.c', dep / 'cJSON.c']
        run(*common, *sources, '-o', exe)
        run(exe)
    run(sys.executable, 'tests/test_cast_protocol.py', out / f'test_cast_online{suffix}')
    for configured in (0, 1, 2):
        exe = out / f'test_cast_sync_{int(configured)}{suffix}'
        defines = ['-DCONFIG_CAST_BASE_URL="https://example.invalid"'] if configured else []
        if configured == 2:
            defines += ['-DCONFIG_CAST_WIFI_PORTAL=1']
        run(*common, '-Itests/cast_stubs', *defines, 'tests/test_cast_sync.c',
            'main/cast_protocol.c', 'main/cast_profile_online.c', 'main/cast_data.c', dep / 'cJSON.c', '-o', exe)
        run(exe)
    run(sys.executable, 'tools/check_cast_font.py')
    run(sys.executable, 'tools/generate_cast.py', '--check')


if __name__ == '__main__':
    main()
