# Qt source and licenses

The Windows preview dynamically links unmodified Qt 6.10.1 libraries. The package includes license texts and module SBOM/third-party attribution under licenses/Qt/. Users may replace the Qt libraries with compatible modified builds, subject to ABI compatibility.

The complete corresponding upstream Qt source is available from the Qt Project:

- https://download.qt.io/archive/qt/6.10/6.10.1/single/qt-everywhere-src-6.10.1.tar.xz
- https://download.qt.io/archive/qt/6.10/6.10.1/submodules/

Qt modules used by this package come from qtbase, qtdeclarative, qtsvg, qttools deployment components, and qttranslations. Build instructions are included in the Qt source. Application build commands are in README.md. Report broken source links through this repository so equivalent source access can be restored. No Qt source modifications are applied by this project.

Windows预览版动态链接未修改的Qt 6.10.1，包内附许可文本、模块SBOM和第三方声明。完整对应源码可通过上述Qt官方地址取得。用户可使用兼容的修改版Qt动态库运行应用。

The exact five unmodified module source archives used for this Qt SDK are also attached to the [v0.1.0-preview.1 release](https://github.com/rayccr/Croc-DawnUI/releases/tag/v0.1.0-preview.1), at no charge alongside the binary package. Archive origins, sizes and SHA256 values are recorded in [SOURCE_ARCHIVES.json](../licenses/Qt/SOURCE_ARCHIVES.json). This is additional source access, independent of the upstream download server. Qt's library license option used here is LGPL-3.0; the GPL text and third-party terms are included as required. Qt tools with the Qt GPL exception are build-time tools, not application runtime components.

Release packages omit optional SDK copies of D3D compiler and Mesa software OpenGL DLLs; the standard Windows graphics components are used. The official Microsoft VC runtime installer is included unchanged. No application license restricts replacement of the LGPL libraries or reverse engineering for debugging such modifications. Close the app, replace compatible Qt DLLs/QML plugins in the extracted folder, then restart; no signature lock is imposed.

同一Release额外提供五个对应版本Qt模块源码包及SHA256，和程序包一样可免费下载。应用使用Qt的LGPL-3.0许可选项；依赖动态库可替换，没有签名锁。可关闭程序后替换兼容的Qt库和QML插件，再重新启动。
