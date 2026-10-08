[简体中文](DEPLOY.zh_CN.md)

# Run and deploy

Python 3.13 is tested. The server uses only the standard library. Keep the
existing `static/` and `data/` directories next to `server.py` and
`device_contract.py` / `device_link.py`. No build step or JavaScript package installation is needed.

On Windows, run `./Start-Website.ps1` and open `http://127.0.0.1:8766`.
The first start creates a random administrator password in
`runtime/credentials.json`. Read that file locally; do not send its contents
in chat or include the runtime directory in a deployment bundle. Windows uses
the folder's inherited ACLs. POSIX mode 0600 is not a Windows ACL guarantee.
The parent folder must belong to the intended operator. On POSIX, generated
files are mode 0600 and the runtime folder is created with mode 0700.

For a private LAN trial, substitute the computer's actual private IPv4 address:

```powershell
./Start-Website.ps1 -BindAddress 192.168.1.10 -PublicOrigin http://192.168.1.10:8766
```

The phone, computer and Passport must reach the same LAN. Enter that same origin
in device provisioning. A router guest network can isolate clients. Firewall
access is a separate local configuration; this package does not change it.
HTTP origins on the device are restricted to RFC1918 IPv4 (10/8, 172.16/12,
192.168/16). Public deployments require HTTPS; no certificate bypass exists.

For a Linux server, the supplied service and Caddyfile are deployment templates,
not evidence of a live deployment. Install Python and Caddy using your server's
normal method, create a dedicated `cast-online` account, copy the website to
`/opt/cast-online-web`, and install `deploy/cast-online.service`. Set
`CAST_DOMAIN=your.actual.domain` in `/etc/cast-online.env`; set the same variable
in Caddy's service environment before using `deploy/Caddyfile`. DNS, ports,
certificate issuance and server readiness must be checked on the real host.
The Python process binds only to loopback behind the HTTPS proxy. The public
origin must exactly match the browser's scheme and authority; forwarded headers
are not trusted to choose an origin. HTTPS mode enables Secure cookies.
Run exactly one Python process for this state directory; the lock is in-process.

Keep `state.json` and `credentials.json` together in a private backup; stop the
server before restoring. File replacement flushes file contents on both systems
and the containing directory on POSIX. Hardware power-loss durability is not
established by the application tests.

Publication freezes both lineup and device profile pages in one state update.
Drafts remain private. Each export must fit 8192 UTF-8 bytes, at most 80 groups
overall and 12 per session; actor names fit 24 UTF-8 bytes. Each pair has at most
24 pages, 11 characters by 7 lines. The shipped 6875-glyph inventory is checked
before publication. Unsupported text and oversize content produce a specific
error and leave the previous publication intact. Library editing alone does
not change an existing publication. New device profiles require republishing
old publications imported from an earlier website version.

`python -m unittest -v test_api test_device_link` runs the API tests. The delivery report separately
records browser tests, firmware interoperability and unperformed server/device
checks. This package performs no remote deployment, DNS change or publication.

For r4, enter the service root URL directly on Passport, then use website pairing.
Allow authenticated `/api/device/heartbeat` POST and `/api/device/content` GET
through the proxy; preserve Authorization headers. Heartbeat needs no browser
Origin header, while administrator POST requests still require Origin/session/CSRF.
Device identities and one job per device persist in state.json. Online presence
expires after 45 seconds and resets on server restart. Removing a paired device
cancels pending work. Runtime backups include secret hashes and must remain private.
