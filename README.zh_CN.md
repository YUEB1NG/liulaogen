[English](README.md)

# liulaogen

当前固件为 r5，已烧录并独立校验。用户确认显示正常、按键顺畅、无明显开机闪屏；网站上传及离线内容仍待实机验收。

[r5 更新说明](docs/r5-update.zh_CN.md)

[固件与匹配 ELF/MAP 下载](https://github.com/YUEB1NG/liulaogen/releases/tag/v0.2.0-liulaogen-wifi-r5)

在线网站使用 PythonAnywhere 免费托管。

[打开网站](https://quyue.pythonanywhere.com)

刘老根大舞台名单手册：FoloToy AI Passport 固件与手机排班网站完整开源。

**Passport 输入 Wi-Fi 密码 → 网站配对 → 网页选已发布名单 → 上传到设备 → 确认保存回执。**
保留已认可的八城界面与历史资料，下载后的名单离线可用。

- `firmware/`：完整固件、BSP、字库、测试与构建工具。
- `website/`：Python 标准库后端及现有手机网页。
- `website/deploy/pythonanywhere_wsgi.py`：PythonAnywhere 免费托管配置模板。
- [部署说明](docs/hosting.zh_CN.md)

- [验证状态](docs/status.zh_CN.md)

- [许可](docs/licenses.zh_CN.md)

## 本机启动网站

```sh
cd website
python server.py
```

本机网站地址：

http://127.0.0.1:8766

私下读取 `runtime/credentials.json` 获取初始登录信息，
不要提交或分享该文件。公网使用见部署说明；GitHub Pages 无法运行 Python 后端。
上方网站已完成 HTTPS、登录、手机布局和模拟设备配对检查，真实设备验收见验证状态。

## 固件

使用原生 **ESP-IDF 5.5.3**，ESP32-C3 / 8 MB / 无 PSRAM。参见
[固件操作](firmware/docs/development/cast-wifi.zh_CN.md)

[构建验证](firmware/docs/development/engineering/build-and-test.zh_CN.md)

在 `firmware/` 内使用已激活的 Windows 环境运行 `tools/validate.ps1 --all`，
或在支持的 POSIX 环境运行 `bash tools/validate.sh`。
经校验合并镜像从 **0x0** 写入，会重置配置与缓存；不要把应用单独 BIN 写到 0x0，不全片擦除。

网站在 `website/` 内运行 `python -m unittest -v test_api test_device_link test_hosted test_wsgi test_features`。
GitHub Actions 执行源码检查；固件按文档中的 IDF 完整验证流程复现。
开源内容不包含账号凭据、Wi-Fi 密码、私有运行数据、本机工具链、构建产物或原始设备日志。

代码采用 MIT，Noto 字体保留 SIL OFL。这是社区衍生应用，不是 FoloToy 官方发行版；
历史名单与资料不代表今日演出安排。
