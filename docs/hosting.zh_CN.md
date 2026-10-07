[English](hosting.md)

# PythonAnywhere 免费托管

GitHub 保存完整源码，PythonAnywhere 免费账号在同一 HTTPS 地址运行网页和设备 API，
保留会话/CSRF 校验，避免 iPhone 跨站 Cookie 问题。没有配置付费资源。

目前免费计划包含一个网站/工作进程、512 MiB 存储。网站一个月到期，需要定期登录后台
延长有效期；免费容量与可用性不属于生产 SLA。需要个人账号及实际分配的 HTTPS 地址，
无需购买自定义域名。

## 配置

1. 在 https://www.pythonanywhere.com/ 注册免费账号，打开 Bash 控制台。
2. 克隆 `https://github.com/YUEB1NG/liulaogen.git` 到 `/home/YOUR_USERNAME/liulaogen`。
3. Web 页面添加网站，选 Manual configuration 与 Python 3.13；应用无第三方 Python
   依赖，不需要 pip 安装。
4. 将生成的 WSGI 配置替换成 `website/deploy/pythonanywhere_wsgi.py`，把
   `YOUR_USERNAME` 改为实际账号。核对 HTTPS 域名与 Web 页一致，其他区域应使用实际分配域名。
   私有数据在源码目录外的 `/home/YOUR_USERNAME/.liulaogen-data`。
5. Web 页面启用 Force HTTPS，然后 Reload。不要在控制台后台运行 `python server.py`，
   由平台加载 WSGI 应用。
6. 首次启动后，私下使用 Files/控制台读取 `.liulaogen-data/credentials.json`，
   使用 `admin` 和随机密码登录。不要把文件、密码或托管 API token 发到 GitHub 或聊天。
7. Passport 填写实际 HTTPS 根地址，打开网站配对；网页输入配对码，发布日期名单后明确上传。
8. 验证保存回执、断网使用和 Web 页 Reload 后名单仍在。

程序校验配置的 Host，使用 Secure/HttpOnly Cookie，管理员接口验证会话/Origin/CSRF；
设备接口使用独立密钥。`/healthz` 仅返回 `{"ok":true}`。
保持一个工作进程，因为内存会话、在线状态和文件锁都属于单进程。私密备份要包含数据与凭据。

本机执行 `python -m unittest -v test_api test_device_link test_hosted test_wsgi`，
49 项检查包含经实际 WSGI 服务执行的完整网站与设备 HTTP 测试。PythonAnywhere 的真正创建、
证书、用户网络可达性以及 Passport 实际上传仍须单独验收。

参考：[免费限制](https://help.pythonanywhere.com/pages/FreeAccountsFeatures/)、
[手动 WSGI 配置](https://help.pythonanywhere.com/pages/Flask/)、
[GitHub Pages 限制](https://docs.github.com/en/pages/getting-started-with-github-pages/creating-a-github-pages-site)。
