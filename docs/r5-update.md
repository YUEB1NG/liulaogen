[简体中文](r5-update.zh_CN.md)

# October 8 update: website and offline firmware

The existing visual design and three-button navigation are retained. This update
implements the requested twenty fixes; physical acceptance of the new firmware
must be recorded separately from automated checks.

| Request | Implementation |
| --- | --- |
| 1, 8 | Explicit date-selection dialog and aligned, responsive date controls |
| 2 | Thirteen Unicode characters per reader row; seven rows within 216 × 183 px |
| 3, 4 | Always confirm administrator logout; actor picker opens without search focus |
| 5 | Debounced button releases navigate immediately; long presses do not add a click; battery polling is independent of redraws |
| 6, 7 | Versioned city configuration: 1–16 cities, editable afternoon/evening HH:MM times, removal from drafts with published history retained |
| 9 | Compare the current draft with the previous calendar day's published session: additions, removals and position changes |
| 10–12 | Individual/all device selection, per-device batch results, editable remarks, confirmed individual/all removal |
| 13–16 | Search names, aliases, roles and biography keywords; readable source text; 179 source records imported without collapsing aliases or overwriting administrator edits; retired stage-feature sections |
| 17 | A complete 15-day window of published lineups and biographies, centered on the server's Shanghai date at synchronization |
| 18 | Durable actor/draft/publication history, a previous-state backup, administrator content export and storage usage |
| 19 | Hosting expiry and elapsed-period progress from deployment metadata, with its verification timestamp |
| 20 | Keep backlight off until the first LVGL frame and queued panel transfers finish |

## Data and compatibility

`website/data/actors-20261008.json` is extracted from the user-supplied October 8
Markdown. It contains 179 independent entries; two redirect-only entries are not
new people. Source uncertainty is preserved. Existing IDs and administrator-edited
text remain intact. The original private attachment and internal storage links
are not published. The font inventory includes the supplied names, credits and
biographies; emoji presentation selectors are omitted only in device exports.

New lineups use schema 2 with a bounded `cities` array and optional session `time`.
r5 reads both legacy schema 1 and schema 2. Heartbeats advertise protocol 2;
the website asks older devices to upgrade before sending new-schema lineups.
Previously published lineups remain immutable. Profile versions are retained,
including the revisions referenced by queued jobs. Legacy profile clients keep
the 11-column endpoint; r5 requests 13 columns and caches the complete archive.

The window endpoint lists 15 dated archive hashes. SHA-256-addressed immutable
blobs contain lineups and shared biography pages. Every hash, date, revision,
actor pair, glyph and page limit is verified before one NVS record activates the
complete window. Lower revisions and changed content under the same revision are
rejected. Incomplete downloads or full storage preserve the prior active window.
Presence heartbeats continue between file downloads; a saved ACK follows the
complete window commit. A missing publication is represented explicitly and is
never replaced with another day's actors.

The device stores a bounded offline window, not all website history. Dates absent
from that window remain unavailable offline. The website retains all published
dates and versions. Storage is finite: an update that cannot fit alongside the
previous window fails safely, and the website does not claim a saved receipt.

## Flash layout and controls

| Partition | Offset | Size |
| --- | --- | --- |
| NVS | 0x9000 | 0x16000 |
| PHY | 0x1f000 | 0x1000 |
| Factory application | 0x20000 | 0x3e0000 |
| Offline cache, SPIFFS | 0x400000 | 0x400000 |

The 8 MB boundary, NVS location and application start are unchanged. Cache
initialization runs in the worker after the UI can become visible. Only a
provably erased cache partition is formatted automatically; corrupt storage is
not erased. A merged BIN starts at 0x0 and can reset NVS. A compatible segmented
update preserves NVS; neither operation authorizes a full-chip erase.

Long OK on the home page opens date/update controls. UP/DOWN selects a date;
OK synchronizes when online or opens its cached lineup offline. Long DOWN selects
the network date; long UP opens Wi-Fi settings. Long OK returns. City pages retain
the existing eight-cell layout and paginate when more cities are configured.

## Validation and hosting

Run the website's `test_api`, `test_device_link`, `test_hosted`, `test_wsgi` and
`test_features` unittest modules. Run `firmware/tools/validate.ps1 --all` using
native ESP-IDF 5.5.3 on Windows, or the corresponding shell gate on POSIX.
Archive tests exercise 15 dates and biographies, deduplication, restart,
interrupted downloads, full-disk writes, origin isolation and revision rollback.
Physical button feel, startup flicker and unplugged browsing require the device.

Hosting metadata lives outside the repository, next to the private state file.
The project usage figure is measured from its data directory; the account quota
is a separate platform limit, not a claim that all remaining account storage is
available to this application. Free expiry requires renewal in the platform
dashboard; no paid service or automatic renewal is activated. See the provider's
[free plan limits](https://help.pythonanywhere.com/pages/FreeAccountsFeatures/)
