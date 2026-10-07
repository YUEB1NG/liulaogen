[English](status.md)

# 验证状态

固件 `cast-wifi-20261007-r4`：

- Build：PASS，原生 ESP-IDF 5.5.3、合并镜像及匹配调试归档校验通过。
- Host tests：PASS，输入、状态、网络、缓存故障、协议和中文覆盖检查通过。
- Device tests：PARTIAL。烧录、回读和启动检查通过；用户于 2026-10-07 确认已连接 Wi-Fi，
  但这不等于网站端到端验收完成。
- Unverified：公网部署、真实配对/上传/保存回执、硬件掉电持久性、长期无线/堆/栈与 iPhone Safari。

已校验合并 BIN SHA256：
`d5edb113a07c0faf78999792fec8aff974899b65e53d05d7079b30c6f26a5d1d`。

匹配 ELF SHA256：
`1db21b5bc3c347f67f4b2e66b555e413d2cf3a68809d16674d78f966f01441d6`。

固件未预填网站地址。本次托管适配只改网站，不改变已烧录镜像。本机浏览器测试使用模拟
设备心跳/回执，真实 HTTP 报文通过固件 C 解析；原始本机和设备日志不公开。

网站托管适配：47 项本机测试通过，包含经 WSGI 执行的完整 HTTP 网站/设备测试。
