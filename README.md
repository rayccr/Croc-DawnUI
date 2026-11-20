# Croc-DawnUI 0.11 深色玻璃界面版

加载本目录 CMakeLists.txt，使用 Qt 6.10.1 / MSVC 2022 x64。当前已接入 Croc 11.6.0 真实收发、历史与设置；详细最新状态请读 D:/Croc-DawnUI/FlowDrop_项目上手与开发指南.md 和 Croc-DawnUI_验收修复记录_20261101.md。

破晓图标在 src/ui/resources/icons/dawn-logo.svg；关于布局在 src/ui/qml/components/panels/AboutPanel.qml。开发测试选项 CROCD_ENABLE_SELFTEST / CROCD_BUILD_TESTS 默认关闭。未完成的严格代理/指定路径/时间戳/托盘等不伪装可用。

## 最新交接入口（2026-11-05 / S020）

1. 项目根目录 `D:/Croc-DawnUI/AGENTS.md`：协作与每轮持续更新规则。
2. `D:/Croc-DawnUI/FlowDrop_项目上手与开发指南.md`：项目概况、架构、文件职责、构建及需求约束。
3. `D:/Croc-DawnUI/Croc-DawnUI_阶段进度与持续记录.md`：当前基线、38项功能状态、T01–T10待办、验证边界及变更记录。

当前DLL优先，已接入接收/覆盖确认、文件保护与实际结果、分开的整批/文件/校验进度、纯DLL本机局域网和独立队列恢复。应用层任务调度与tasks.json在 `src/logic/transfer/AppControllerQueue.cpp`，窗口仍保留五页、深色玻璃与首页收发分栏。恢复发送需重新分享就绪口令，接收需输入有效口令；重启不会自动联网。跨机LAN、实际弱网/休眠、可靠暂停、严格代理、托盘通知及跨平台发行仍待验收或实现。

调色加载 `src/ui/qml/shared/Theme.qml`，材质加载 `src/ui/qml/components/glass/GlassSurface.qml` / `src/ui/qml/components/glass/GlassBackdrop.qml`，GLSL与QSB同步方式见 `src/ui/resources/shaders/README.md`。新增QML只需维护当前 `FLOWDROP_QML_FILES`，部署由同一清单生成；下文旧记录的双列表说明已过时。

每轮结束更新台账；稳定说明变化时同步指南。以下内容按现行说明和历史记录分别阅读。


## 源码目录导航（2026-11-05 / S020）

```text
src/
├─ app/                         程序入口 main.cpp
├─ ui/
│  ├─ qml/                      Main.qml
│  │  ├─ pages/                 首页、发送、接收、历史、设置
│  │  ├─ components/            controls、typography、panels、transfer、glass
│  │  └─ shared/                Theme、I18n、SettingsSchema、UiIcon
│  ├─ native/                   FileDropArea
│  └─ resources/                icons、shaders
├─ logic/                       application、transfer、settings、safety
├─ models/                      transfer、history
├─ engine/                      common、adapter、dll、mapping
└─ tests/                       regression、engine、transfer、ui
```

当前Qt工程的源码和资源都从src进入。程序入口为src/app/main.cpp，界面宿主为src/ui/qml/Main.qml，业务入口为src/logic/application/AppController.h/.cpp，队列与恢复在src/logic/transfer/AppControllerQueue.cpp，DLL会话在src/engine/dll/CrocDllSession.h/.cpp。测试入口集中在src/tests。

工程根CMakeLists.txt负责构建，bin存放程序与运行依赖。Croc-lib是PROJECT下独立的引擎工程，继续由其构建脚本生成DLL。

新文件按职责加入对应目录并登记CMake清单。C++头文件由工程内include目录查找。QML磁盘目录按职责划分，通过QT_RESOURCE_ALIAS继续映射到FlowDrop模块根，因此Main入口仍为qrc:/qt/qml/FlowDrop/Main.qml。图标保持qrc:/ui/icons/，QSB保持qrc:/ui/shaders/glass.frag.qsb。新增QML文件名须在模块内唯一，部署文件由同一QML清单生成。

修改材质时，阅读src/ui/resources/shaders/README.md，同步GLSL与QSB。旧变更记录按当时的路径保留，查询当前路径以本节为准。

以下保留早期工程记录，以本节及最新指南为准。

# Croc-DawnUI Qt 工程

这是 Qt 6 / C++ / QML 的初始工程，包含五个界面入口和未接入引擎时的空状态。使用 Qt Creator 或 CLion 打开本目录的 `CMakeLists.txt`，选择 Qt 6.10.1 MSVC 2022 64 位工具链后构建 `Croc-DawnUI` 目标。

界面参考 `D:/Croc-DawnUI/FlowDrop_UI_Reference_v0.4.html`。当前未接入 Croc；文件传输、历史、设置持久化和续传均尚未实现。不要把界面空状态当作已完成的业务功能。

## Qt 运行依赖部署
Windows 构建后会调用 Qt 6.10.1 的 windeployqt，将 Qt 库、插件及 QML 模块部署到 bin。新增 QML 文件时，请同时更新 CMakeLists.txt 中的 QML_FILES 与 FLOWDROP_DEPLOY_QML_FILES 列表。Debug 与 Release 共用 bin 时不要混合分发；独立发布还需确认目标机器的官方 MSVC 运行库。

## HTML v0.4 界面重绘（2026-11-01）
Main.qml 已按 HTML 重绘五个页面；UiIcon.qml 与 icons 目录提供 SVG 图标。首页包含两行独立滚动列表、圆环和连接详情；三个子页面无当前任务卡。默认开启明确标注的示例预览，可在顶部关闭。文件选择/文件夹与拖放已接入本地选择入口；发送接收仍禁用，Croc 引擎未接入。历史数据为示例，设置只保留于当前会话。旧 Main.qml 和 CMakeLists.txt 保存在 .ui-backups 中。
