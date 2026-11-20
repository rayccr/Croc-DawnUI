# 深色玻璃着色器

`glass.frag` 是材质源码，`glass.frag.qsb` 是随工程提供的 Qt 6 着色器包。默认构建只需要原有 MSVC Qt，不要求额外安装 ShaderTools。

修改 GLSL 后，使用 Qt 6 的宿主 `qsb` 重新生成随源码提交的包：

```bat
qsb --qt6 --qsbversion 64 -o src/ui/resources/shaders/glass.frag.qsb src/ui/resources/shaders/glass.frag
```

本机已有工具为 `D:\qt\QT\6.10.1\mingw_64\bin\qsb.exe`。这里只执行离线资源工具，生成的 QSB 与 C++ 编译器无关，**不要**把 MinGW Qt DLL、插件或库复制到 MSVC 应用。

也可给 CMake 传入 `-DCROCD_QSB_EXECUTABLE=完整的qsb.exe路径`，让构建目录按 GLSL 依赖自动生成资源。默认仍使用随源码的 QSB，修改后两者应同步。QSB 采用 Qt 6.4 兼容序列化格式，并包含 SPIR-V、GLSL、HLSL、MSL；当前已验证 Qt 6.10.1 / Windows Direct3D 11。其他平台仍需实际验收。

材质只采样 `GlassBackdrop.qml`，经圆角边缘偏折、九点模糊和指针反光绘制，不采样页面文字或桌面。`GlassSurface.qml` 负责参数、映射与软件绘制降级；`Theme.qml` 负责共享背景引用和默认颜色。Qt 软件场景图不运行 ShaderEffect，退回半透明底色和细描边。
