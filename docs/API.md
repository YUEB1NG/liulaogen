[简体中文](API.zh_CN.md)

# Website HTTP API

See [server.py](../website/server.py) and [device_link.py](../website/device_link.py) for the implementation. These endpoints support this repository's website and firmware; independently versioned public API compatibility is not promised. Historical and administrator-published records are not confirmation of a real performance schedule.

## Run locally

From the repository root, start the server and example in separate terminals:

```sh
python website/server.py
python website/examples/read_public_api.py
```

The default origin is `http://127.0.0.1:8766`, with data in `website/runtime/`. Only the Python standard library is needed. Use `python website/server.py --help` for port, data-directory and hosting options.

The example reads health, configuration, actor counts and published dates without authentication or writes. An empty dates list is normal on a fresh installation. Add `--date 2026-10-03` to read a publication; this date illustrates the format and returns 404 if unpublished.

## Requests and authentication

Responses are JSON. Errors use `{"error":{"code":404,"message":"该日期尚未发布"}}`.

Ordinary POST requests require `Content-Type: application/json` and an `Origin` matching the configured service origin. Login establishes a `cast_session` Cookie. Later administrator writes require `X-CSRF-Token` from the login response. Sessions are held in memory and do not survive a server restart.

| Category | Requirement |
| --- | --- |
| Public reads | No session; public configuration or published content only |
| Administrator reads | Valid session Cookie |
| Administrator writes | Cookie, Origin, X-CSRF-Token |
| Device heartbeat and task content | Device-generated Bearer credential, without administrator Cookies |

Heartbeat is exempt from browser Origin validation but validates identity, Bearer and message structure. Use configured HTTPS origins in production; see [hosting](hosting.md). Keep real passwords, Cookies, CSRF values, pairing codes and device tokens out of issues, examples and screenshots.

## Public reads

| Method and path | Parameters | Response |
| --- | --- | --- |
| `GET /healthz` | None | `{"ok":true}`; HTTP responsiveness only |
| `GET /api/config` | None | Cities, sessions, site configuration |
| `GET /api/actors` | None | `schema_version`, `actors`, provenance notice |
| `GET /api/published-dates` | None | `dates`: date, publication revision, city count |
| `GET /api/lineup` | `date=YYYY-MM-DD` | Published lineup, or 404 |
| `GET /api/device/window` | Optional `date`, `revision` | Fifteen-day archive index centred on today in Shanghai time |
| `GET /api/device/blob` | SHA-256 `id` from index | Published offline content block |
| `GET /api/device/profile` | `date`, `revision`, `m0`, `m1`; optional `columns=13` | One pair's biographies at a published revision |

## Administrator endpoints

| Method and path | Parameters or JSON body | Result |
| --- | --- | --- |
| `POST /api/login` | `username`, `password` | Cookie and `csrf`; no prior session |
| `GET /api/session` | None | Username and CSRF |
| `POST /api/logout` | `{}` | End session |
| `GET /api/overview` | None | Content, device, storage counts; recent publications |
| `GET /api/storage` | None | Storage and hosting information |
| `GET /api/backup` | None | Content backup excluding pairing credentials and delivery jobs |
| `GET /api/draft` | `date` | Draft, `version`, current published `revision` |
| `GET /api/previous` | `date`, `city`, `session` | Previous lineup for copying |
| `GET /api/changes` | `date`, `city`, `session` | Previously published session members |
| `POST /api/draft` | `date`, `version`, `venues` | Save and increment draft version |
| `POST /api/preview` | `date`, `version`, `revision` | Validate and preview without publishing |
| `POST /api/publish` | Same | Freeze lineup and biographies; increment publication revision |
| `POST /api/config` | `cities`; see source `save_settings` | Save cities and performance times |
| `POST /api/actors` | Actor fields; see source `save_actor` | Create actor, HTTP 201 |
| `POST /api/actors/{id}` | Actor fields and current `version` | Update without changing frozen publications |
| `GET /api/devices` | None | Online and delivery status |
| `POST /api/devices/claim` | `code` | Claim an online device's eight-character pairing code |
| `POST /api/devices/rename` | `device`, `name` | Update label |
| `POST /api/devices/upload` | `device`, `date`, `revision` | Queue delivery, HTTP 202; not storage confirmation |
| `POST /api/devices/forget` | `device` | Remove pairing |
| `POST /api/devices/batch` | `action`, `devices`; uploads also need `date`, `revision` | Batch upload or forget; inspect each result |

Use city IDs from config and member IDs from actors. A runnable draft structure is in [test_api.py's payload](../website/test_api.py). Names are not stable IDs.

Read the draft, save using its current version, preview using the returned version/revision, publish, then explicitly upload using the publication response's revision. On 409, reload and review rather than overwriting or blindly retrying publication.

## Device delivery

`POST /api/device/heartbeat` uses `Authorization: Bearer <device-generated token>` with `id`, `code`, `ack`; r5 also sends `protocol: 2`. The device requests a pairing code and an administrator confirms through claim. Do not embed a shared token.

`GET /api/device/content?id=…&job=…` uses the same device Bearer and returns the job's frozen publication. The device must also download, verify and durably store the complete offline archive before sending `ack: {"id":"job ID","result":"saved"}` in a heartbeat.

States are `queued → transferring → saved / failed`. Only a matching saved receipt reports completed device storage. HTTP 202, online presence and finished downloads do not substitute for it. [Protocol tests](../website/test_device_link.py) cover pairing, authentication, failures and restarts.

## Errors and verification

| Status | Meaning |
| --- | --- |
| 400 / 413 / 415 | Invalid structure, size or Content-Type |
| 401 / 403 | Session, device credential, Host, Origin or CSRF failure |
| 404 | Unpublished or missing object |
| 409 | Revision conflict, offline device, expired pairing or delivery precondition |
| 422 | Invalid date, glyph, capacity or field |
| 429 | Login, pairing or session limit |
| 500 | Storage or handling failure; success is not confirmed |

Inside website:

```sh
python -m unittest -v test_api test_device_link test_hosted test_wsgi test_features
```

These exercise local HTTP and simulated devices. See [verification status](status.md) for physical uploads, power-loss durability and real-device browsers.
