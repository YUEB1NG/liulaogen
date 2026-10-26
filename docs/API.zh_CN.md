[English](API.md)

# 网站 HTTP API

接口实现见 [server.py](../website/server.py) 和 [device_link.py](../website/device_link.py)，随本仓库维护，暂不承诺独立版本的公共 API 兼容性。历史资料和管理员发布的名单不代表真实当日演出安排。

## 本机运行与示例

从仓库根目录执行，服务和示例分别在两个终端运行：

```sh
python website/server.py
python website/examples/read_public_api.py
```

默认来源为 `http://127.0.0.1:8766`，运行数据在 `website/runtime/`，只需 Python 标准库。运行 `python website/server.py --help` 查看端口、数据目录与托管参数。

示例仅读取健康状态、城市配置、演员数量和已发布日期，不登录、不修改内容。新安装返回空的发布日期列表是正常结果。可加 `--date 2026-10-03` 读取指定日期；这是格式示例，未发布时返回 404。

## 请求与鉴权

响应为 JSON。错误格式为 `{"error":{"code":404,"message":"该日期尚未发布"}}`。

普通 POST 使用 `Content-Type: application/json` 和与服务来源一致的 `Origin`。管理员登录后使用 `cast_session` Cookie，后续写操作还需登录响应中的 `X-CSRF-Token`。会话在内存中，重启服务后需重新登录。

| 类别 | 要求 |
| --- | --- |
| 公开读取 | 无需登录，仅读取公开配置或已发布内容 |
| 管理员读取 | 有效的会话 Cookie |
| 管理员写入 | Cookie、Origin、X-CSRF-Token |
| 设备心跳与任务内容 | 设备本机生成的 Bearer 凭据，不用管理员 Cookie |

设备心跳免浏览器 Origin 校验，仍校验标识、Bearer 和消息结构。生产托管使用正确配置的 HTTPS 来源，见 [托管指南](hosting.zh_CN.md)。真实密码、Cookie、CSRF、配对码和设备令牌不应出现在 Issue、示例或截图中。

## 公开读取

| 方法与路径 | 参数 | 内容 |
| --- | --- | --- |
| `GET /healthz` | 无 | `{"ok":true}`，仅证明 HTTP 可响应 |
| `GET /api/config` | 无 | 城市、场次与站点配置 |
| `GET /api/actors` | 无 | `schema_version`、`actors` 与资料说明 |
| `GET /api/published-dates` | 无 | `dates`，含日期、发布修订号、城市数量 |
| `GET /api/lineup` | `date=YYYY-MM-DD` | 已发布名单，未发布返回 404 |
| `GET /api/device/window` | 可选 `date`、`revision` | 上海时区当天前后各 7 天的资料索引 |
| `GET /api/device/blob` | 索引中的 SHA-256 `id` | 已发布离线资料块 |
| `GET /api/device/profile` | `date`、`revision`、`m0`、`m1`，可选 `columns=13` | 指定发布版本的一组演员简介 |

## 管理员接口

| 方法与路径 | 参数或 JSON 正文 | 结果 |
| --- | --- | --- |
| `POST /api/login` | `username`、`password` | Cookie 与 `csrf`，不需已有会话 |
| `GET /api/session` | 无 | 用户名与 CSRF |
| `POST /api/logout` | `{}` | 结束会话 |
| `GET /api/overview` | 无 | 内容、设备、存储统计与近期发布 |
| `GET /api/storage` | 无 | 存储与托管信息 |
| `GET /api/backup` | 无 | 内容备份，不含配对凭据与上传任务 |
| `GET /api/draft` | `date` | 草稿、`version` 与当前发布 `revision` |
| `GET /api/previous` | `date`、`city`、`session` | 可复制的此前阵容 |
| `GET /api/changes` | `date`、`city`、`session` | 此城市场次的此前发布成员 |
| `POST /api/draft` | `date`、`version`、`venues` | 保存并递增草稿版本 |
| `POST /api/preview` | `date`、`version`、`revision` | 校验并预览，不发布 |
| `POST /api/publish` | 同上 | 冻结名单和简介，递增发布修订号 |
| `POST /api/config` | `cities`，见源码 `save_settings` | 保存城市和场次时间 |
| `POST /api/actors` | 演员字段，见源码 `save_actor` | 创建演员，201 |
| `POST /api/actors/{id}` | 演员字段与当前 `version` | 更新资料，不改变历史发布 |
| `GET /api/devices` | 无 | 在线状态与上传任务状态 |
| `POST /api/devices/claim` | `code` | 认领在线设备的八位配对码 |
| `POST /api/devices/rename` | `device`、`name` | 修改设备备注 |
| `POST /api/devices/upload` | `device`、`date`、`revision` | 建立任务，202，不等于已保存 |
| `POST /api/devices/forget` | `device` | 解除配对 |
| `POST /api/devices/batch` | `action`、`devices`；上传还需 `date`、`revision` | 批量 upload 或 forget，逐项核对结果 |

城市 ID 来自 config，成员 ID 来自 actors。最小可运行草稿结构见 [test_api.py 的 payload](../website/test_api.py)，演员名称不是稳定 ID。

顺序为读取草稿 → 使用当前 version 保存 → 使用保存响应的 version/revision 预览 → 发布 → 使用发布响应的 revision 明确上传。409 时重新读取并核对，不盲目覆盖或自动重试发布。

## 设备收发与回执

`POST /api/device/heartbeat` 使用 `Authorization: Bearer <设备本机生成的令牌>`，正文为 `id`、`code`、`ack`，r5 另传 `protocol: 2`。设备申请配对码，管理员通过 claim 确认；不预置共用令牌。

`GET /api/device/content?id=…&job=…` 使用相同设备 Bearer，返回任务绑定的已发布名单。设备还需下载完整离线索引与资料、校验、落盘后，通过心跳回传 `ack: {"id":"任务ID","result":"saved"}`。

状态为 `queued → transferring → saved / failed`。只有匹配任务的 saved 回执表示设备报告保存完成，202、设备在线与下载结束均不能替代它。[协议测试](../website/test_device_link.py) 覆盖配对、鉴权、失败与重启。

## 错误与验证

| 状态 | 含义 |
| --- | --- |
| 400 / 413 / 415 | 请求结构、大小或 Content-Type 不正确 |
| 401 / 403 | 登录、设备凭据、Host、Origin 或 CSRF 校验失败 |
| 404 | 未发布或不存在 |
| 409 | 版本冲突、离线、配对过期或上传条件不满足 |
| 422 | 日期、字形、容量或字段校验失败 |
| 429 | 登录、配对或会话限制 |
| 500 | 存储或处理失败，不能确认成功 |

在 website 目录运行：

```sh
python -m unittest -v test_api test_device_link test_hosted test_wsgi test_features
```

以上为本机 HTTP 与模拟设备检查；真实上传、断电耐久与真机浏览器状态见 [验证记录](status.zh_CN.md)。
