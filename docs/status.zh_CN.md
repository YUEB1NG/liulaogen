[English](status.md)

# 验证状态

固件：`cast-wifi-20261008-r5`，2026-10-08 验证。

- Build：PASS。本机原生 ESP-IDF 5.5.3，合并镜像及匹配 ELF/MAP 归档校验通过。
- Host tests：PASS。网站 56 项测试；固件应用、协议、缓存故障及 6900 字形检查通过。生产源码及仅测试修正的后续提交均完成相应检查，最新 GitHub CI 通过。
- Device tests：PARTIAL。分段烧录及独立校验通过；重启前 NVS/PHY 数据逐字节一致，未全片擦除。45 秒启动日志匹配版本和 ELF，显示、背光、按键初始化正常，Wi-Fi 连接并取得 DHCP，无崩溃、看门狗或欠压。网站检测到一台在线 r5 设备。
- 用户验收：针对显示、每行 13 字、按键与闪屏的组合检查，用户回复“显示正常，按键顺畅，没有明显闪屏”。
- Unverified：网站上传和保存回执、随后断网浏览人物简介、物理掉电持久性、长期无线/内存及真实 iPhone Safari。

[r5 行为与容量](r5-update.zh_CN.md)

[下载固件与匹配调试文件](https://github.com/YUEB1NG/liulaogen/releases/tag/v0.2.0-liulaogen-wifi-r5)

合并 BIN SHA256：
`9d5364665b194229c3a830734c0d7c11bacd02054c8baa0720b6bd176f7c6ed8`

匹配 ELF SHA256：
`2233ab9545075151365844b094e5c7134eb85fb07092cbcde11beef31d19241f`

合并 BIN 从 0x0 写入，可能重置配置。兼容分段更新保留位于 0x9000–0x1ffff 的 NVS/PHY；新增缓存分区位于 0x400000–0x7fffff。原始日志、设置读回及账号凭据仅留本机；公开调试文件保留编译器原构建路径。

## 网站

[打开在线网站](https://quyue.pythonanywhere.com)

2026-10-08 部署并验证，现有 185 位演员；原有发布记录、管理员修改、凭据与配对保留。公网静态文件与源码一致，10 个缓存文件的 SHA256 校验通过。本机手机模拟覆盖 320/390/768 px；线上登录、日期选择、演员库、设备列表、存储与到期时间显示检查通过。

免费服务当前有效期至 2026-11-07，需要在平台控制台续期，未开通付费服务或自动续期。

旧 r4 归档保留作历史下载。

[历史 r4 版本](https://github.com/YUEB1NG/liulaogen/releases/tag/v0.1.0-liulaogen-wifi-r4)
