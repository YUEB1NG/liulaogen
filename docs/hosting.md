[简体中文](hosting.zh_CN.md)

# Free PythonAnywhere hosting

GitHub hosts the complete source. PythonAnywhere's free account runs the website
and device API together at one HTTPS origin, retaining session/CSRF protections
without cross-site cookies on iPhone. No paid resources are configured.

The current free plan provides one web app/worker and 512 MiB storage. The app
expires after one month unless you extend it from the dashboard; renew it regularly.
Free-plan capacity and availability are not a production SLA. A personal account
and its assigned HTTPS hostname are required. No custom domain purchase is needed.

## Setup

1. Create a free account at https://www.pythonanywhere.com/ and open a Bash console.
2. Clone `https://github.com/YUEB1NG/liulaogen.git` into `/home/YOUR_USERNAME/liulaogen`.
3. In the Web tab, add a web app with Manual configuration and Python 3.13.
   This app has no third-party Python dependencies and needs no pip installation.
4. Replace the generated WSGI configuration with
   `website/deploy/pythonanywhere_wsgi.py`, replacing `YOUR_USERNAME` with your
   actual account name. Verify the HTTPS hostname matches the Web tab; use the
   assigned hostname for your region. The private data directory is outside the
   public source tree, at `/home/YOUR_USERNAME/.liulaogen-data`.
5. Enable Force HTTPS in the Web tab and reload. Do not run `python server.py`
   in a background console; the platform invokes the WSGI application.
6. Use the private Files/console panel to read `.liulaogen-data/credentials.json`
   after first startup. Sign in as `admin` using its randomly generated password.
   Never put the file, password or hosting API token in GitHub or chat.
7. Enter the actual HTTPS root address on Passport, open website pairing, and
   enter its code in the website. Publish a date and explicitly upload it.
8. Verify saved ACK, offline use and persisted content after Web-tab reload.

The app checks Host against the configured public origin, uses Secure/HttpOnly
cookies and requires session/Origin/CSRF for administration. Device endpoints
use their own bearer authentication. `/healthz` returns only `{"ok":true}`.
Keep one web worker/process: in-memory sessions, presence and file-state locking
are process-local. Private backups must include both state and credentials.

`python -m unittest -v test_api test_device_link test_hosted test_wsgi` executes
47 checks locally, including the full existing website/device HTTP suite through
WSGI. Actual PythonAnywhere provisioning, certificates, reachability from the
user's network and physical Passport upload remain separate deployment checks.

Sources: [free-plan limits](https://help.pythonanywhere.com/pages/FreeAccountsFeatures/),
[manual WSGI setup](https://help.pythonanywhere.com/pages/Flask/),
[GitHub Pages limitations](https://docs.github.com/en/pages/getting-started-with-github-pages/creating-a-github-pages-site).
