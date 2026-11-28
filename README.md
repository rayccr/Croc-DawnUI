# Croc-DawnUI

简体中文 · [English](README.en.md)

Croc-DawnUI 是一款用口令在两台设备之间传文件的桌面应用。把文件或文件夹拖进窗口，将生成的口令发给对方，对方就能选择保存位置并接收。文件传输由 [Croc-lib](https://github.com/rayccr/Croc-lib) 提供端到端加密。

![Croc-DawnUI 首页的发送和接收任务列表](docs/screenshots/home.png)

## 第一次传文件

1. **发送方**打开「发送」，选择或拖入文件、文件夹，点击「开始发送」。等口令就绪后，复制给接收方。
2. **接收方**打开「接收」，输入口令并选择保存位置，点击「开始接收」，按提示确认文件。
3. 双方都可以在首页查看进度。传输结束后，可以在「历史」里查看结果。

## 从源码运行

**Windows x64** 下，将本仓库与 [Croc-lib](https://github.com/rayccr/Croc-lib) 放在同一父目录，先构建 Croc-lib。然后用 Qt Creator 打开本仓库的 [CMakeLists.txt](CMakeLists.txt)，选择 Qt 6.10.1 的 MSVC 2022 x64 套件，以 Release 模式构建并运行 `Croc-DawnUI`。

**Ubuntu** 用户请按 [Ubuntu 构建说明](docs/UBUNTU.md) 编译和启动桌面端。

## 项目结构

| 目录 | 内容 |
| --- | --- |
| [src/ui/](src/ui/) | 页面、组件和图标 |
| [src/logic/](src/logic/) | 收发队列、设置与文件处理 |
| [src/engine/](src/engine/) | 加载 Croc-lib，获取传输进度和结果 |

[MIT 许可证](LICENSE) · [第三方组件](THIRD_PARTY_NOTICES.md)
