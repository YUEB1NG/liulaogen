[简体中文](README.zh_CN.md)

# AI Passport cast editor

Passport scans Wi-Fi and accepts the password on its physical buttons. Set the
deployed service origin on Passport, open website pairing, and enter its code in
the phone website. Choose a date, edit/save/preview/publish its lineup, then click
**上传到设备** and select the online Passport. Publication alone does not upload.
The website shows waiting, transferring and saved; only the device's durable ACK
confirms success. The complete downloaded 15-day lineup and biography window remains usable offline with r5.

The supplied interface is preserved; device management is an added dialog. No
travelling computer, phone renaming or 192.168.4.1 page is needed. A reachable HTTPS
deployment is available at https://quyue.pythonanywhere.com. Enter this root URL
on Passport; the flashed firmware has no preset URL. Public HTTPS, browser login
and simulated pairing/reload persistence have passed; real Passport upload remains
to be checked. See [validation status](../docs/status.md)

See [Run and deploy](DEPLOY.md)

Start `python server.py` and open
`http://127.0.0.1:8766` for local browser development. Runtime credentials stay local.
Administrators manage actor profiles and drafts; publication validates device
glyph/size limits and freezes lineup/profile data. With r5, upload verifies and saves a complete 15-day window, including biographies,
before acknowledging success. Previously published profile revisions remain available.
Older devices are asked to upgrade before new-schema uploads. This is not firmware OTA.
See [r5 behavior, controls and limits](../docs/r5-update.md)

Run `python -m unittest -v test_api test_device_link test_hosted test_wsgi test_features`. The delivery report separately
records browser/parser interoperability and unperformed hardware/server checks.
Original handoff notes remain in `REFERENCE_ORIGINAL.txt` as historical reference.
No credentials or runtime state belong in the source package.

Free hosting: [PythonAnywhere guide](../docs/hosting.md)
