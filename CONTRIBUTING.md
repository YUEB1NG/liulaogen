# 参与贡献 / Contributing

欢迎可复现的问题、文档修正与范围明确的改进。先搜索已有 Issue 和 PR；较大变更先讨论范围。凭据、Cookie、令牌、配对码与原始设备日志留在本机。

Reproducible reports, documentation fixes and focused improvements are welcome. Search existing issues and PRs and discuss larger changes first. Keep credentials, Cookies, tokens, pairing codes and raw device logs private.

## 本机检查 / Local checks

网站使用 Python 标准库，在 website 目录运行：

The website uses the Python standard library. Run inside website:

```sh
python -m unittest -v test_api test_device_link test_hosted test_wsgi test_features
```

从根目录执行 `python website/server.py` 启动网站，再在另一终端运行 `python website/examples/read_public_api.py`。见 [中文 API](docs/API.zh_CN.md) / [English API](docs/API.md)。

固件变更先读 [固件约定](firmware/AGENTS.md) 和 [构建检查](firmware/docs/development/engineering/build-and-test.zh_CN.md)。分别记录主机测试、ESP-IDF 构建、烧录和硬件验收，模拟设备不能写成实机通过。

For firmware, read the conventions and build guide. Separate host tests, ESP-IDF builds, flashing and physical acceptance.

## 提交说明 / Pull requests

说明触发条件、预期与实际行为、修改结果、实际执行的检查。保持中英文文档一致，保留 FoloToy、字体和依赖的来源与版权。文档修正不应顺带更换已认可界面或烧录设备。

Describe the trigger, expected/actual behaviour, resulting change and checks performed. Keep languages aligned and preserve upstream notices. Keep unrelated interface changes and flashing out of documentation patches.

可以从验证安装步骤、修正文档断链和补充公开 API 示例开始。

Good starting points include checking setup instructions, fixing broken links and clarifying existing API examples.
