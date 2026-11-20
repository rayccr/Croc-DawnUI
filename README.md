# Croc-DawnUI

[English](README.md) | [简体中文](README.zh-CN.md)

A compact Qt 6 / C++20 / QML desktop file-transfer UI built on [schollz/croc](https://github.com/schollz/croc), using [Croc-lib](https://github.com/rayccr/Croc-lib) as its C ABI transfer engine. Community-maintained by Ray.

![Home screen](docs/screenshots/home.png)


## Features

- File/folder selection and drag-and-drop, share codes and receive confirmation.
- Separate send/receive task columns, bounded scheduling and cancellation.
- Batch/file/verification progress, file-result details and local history.
- Interrupted-task records and reconnection, with engine-verified reusable content.
- Dark glass panels, SVG icons, Chinese/English UI and compact layouts.

## Build on Windows

Validated toolchain: Qt 6.10.1 MSVC 2022 x64, Visual Studio 2022 C++ tools, CMake 3.21+ and Ninja. Place the two repositories side by side. Build Croc-lib first.

Run from an x64 VS developer shell, adjusting the Qt path for your installation:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.10.1/msvc2022_64
cmake --build build
```

The executable and deployed Qt files are under `bin/`. To use another library location, pass `-DCROCD_ENGINE_LIB_DIR=<directory>`. The normal application uses the DLL and does not require croc.exe. Development relay tests use the upstream CLI separately.

## Linux build

See [Ubuntu build instructions](docs/UBUNTU.md). Native Linux transfer validation is pending.

## Source map

| Directory | Responsibility |
| --- | --- |
| src/app | Application entry |
| src/ui | QML views/components, native drag-and-drop, icons and shaders |
| src/logic | Application operations, queue/recovery, settings and file safety |
| src/models | Task and history models |
| src/engine | C ABI loading, sessions, event pump and JSON mapping |
| src/tests | Regression and development checks |

Open the root `CMakeLists.txt`. `CROCD_ENABLE_SELFTEST` and `CROCD_BUILD_TESTS` default to OFF. With tests enabled, `CrocRegression` covers the process compatibility backend and snapshot mapping. `CrocDllRegression`, `CrocDllLanRegression` and `CrocQueueRegression` exercise the DLL. Use isolated test directories. Details are in [TESTING.md](docs/TESTING.md).

## Status and limitations

Windows x64 is the validated platform. Local Windows testing covers file transfers, confirmation handling, cancellation, structured progress, queue scheduling and interrupted-task recovery. Cross-machine LAN discovery, poor networks, suspend/resume, power loss and hostile filesystem races still need broader validation. A synthetic all-zero same-name overwrite case can repeatedly request data and remains a known issue. Reliable pause/resume is not exposed. Task restoration reconnects peers; after restarting the app the sender shares a newly ready code and the receiver enters the current code. Partial files can have their final name and preallocated size; only the confirmed result determines completion.

The UI supports five views: home, send, receive, history and settings. Text transfer, contacts and offline storage are outside the current scope. Public relays may be used according to the selected engine configuration. Endpoint addresses are informational and do not establish identity.

## License and attribution

Project additions are licensed under [MIT](LICENSE). Retain Croc's original copyright and license, and dependency notices. Qt and other bundled components keep their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [Qt source information](docs/QT-SOURCE.md). Croc-lib records the pinned upstream commit and integration changes in [UPSTREAM.md](https://github.com/rayccr/Croc-lib/blob/main/UPSTREAM.md).

See [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md) and [CHANGELOG.md](CHANGELOG.md).
