[简体中文](r6-interface.zh_CN.md)

# r6 interface review

This is a locally verified candidate, not a deployed release. r5 remains the last
user-accepted hardware version. No r6 flash, GitHub push or hosted deployment has occurred.

- Unified light interface and self-hosted fonts, local Lucide icons and reduced-motion support.
- Previous/next/today navigation, published dates, frozen published actor introductions and a phone-first help guide.
- Administrator overview with actual content counts, delivery status and recent publications.
- Static assets no longer hold the business-state lock; unchanged device polling preserves controls and focus.
- Firmware keeps the eight-city layout and 13-column biography, changes the palette and skips unchanged label writes.

The backend has 62 passing tests. Browser checks cover 320/390/768/1280 px, date alignment,
search without autofocus, alias search, city/time editing, device selection/rename/deletion,
publication isolation, failure messages, overview, public introductions and reduced motion.
Browser checks use Edge touch emulation; a physical iPhone and r6 hardware are not tested.

Firmware gates: native ESP-IDF 5.5.3, host application tests, Chinese glyph coverage,
LVGL runtime glyph checks, merged-image verification and matching ELF/MAP archive.
The static gate has 21 documented platform-dependent skips; it does not establish live radio readiness.

The merged binary is flashed at 0x0 and resets configuration/cache in its included partitions.
Keep the matching debug archive. New flashing requires explicit approval.

## Reproduce the browser check

Run a local server on port 8775 with a **new isolated data directory**. Install Playwright
in a project-local tooling directory and set NODE_PATH accordingly. Set
LIULAOGEN_TEST_DATA to that isolated directory, LIULAOGEN_TEST_OUTPUT to a report directory,
and optionally LIULAOGEN_BROWSER to a Chromium browser executable. Then run
`node website/tests/browser.cjs`. The test changes only its localhost fixture data.

Overview screenshot (test data)

![Overview](images/r6-overview-desktop.png)
