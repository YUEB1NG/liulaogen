[English](design-sources.md)

# 界面来源与取舍

保留现有原生 JavaScript 前端和 Python 标准库后端。统一自托管思源黑体，白色主界面、深色正文、蓝色操作。不依赖字体或脚本 CDN。

| 开源来源 | 实际用途 | 许可 |
| --- | --- | --- |
| shadcn/ui | 将按钮样式与键盘焦点规范适配到现有 CSS | MIT |
| Motion 14.6.0 | 本地弹簧入场、列表位置衔接、弹窗退出 | MIT |
| Lucide Static 1.61.0 | 本地 SVG 图标 | ISC |
| LVGL 9.5.0 | 保留现有固件绘制库，不升级版本 | MIT |

[shadcn/ui 源码](https://github.com/shadcn-ui/ui)

[Motion 源码](https://github.com/motiondivision/motion)

[Lucide 源码](https://github.com/lucide-icons/lucide)

也审阅了 Beautiful UI、BeUI、Rare UI 和 Transitions。没有把框架专用组件直接塞进原生网页；Transitions 的代码片段另有再分发限制，本次未复制。

使用技能：find-skills、black-white-ui-prototyping、web-design-guidelines、create-readme、passport-develop、passport-build。技能只作为开发指引，不打包进项目；字体以用户要求为准。

支持系统“减少动态效果”。弹窗退出 120 毫秒，按钮状态即时响应；长演员列表只交错前十项并使用延迟绘制。ESP32-C3 保留即时选择反馈，不分配整屏过渡缓冲区。

包版本、下载地址与 SHA-256 见 website/static/vendor/versions.json，许可文本随代码保留。
