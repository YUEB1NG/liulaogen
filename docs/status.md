[简体中文](status.zh_CN.md)

# Verification status

Current r5 source implements the October 8 fixes; see [r5 behavior and limits](r5-update.md). Local website: 56 tests passed. Firmware static gate and 6,900-glyph coverage passed. Mobile browser emulation passed at 320/390/768 px. Final build and deployment evidence is delivered with the r5 package. r5 hardware tests, real iPhone Safari, and physical power-cycle acceptance are NOT RUN. The r4 evidence below is historical.

Firmware `cast-wifi-20261007-r4`:

- Build: PASS, native ESP-IDF 5.5.3 and merged-image/debug-bundle verification.
- Host tests: PASS, input, state, network, cache faults, protocols and Chinese glyphs.
- Device tests: PARTIAL. Flash/readback/startup passed; the user reported Wi-Fi
  connected on 2026-10-07. This does not establish website end-to-end acceptance.
- Unverified: real pairing/upload/saved receipt, persistence
  under hardware power loss, sustained RF/heap/stack behavior and iPhone Safari.

Verified merged BIN SHA256:
`d5edb113a07c0faf78999792fec8aff974899b65e53d05d7079b30c6f26a5d1d`.

Matching ELF SHA256:
`1db21b5bc3c347f67f4b2e66b555e413d2cf3a68809d16674d78f966f01441d6`.

The shipped firmware has no preset website origin. Hosting integration is an
additional website-only change; it does not change the already-flashed binary.
Local browser tests simulated device heartbeats/ACKs and passed actual HTTP
responses through the firmware C parser. Raw machine/device logs stay private.

Website hosting adaptation: 49 tests passed locally and in GitHub Actions, including
full HTTP API/device tests through WSGI and uWSGI content-header alias regressions.

Public service: https://quyue.pythonanywhere.com (verified 2026-10-07).
HTTPS, exact source asset delivery, private-file denial, session/CSRF/origin checks,
simulated device pairing/online status and pairing persistence across worker reload
passed. The temporary simulated device was removed; no performance lineup was
published or modified. Mobile browser checks passed at 390 x 844 in Edge Chromium
emulation, including login, actor library, device dialog and zero JavaScript errors.
This is not an iPhone Safari or physical Passport upload test.

The free website currently expires on 2026-11-07; renew it from the hosting dashboard.
Runtime data and administrator credentials remain outside the public source tree.
