[English](README.md)

# AI Passport 名单网站

Passport 自己扫描 Wi-Fi，用实体按键输入密码。设备填写已部署网站服务地址，打开网站配对，
在手机网页输入配对码。选择日期，编辑、保存、预览并发布名单，再点“上传到设备”，选择
在线 Passport。发布本身不会上传。网站显示等待、传输中和设备已保存，只有收到设备持久
保存回执才确认成功；已下载名单可继续离线使用。

保留原有界面，新增设备管理弹窗。日常无需带电脑、重命名手机或打开 192.168.4.1。
公网网站为 https://quyue.pythonanywhere.com，请在 Passport 填写此根地址；
当前固件未预填服务地址。HTTPS、浏览器登录、模拟配对及重启保留已验证，
真实 Passport 上传仍待验收。参见[验证状态](../docs/status.zh_CN.md)。

参见[启动与部署](DEPLOY.zh_CN.md)。本机网页开发运行 `python server.py`，打开
`http://127.0.0.1:8766`。凭据只在本机读取。管理员管理资料与草稿，发布时校验设备字库和
容量，并冻结名单与演员资料。r5 上传时保存同步当天前后 7 天共 15 天的已发布名单，以及这些名单中全部演员简介；完整校验和持久保存后才回执成功。未发布的日期明确标记，历史版本保持冻结。
不是固件 OTA。

运行 `python -m unittest -v test_api test_device_link test_hosted test_wsgi test_features`。交付报告单列浏览器、固件解析互通
以及未完成的硬件/公网检查。原始交接说明保留在 `REFERENCE_ORIGINAL.txt` 供历史参考。
源码包不包含凭据和运行数据。

免费托管：[PythonAnywhere 说明](../docs/hosting.zh_CN.md)。

当前 r5 支持完整的 15 天名单及简介缓存，全部校验落盘后才回执成功，旧版资料按发布版本保留。新协议名单上传前要求设备升级到 r5。参见 [r5 行为与容量说明](../docs/r5-update.zh_CN.md)，替代早期按需下载单份人物资料的限制。
