[简体中文](README.zh_CN.md)

# AI Passport cast editor

Passport scans Wi-Fi and accepts the password on its physical buttons. Set the
deployed service origin on Passport, open website pairing, and enter its code in
the phone website. Choose a date, edit/save/preview/publish its lineup, then click
**上传到设备** and select the online Passport. Publication alone does not upload.
The website shows waiting, transferring and saved; only the device's durable ACK
confirms success. The latest downloaded lineup remains usable offline.

The supplied interface is preserved; device management is an added dialog. No
travelling computer, phone renaming or 192.168.4.1 page is needed. A reachable HTTPS
deployment is required away from the local test network. The domain is undecided;
no public server has been deployed or tested. Firmware currently has no preset URL.

See [Run and deploy](DEPLOY.md). Start `python server.py` and open
`http://127.0.0.1:8766` for local browser development. Runtime credentials stay local.
Administrators manage actor profiles and drafts; publication validates device
glyph/size limits and freezes lineup/profile data. Upload transfers the entire
selected published date. Profiles still download on demand on Passport; a success
ACK confirms the lineup only. Earlier frozen jobs keep their lineup after republish,
but an uncached older profile may become unavailable. This is not firmware OTA.

Run `python -m unittest -v test_api test_device_link`. The delivery report separately
records browser/parser interoperability and unperformed hardware/server checks.
Original handoff notes remain in `REFERENCE_ORIGINAL.txt` as historical reference.
No credentials or runtime state belong in the source package.

Free hosting: [PythonAnywhere guide](../docs/hosting.md).
