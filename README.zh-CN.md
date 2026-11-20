# Croc-DawnUI

[English](README.md) | [简体中文](README.zh-CN.md)

基于 [schollz/croc](https://github.com/schollz/croc) 构建的 Qt 6 / C++20 / QML 桌面文件传输界面，通过 [Croc-lib](https://github.com/rayccr/Croc-lib) 的C ABI调用传输引擎，由Ray维护。

![首页](docs/screenshots/home.png)


## 功能

- 文件和文件夹选择、拖入、分享口令及接收确认。
- 首页收发分栏，独立调度、排队和对应任务取消。
- 整批、当前文件、校验进度分开显示，提供文件结果和本地历史。
- 保留中断记录并重新连接，由引擎检查可复用内容。
- 深色玻璃卡片、SVG图标、中文/英文界面与紧凑布局。

## Windows构建

已验证Qt 6.10.1 MSVC 2022 x64、Visual Studio 2022 C++、CMake 3.21+和Ninja。将两个仓库放在同一父目录，先构建Croc-lib。

在VS x64开发终端执行，按本机安装位置调整Qt路径。

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.10.1/msvc2022_64
cmake --build build
```

产物在bin/，构建后自动部署Qt依赖。自定义库目录可传-DCROCD_ENGINE_LIB_DIR=<目录>。正常DLL收发不需要croc.exe，开发时的本机中继测试另用上游CLI。

## Linux 构建

参见 [Ubuntu 构建说明](docs/UBUNTU.md)。Linux 双端收发仍待实机验证。

## 代码结构

| 目录 | 职责 |
| --- | --- |
| src/app | 程序启动 |
| src/ui | 页面、组件、拖放交互、图标和着色器 |
| src/logic | 业务入口、队列恢复、设置和文件保护 |
| src/models | 任务与历史模型 |
| src/engine | 库加载、DLL会话、事件和快照转换 |
| src/tests | 业务回归与开发自检 |

加载根CMakeLists.txt。开发自检和测试开关默认关闭。测试说明见[TESTING.md](docs/TESTING.md)。

## 当前状态与限制

Windows x64 已进行本机验证。本机验证覆盖文件收发、接收确认、取消、结构化进度、队列调度和中断记录恢复。跨电脑局域网发现、弱网、休眠、断电和恶意文件系统竞态仍需更多验证。全零合成文件的同名覆盖可能重复请求数据，属于已知问题。可靠暂停尚未开放。重启后发送方恢复并重新分享就绪口令，接收方输入当前有效口令，再由引擎匹配可复用内容。部分文件可能使用正式文件名和预分配大小，成功状态以最终确认结果为准。

界面包含首页、发送、接收、历史、设置五个视图。暂不提供文本传输、联系人和离线存储。所选引擎配置可能使用公共中继。对端地址只作参考，不代表身份认证。

## 许可与致谢

新增代码使用[MIT](LICENSE)。保留Croc原始版权、许可及第三方依赖声明。Qt等组件保留各自许可，详见[第三方说明](THIRD_PARTY_NOTICES.md)和[Qt源码获取说明](docs/QT-SOURCE.md)。Croc-lib的[UPSTREAM.md](https://github.com/rayccr/Croc-lib/blob/main/UPSTREAM.md)记录所基于的版本和库化改动。

参与贡献见[CONTRIBUTING.md](CONTRIBUTING.md)，安全问题见[SECURITY.md](SECURITY.md)，版本变化见[CHANGELOG.md](CHANGELOG.md)。
