#!/usr/bin/env python3
"""Shared native ESP-IDF build, merge, verification and debug-archive gate."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    os.chdir(ROOT)
    idf_path = os.environ.get('IDF_PATH')
    if not idf_path:
        raise RuntimeError('Activate ESP-IDF 5.5.3 before running the firmware gate')
    if os.name == 'nt' and os.environ.get('MSYSTEM'):
        raise RuntimeError('Use tools/validate.ps1 from the activated native ESP-IDF PowerShell')
    idf = [sys.executable, str(Path(idf_path) / 'tools/idf.py')]
    version = subprocess.check_output(idf + ['--version'], text=True).strip()
    if version != 'ESP-IDF v5.5.3':
        raise RuntimeError(f'ESP-IDF 5.5.3 required, got {version!r}')
    parent = ROOT / 'build/validation'
    parent.mkdir(parents=True, exist_ok=True)
    build = Path(tempfile.mkdtemp(prefix='firmware.', dir=parent))
    env = dict(os.environ, SDKCONFIG_DEFAULTS=str(ROOT / 'sdkconfig.defaults'))

    def run(*args):
        subprocess.run(list(map(str, args)), cwd=ROOT, env=env, check=True)

    run(*idf, '-B', build, '-D', f'SDKCONFIG={build / "sdkconfig"}', 'build')
    full = build / 'FoloToy-AI-Passport-full.bin'
    run(*idf, '-B', build, 'merge-bin', '-o', full)
    run(sys.executable, 'tools/verify_firmware.py', build)
    run(sys.executable, 'tools/archive_firmware.py', 'create', build,
        '--archive-root', ROOT / 'build/firmware')
    shutil.copyfile(full, ROOT / 'build' / full.name)
    print(f'Verified build retained: {build}')
    print('Firmware build: PASS')


if __name__ == '__main__':
    main()
