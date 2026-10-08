[English](DEPLOY.md)

# 本机运行与部署

已在 Python 3.13 上测试，服务器仅使用标准库。将 `static/`、`data/`、
`device_contract.py` / `device_link.py` 和 `server.py` 一同保留；网站不需要 npm 安装或前端构建。

Windows 运行 `./Start-Website.ps1`，浏览器打开 `http://127.0.0.1:8766`。
第一次运行会在 `runtime/credentials.json` 生成随机管理员密码。请在本机读取，
不要将内容发到聊天中或打入交付包。Windows 使用目录继承的 ACL，POSIX 0600
不能代替 Windows 权限检查；应使用属于当前操作人的私有目录。POSIX 下文件为
0600，运行目录创建权限为 0700。

局域网联调时，将示例 IP 换为电脑实际的私有 IPv4 地址：

```powershell
./Start-Website.ps1 -BindAddress 192.168.1.10 -PublicOrigin http://192.168.1.10:8766
```

手机、电脑和设备需要能在同一局域网互访。配网页填写同一个服务地址。
路由器的访客隔离可能阻止互访；防火墙需按实际环境配置，本包不会自动修改。
设备仅允许 10/8、172.16/12、192.168/16 私有 IPv4 使用 HTTP；公网必须使用
HTTPS，没有跳过证书校验的开关。

Linux 服务器附带的 systemd 和 Caddy 配置是部署模板，尚未在你的服务器运行。
按服务器既有方法安装 Python、Caddy，创建专用 `cast-online` 账户，将网站复制到
`/opt/cast-online-web`，安装 `deploy/cast-online.service`。在
`/etc/cast-online.env` 设置 `CAST_DOMAIN=你的实际域名`，Caddy 服务环境也设置
相同变量，然后使用 `deploy/Caddyfile`。真实服务器仍需检查 DNS、端口、证书和
上线条件。Python 只监听本机回环地址，由 HTTPS 代理转发；公开地址必须与浏览器
访问的协议和主机完全一致，不根据转发请求头猜测地址。HTTPS 模式启用 Secure
Cookie。同一个数据目录只运行一个 Python 进程，文件锁为进程内锁。

私下备份 `state.json` 和 `credentials.json`，恢复前停止服务。Windows 和 POSIX
都会先刷新文件再替换；POSIX 还会刷新目录。应用测试不能证明硬件断电持久性。

发布会在一次状态写入中冻结名单和设备资料，草稿不会公开。每个导出 JSON 最多
8192 UTF-8 字节；每天最多 80 组，每场最多 12 组；演员名最多 24 UTF-8 字节，
通常为 8 个汉字。每个组合资料最多 24 页，每页 11 字 × 7 行。发布前检查本版
6875 字符字库，不支持的文字或超限内容会给出具体错误并保留旧发布版本。
只编辑演员库不会改变已发布资料；从旧网站版本保留下来的发布记录需重新发布，
才会生成对应设备资料。

运行 `python -m unittest -v test_api test_device_link` 可测试 API。交付报告分别记录浏览器、
固件协议联调和未执行的服务器/实机检查。本包不会远程部署、修改 DNS 或发布上线。

r4 在 Passport 直接输入服务根地址，再进行网站配对。代理需允许带设备鉴权的
`/api/device/heartbeat` POST 和 `/api/device/content` GET，并保留 Authorization 请求头。
设备心跳无需浏览器 Origin；管理员 POST 仍验证 Origin、会话与 CSRF。设备身份及每台一个
任务存入 state.json；45 秒无心跳显示离线，服务器重启后等待心跳恢复。移除设备会取消待办任务。
运行数据备份含密钥哈希，应私密保管。
