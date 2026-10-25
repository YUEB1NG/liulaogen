<p align="center"><img src="docs/brand/cover.svg" width="100%" alt="liulaogen" /></p>
<p align="center"><b>简体中文</b> · <a href="README.en.md">English</a></p>
<p align="center"><a href="https://github.com/YUEB1NG/liulaogen/actions/workflows/checks.yml"><img src="https://github.com/YUEB1NG/liulaogen/actions/workflows/checks.yml/badge.svg" alt="Build checks" /></a> <a href="LICENSE"><img src="https://img.shields.io/badge/code-MIT-111827?style=flat-square" alt="MIT license" /></a> <img src="https://img.shields.io/badge/ESP--IDF-5.5.3-2563eb?style=flat-square" alt="ESP--IDF-5.5.3" /></p>

# liulaogen · 刘老根

**把舞台装进口袋 把名单留在手边**

面向 FoloToy AI Passport 的演出阵容手册：ESP32-C3 固件与手机管理网站一起开源。通过 Wi-Fi 配对、选择已发布日期、上传名单，再用保存回执确认同步。

<p><a href="https://quyue.pythonanywhere.com">打开网站</a> · <a href="https://github.com/YUEB1NG/liulaogen/releases/tag/v0.2.0-liulaogen-wifi-r5">r5 体验固件</a> · <a href="docs/hosting.zh_CN.md">部署指南</a> · <a href="docs/status.zh_CN.md">验证记录</a></p>

| 设备里的手册 | 手机上的管理台 | 明确的同步流程 |
| --- | --- | --- |
| 八城界面 历史资料 按键阅读 | 演员资料 日期选择 已发布名单 | Wi-Fi 配对 选择名单 上传 确认回执 |
| 已同步内容支持离线浏览 | Python 标准库后端 | 不把“已发送”当作“已保存” |

## 快速开始

```sh
git clone https://github.com/YUEB1NG/liulaogen.git
cd liulaogen/website
python server.py
```

浏览器打开 `http://127.0.0.1:8766`。初始登录信息保存在本机 `runtime/credentials.json`，不要提交或公开该文件。

公网部署见 [中文托管指南](docs/hosting.zh_CN.md)。在线演示使用 PythonAnywhere；GitHub Pages 无法运行此 Python 后端。免费服务需要在平台控制台续期，托管状态与最后验证日期见 [验证记录](docs/status.zh_CN.md)。

## 从网站到设备

```mermaid
flowchart LR
  A[设备连接 Wi-Fi] --> B[网站配对]
  B --> C[选择已发布日期]
  C --> D[上传到设备]
  D --> E[确认保存回执]
  E --> F[离线查看已同步内容]
```

## 固件与硬件

| 项目 | 配置 |
| --- | --- |
| 固件 | r5 体验版 |
| 芯片 | ESP32-C3 |
| 存储 | 8 MB Flash / 无 PSRAM |
| 工具链 | 原生 ESP-IDF 5.5.3 |
| 下载 | [合并 BIN 与匹配 ELF / MAP](https://github.com/YUEB1NG/liulaogen/releases/tag/v0.2.0-liulaogen-wifi-r5) |

烧录前阅读 [固件操作](firmware/docs/development/cast-wifi.zh_CN.md) 和 [构建验证](firmware/docs/development/engineering/build-and-test.zh_CN.md)。经校验的合并镜像写入 **0x0**，会重置配置与缓存；应用单独 BIN 不能替代合并镜像，不做全片擦除。

## 验证边界

按 2026-10-08 的 r5 记录，构建与主机检查通过，设备已烧录并独立校验；用户反馈显示正常、按键顺畅、无明显启动闪屏。网站上传后的保存回执、离线资料、断电耐久与真实 iPhone Safari 仍待实机验收。

[r5 更新内容](docs/r5-update.zh_CN.md) · [完整验证状态](docs/status.zh_CN.md)

<details>
<summary><b>源码结构与开发检查</b></summary>

- `firmware/`：应用、BSP、字库、测试和构建工具
- `website/`：Python 后端与手机前端
- `website/deploy/pythonanywhere_wsgi.py`：WSGI 托管模板

网站检查在 `website/` 内运行：

```sh
python -m unittest -v test_api test_device_link test_hosted test_wsgi test_features
```

固件在激活 ESP-IDF 的环境中，从 `firmware/` 执行 `tools/validate.ps1 --all`（Windows），或在支持的 POSIX 环境执行 `bash tools/validate.sh`。源码 CI 与实际硬件验收分别记录。

</details>

## 开源与来源

代码采用 [MIT](LICENSE)，Noto 字体保留 SIL OFL，原 FoloToy 版权声明与第三方来源保留，详见 [许可](docs/licenses.zh_CN.md)。本项目为社区衍生应用，不是 FoloToy 官方发行版；历史资料不代表今日演出安排。

账号、令牌、Wi-Fi 密码、私有运行数据、工具链与原始设备日志不进入仓库。

---

来自 [YUEB1NG](https://github.com/YUEB1NG) · 另一个项目 [zibian](https://github.com/YUEB1NG/zibian)
