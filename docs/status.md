[简体中文](status.zh_CN.md)

# Verification status

Firmware: `cast-wifi-20261008-r5`, verified on 2026-10-08.

- Build: PASS. Native ESP-IDF 5.5.3; merged image and matching ELF/MAP archive verified.
- Host tests: PASS. Website 56 tests; firmware application, protocol, cache fault and 6,900-glyph checks. GitHub CI passed for production source and the test-only follow-up.
- Device tests: PARTIAL. Segmented flashing and independent verification passed. NVS/PHY bytes were unchanged before restarting; no whole-chip erase. The 45-second startup log matched version and ELF, initialized display/backlight/buttons, connected to Wi-Fi and obtained DHCP, with no panic, watchdog or brownout. The website detected one online r5 device.
- User acceptance: normal display, responsive buttons and no obvious startup flicker, reported in response to the combined display/13-character-layout check.
- Unverified: website upload/saved receipt and subsequent offline biographies, physical power-loss durability, sustained wireless/memory behavior and real iPhone Safari.

[r5 behavior and limits](r5-update.md)

[Download firmware and matching debug artifacts](https://github.com/YUEB1NG/liulaogen/releases/tag/v0.2.0-liulaogen-wifi-r5)

Merged BIN SHA256:
`9d5364665b194229c3a830734c0d7c11bacd02054c8baa0720b6bd176f7c6ed8`

Matching ELF SHA256:
`2233ab9545075151365844b094e5c7134eb85fb07092cbcde11beef31d19241f`

The merged BIN is for offset 0x0 and can reset stored settings. Compatible segmented updates preserve NVS/PHY at 0x9000 through 0x1ffff. The new archive partition occupies 0x400000 through 0x7fffff. Raw logs, settings readbacks and account credentials stay private; published debug files retain original compiler paths.

## Website

[Open the live website](https://quyue.pythonanywhere.com)

Deployed and verified on 2026-10-08. All 185 actor records are available; existing publications, administrator edits, credentials and device pairing were preserved. Public static files matched source; ten archive blobs passed SHA256 checks. Mobile emulation passed at 320/390/768 px; live login, date selection, actor library, device list and storage/expiry displays passed.

The free website currently expires on 2026-11-07; renew from the hosting dashboard. No paid service or automatic renewal is enabled.

The previous r4 artifacts remain available as historical downloads.

[Previous r4 release](https://github.com/YUEB1NG/liulaogen/releases/tag/v0.1.0-liulaogen-wifi-r4)
