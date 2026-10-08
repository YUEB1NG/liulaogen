[简体中文](cast-wifi.zh_CN.md)

# Cast Wi-Fi and website upload

The r5 implementation supersedes the original single-profile cache and eleven-column layout below: see [r5 behavior and limits](../../../docs/r5-update.md). It uses a 15-day immutable archive with 13-column biographies.

Version `cast-wifi-20261007-r4`, native Windows ESP-IDF 5.5.3, ESP32-C3,
8 MB Flash, no PSRAM. The accepted offline eight-city application, dataset,
charcoal/blue interface, backlight fix and navigation remain.

## Everyday controls

1. Long OK on the city screen opens updates; long UP opens network settings.
2. Select **Choose Wi-Fi**, wait for scanning, select a 2.4 GHz personal network,
   then enter its password on Passport. UP/DOWN chooses a character, OK appends;
   long UP cycles lowercase, uppercase, digits, symbols/space. Each group has
   a delete option. Long DOWN submits; long OK cancels and clears the input.
   Passwords are masked; all 95 printable ASCII characters are supported.
   An empty password selects an open network; personal-network passwords use
   8–63 characters. Hidden-network manual names use ASCII; scanned UTF-8 names
   retain their original bytes. Unsupported name glyphs receive a placeholder.
3. Wait for successful connection and saving. Failed credentials, DHCP timeout
   (30 seconds), or NVS failure restore the previous configuration. Saved networks
   reconnect at startup and retry every 15 seconds. No phone hotspot renaming,
   192.168.4.1 browser page, or travelling computer is required.
4. In **Service origin**, enter the deployed website root origin (no path/trailing
   slash). This build has no preset domain: the user has not chosen one. Public
   service requires HTTPS with normal certificate verification and network time;
   private testing also permits RFC1918 IPv4 HTTP. Wi-Fi alone does not establish
   website connectivity. A service origin may be saved before configuring Wi-Fi.
5. Select **Website pairing**. Enter its eight-character code in the phone website's
   **Upload to device** dialog. The code lasts five minutes; OK requests another code.
   Website administrator login is required. Pair once per device/service.
6. On the website choose a date, edit/save/preview/publish its lineup, then click
   **Upload to device**, choose the online Passport and confirm with **Upload to this device**.
   This sends that date's complete published lineup, not only the city currently
   visible in the editor. Unsaved drafts are excluded; publication alone does
   not enqueue delivery. Phone and Passport need Internet access, not a common
   local network, when using the public service.
7. Status progresses from waiting to transferring to saved. The device checks
   for explicitly queued jobs every ten seconds. The website declares it offline
   after 45 seconds without heartbeat. Only a matching saved ACK completes the
   job. Network loss leaves the last validated lineup available offline.

Manual date-based update remains available on Passport: short UP/DOWN adjusts
the date and OK downloads; long DOWN uses China's date after clock sync. Long OK
returns. The settings clear item asks for OK confirmation and keeps cached content.
Legacy portal and phone-preset code remains for regression compatibility, but is
not exposed by the r4 settings UI and is not the recommended setup route.

## Delivery integrity and scope

The device generates and stores a random ID and bearer secret bound to one service
origin. The website stores only the secret hash; browser login credentials never
go to Passport. Device heartbeat/content requests authenticate separately from
administrator session/CSRF checks. No credentials are logged by application code.
Raw Wi-Fi driver logs can contain SSIDs and must be sanitized before sharing.

One outstanding job per paired device freezes a published lineup. Exact date,
revision, schema, glyph, size and rollback checks precede a dual-slot NVS write,
read-back and active-slot selection. Only then is a durable ACK queued. Retry and
reboot do not turn an incomplete download into success. Failed jobs may be retried
by uploading again. Removing a device cancels its queued job and requires pairing
again. Server restart keeps pairings/jobs but resets online status until heartbeat.
Limits: 32 paired devices, 64 temporary pairing registrations, ten admin pairing
attempts per minute, 8192 bytes per lineup, 80 groups total, 12 per session and
24-byte names. This is content delivery, not firmware OTA.

Online actor profiles still download when opening a pair, through the same worker.
They must match the date/revision/ordered IDs; only the last pair is cached.
Republishing that date may make an older queued lineup's uncached profile unavailable;
the reader retains historical/pending fallback. Upload success acknowledges the
lineup only, not pre-downloading every actor profile.

## Storage and verification

NVS: offset 0x9000, size 0x16000; PHY: 0x1f000/0x1000; application:
0x20000/0x7e0000. The verified merged image starts at 0x0; its padding resets
stored settings/cache. Use only the matching ELF/MAP bundle; no full-chip erase.
The 6875-glyph font and historical data are unchanged. HTTP/TLS buffers are released
before JSON parsing and storage. The existing sync worker serializes heartbeat,
lineup and profile traffic. No audio or Bluetooth stack is added.

Run the complete `tools/validate.ps1 --all` gate and website
`python -m unittest -v test_api test_device_link`. Host checks cover 95-character
input, actual application loops, network worker fault scenarios, cache failures,
device identity/HTTP/ACK behavior, malformed payloads, glyph coverage and actual
LVGL text metrics. Radio/NVS/RTOS boundaries are simulated. Browser tests exercise
the real local website; its actual HTTP responses pass compiled firmware parsers.
Local browser test ACKs are simulated and are not hardware acceptance.

Device tests require separate flash approval: input/display/button usability,
real scan/correct/wrong password, pairing/upload/ACK, reconnect/restart/offline
recovery, power-loss durability, TLS/time behavior, and runtime heap/stack margins.
The public domain is undecided; no server deployment or public end-to-end claim
has been made. See the delivery report for binary identity and exact results.
