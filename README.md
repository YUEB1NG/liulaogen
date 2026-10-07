[简体中文](README.zh_CN.md)

# liulaogen

Live website: **https://quyue.pythonanywhere.com** (free PythonAnywhere hosting).

An open-source cast-lineup handbook for FoloToy AI Passport, with a mobile website
for managing actor profiles and explicitly uploading published lineups.

**Passport enters Wi-Fi credentials → website pairing → select a published date →
upload to a device → confirm its saved receipt.** Downloaded lineups remain usable
offline. The existing eight-city interface and historical library are preserved.

- `firmware/`: complete ESP32-C3 application, BSP, font assets, tests and build tools.
- `website/`: Python standard-library backend and existing mobile frontend.
- `website/deploy/pythonanywhere_wsgi.py`: free PythonAnywhere WSGI hosting template.
- [Hosting](docs/hosting.md), [validation status](docs/status.md), [licenses](docs/licenses.md).

## Run the website locally

```sh
cd website
python server.py
```

Open http://127.0.0.1:8766 and read `runtime/credentials.json` privately for initial
login. Never commit or share that file. For hosted service use the hosting guide;
GitHub Pages cannot run the Python backend. The live site above passed HTTPS,
login, mobile layout and simulated device pairing checks; see validation status
for the remaining physical-device acceptance.

## Firmware

Use native ESP-IDF **5.5.3**, target ESP32-C3 / 8 MB Flash / no PSRAM. Follow
[firmware instructions](firmware/docs/development/cast-wifi.md) and
[build verification](firmware/docs/development/engineering/build-and-test.md).
Run `tools/validate.ps1 --all` on an activated Windows environment, or
`bash tools/validate.sh` from `firmware/` on a supported POSIX environment.
The verified merged image belongs at **0x0** and resets stored configuration/cache;
do not substitute the application-only binary or perform a whole-chip erase.

Website tests: `python -m unittest -v test_api test_device_link test_hosted test_wsgi` inside
`website/`. Source checks run in GitHub Actions; firmware builds remain reproducible
using the documented IDF gate. No accounts, tokens, Wi-Fi credentials, private
runtime state, machine toolchains, build output or raw hardware logs are published.

MIT-licensed code; Noto fonts retain SIL OFL. This is a community derivative,
not an official FoloToy release or a verified statement of today's performance lineup.
