cmake_minimum_required(VERSION 3.21)
project(Croc-DawnUI LANGUAGES CXX)

# 本机 CMake 将 UTF-8 的中文 /showIncludes 前缀按系统代码页误解码，Ninja 因而漏掉头文件依赖。
# 只纠正已识别的乱码，不覆盖其他机器正常检测到的语言，也不修改调用方编译启动器。
if (MSVC AND CMAKE_GENERATOR MATCHES "Ninja" AND CMAKE_CXX_CL_SHOWINCLUDES_PREFIX MATCHES "娉ㄦ剰")
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "注意: 包含文件:  ")
endif ()
option(CROCD_ENABLE_SELFTEST "启用开发专用引擎自检入口" OFF)
option(CROCD_BUILD_TESTS "构建验收修复回归检查" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 本机 Qt 位置作默认值；调用方通过命令行传入 CMAKE_PREFIX_PATH 时以传入值为准，便于换机或换工具链。
if (NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "D:/qt/QT/6.10.1/msvc2022_64")
endif ()


find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Svg Concurrent)

qt_standard_project_setup(REQUIRES 6.5)

# 物理目录按职责划分，现有头文件名继续通过这些工程内目录查找。
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/adapter"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/dll"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/mapping"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/logic/application"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/logic/safety"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/logic/settings"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/models/history"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/models/transfer"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tests/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/native"
)


qt_add_executable(Croc-DawnUI
        src/app/main.cpp
        src/logic/safety/TransferSafety.cpp
        src/logic/safety/TransferSafety.h
        src/engine/common/CrocTypes.h
        src/engine/adapter/CrocAdapter.cpp
        src/engine/adapter/CrocAdapter.h
        src/models/transfer/TransferTaskModel.cpp
        src/models/transfer/TransferTaskModel.h
        src/models/transfer/TaskFilterModel.cpp
        src/models/transfer/TaskFilterModel.h
        src/models/history/HistoryModel.cpp
        src/models/history/HistoryModel.h
        src/logic/settings/ConfigStore.cpp
        src/logic/settings/ConfigStore.h
        src/tests/engine/EngineSelfTest.cpp
        src/tests/engine/EngineSelfTest.h
        src/engine/dll/CrocLibrary.cpp
        src/engine/dll/CrocLibrary.h
        src/engine/dll/CrocLibraryTasks.cpp
        src/engine/mapping/CrocJsonMap.cpp
        src/engine/mapping/CrocJsonMap.h
        src/engine/dll/CrocEventPump.cpp
        src/engine/dll/CrocEventPump.h
        src/engine/dll/CrocDllSession.cpp
        src/engine/dll/CrocDllSession.h)

# 界面文件清单：新增 QML 文件时在此登记；部署列表由同一清单生成，无需再单独维护。
set(FLOWDROP_QML_FILES
        src/ui/qml/Main.qml
        src/ui/qml/shared/UiIcon.qml
        src/ui/qml/shared/Theme.qml
        src/ui/qml/shared/SettingsSchema.qml
        src/ui/qml/shared/I18n.qml
        src/ui/qml/components/typography/BodyText.qml
        src/ui/qml/components/typography/Hint.qml
        src/ui/qml/components/panels/Rule.qml
        src/ui/qml/components/panels/Panel.qml
        src/ui/qml/components/glass/GlassSurface.qml
        src/ui/qml/components/glass/GlassBackdrop.qml
        src/ui/qml/components/controls/AppButton.qml
        src/ui/qml/components/panels/EntryCard.qml
        src/ui/qml/components/controls/FieldBox.qml
        src/ui/qml/components/controls/SelectField.qml
        src/ui/qml/components/transfer/Metric.qml
        src/ui/qml/components/transfer/ProgressRing.qml
        src/ui/qml/components/transfer/TaskColumn.qml
        src/ui/qml/pages/HomePage.qml
        src/ui/qml/pages/SendPage.qml
        src/ui/qml/pages/ReceivePage.qml
        src/ui/qml/pages/HistoryPage.qml
        src/ui/qml/pages/SettingsPage.qml
        src/ui/qml/components/panels/AboutPanel.qml)

# 源文件按目录组织，QML运行时仍位于FlowDrop模块根，保持组件的隐式导入。
foreach(FLOWDROP_QML_FILE IN LISTS FLOWDROP_QML_FILES)
    get_filename_component(FLOWDROP_QML_NAME "${FLOWDROP_QML_FILE}" NAME)
    set_source_files_properties("${FLOWDROP_QML_FILE}" PROPERTIES QT_RESOURCE_ALIAS "${FLOWDROP_QML_NAME}")
endforeach()

# 单例声明必须放在 qt_add_qml_module 之前，否则生成的 qmldir 不带 singleton 前缀，
# 页面里引用 Theme / SettingsSchema 只会得到 undefined。
set_source_files_properties(src/ui/qml/shared/Theme.qml src/ui/qml/shared/SettingsSchema.qml src/ui/qml/shared/I18n.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# 控制器注册为 QML 单例：放在 SOURCES 里，qmltyperegistrar 才能在编译期生成类型信息。
qt_add_qml_module(Croc-DawnUI
        URI FlowDrop
        VERSION 1.0
        SOURCES src/logic/transfer/AppControllerQueue.cpp src/logic/application/AppController.cpp src/logic/application/AppController.h src/ui/native/FileDropArea.cpp src/ui/native/FileDropArea.h
        QML_FILES ${FLOWDROP_QML_FILES})
if (CROCD_ENABLE_SELFTEST)
    target_compile_definitions(Croc-DawnUI PRIVATE CROCD_ENABLE_SELFTEST)
    target_sources(Croc-DawnUI PRIVATE src/tests/ui/SendPageCheck.cpp src/tests/ui/SendPageCheck.h)
endif ()
target_link_libraries(Croc-DawnUI PRIVATE Qt6::Concurrent Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Svg)
set_target_properties(Croc-DawnUI PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE)

# 可执行文件输出目录：默认仍为 bin；构建脚本可用 -DFLOWDROP_OUTPUT_DIR=... 隔离验证产物。
set(FLOWDROP_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin" CACHE PATH "Croc-DawnUI 可执行文件输出目录")
set(EXECUTABLE_OUTPUT_PATH ${FLOWDROP_OUTPUT_DIR})
set_target_properties(Croc-DawnUI PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FLOWDROP_OUTPUT_DIR}")


if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()

    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()

    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()

    foreach (QT_LIB Core Gui)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach (QT_LIB)
endif ()


# 收集当前界面的 QML 文件，并在 Windows 构建后部署 Qt 运行依赖。
# 部署文件由FLOWDROP_QML_FILES生成：新增 QML 文件只需登记到 FLOWDROP_QML_FILES，部署会同步。
if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(FLOWDROP_DEPLOY_QML_FILES)
    foreach(FLOWDROP_QML_FILE IN LISTS FLOWDROP_QML_FILES)
        list(APPEND FLOWDROP_DEPLOY_QML_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${FLOWDROP_QML_FILE}")
    endforeach()
    add_custom_command(TARGET Croc-DawnUI POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/deploy_qml"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    ${FLOWDROP_DEPLOY_QML_FILES} "${CMAKE_CURRENT_BINARY_DIR}/deploy_qml"
            COMMAND "${QT_INSTALL_PATH}/bin/windeployqt.exe"
                    "$<IF:$<CONFIG:Debug>,--debug,--release>"
                    --translations zh_CN
                    --qmldir "${CMAKE_CURRENT_BINARY_DIR}/deploy_qml"
                    "$<TARGET_FILE:Croc-DawnUI>"
            VERBATIM)
endif ()


# HTML 参考页对应的 SVG 图标资源。
file(GLOB CROCD_UI_ICONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/resources/icons/*.svg")
qt_add_resources(Croc-DawnUI ui_icons PREFIX "/ui" BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/resources" FILES ${CROCD_UI_ICONS})

# .qsb 是可跨编译器使用的着色器资源，不链接其他 Qt 工具链的 DLL。
# 默认使用随源码提供的预编译资源；指定宿主 qsb 时，修改 GLSL 会自动重新烘焙。
set(CROCD_QSB_EXECUTABLE "" CACHE FILEPATH "可选：Qt 6 qsb 着色器烘焙工具")
if (CROCD_QSB_EXECUTABLE)
    set(CROCD_GLASS_SHADER "${CMAKE_CURRENT_BINARY_DIR}/glass-shaders/glass.frag.qsb")
    add_custom_command(OUTPUT "${CROCD_GLASS_SHADER}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/glass-shaders"
            COMMAND "${CROCD_QSB_EXECUTABLE}" --qt6 --qsbversion 64
                    -o "${CROCD_GLASS_SHADER}" "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/resources/shaders/glass.frag"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/resources/shaders/glass.frag" VERBATIM)
else ()
    set(CROCD_GLASS_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/resources/shaders/glass.frag.qsb")
endif ()
set_source_files_properties("${CROCD_GLASS_SHADER}" PROPERTIES QT_RESOURCE_ALIAS "glass.frag.qsb")
qt_add_resources(Croc-DawnUI glass_shader PREFIX "/ui/shaders" FILES "${CROCD_GLASS_SHADER}")

# 开发回归目标不会进入正式应用，测试替身不参与真实传输。
if (CROCD_BUILD_TESTS)
    enable_testing()
    qt_add_executable(FakeCroc src/tests/regression/FakeCroc.cpp)
    target_link_libraries(FakeCroc PRIVATE Qt6::Core)
    qt_add_executable(CrocRegression src/tests/regression/RegressionTests.cpp src/logic/transfer/AppControllerQueue.cpp src/logic/application/AppController.cpp src/logic/application/AppController.h
            src/engine/adapter/CrocAdapter.cpp src/engine/adapter/CrocAdapter.h src/engine/common/CrocTypes.h src/models/transfer/TransferTaskModel.cpp src/models/transfer/TransferTaskModel.h
            src/models/transfer/TaskFilterModel.cpp src/models/transfer/TaskFilterModel.h src/logic/settings/ConfigStore.cpp src/logic/settings/ConfigStore.h
            src/models/history/HistoryModel.cpp src/models/history/HistoryModel.h src/logic/safety/TransferSafety.cpp src/logic/safety/TransferSafety.h src/tests/engine/EngineSelfTest.cpp src/tests/engine/EngineSelfTest.h src/engine/dll/CrocLibrary.cpp src/engine/dll/CrocLibrary.h src/engine/dll/CrocLibraryTasks.cpp src/engine/mapping/CrocJsonMap.cpp src/engine/dll/CrocEventPump.cpp src/engine/dll/CrocDllSession.cpp)
    target_link_libraries(CrocRegression PRIVATE Qt6::Concurrent Qt6::Core Qt6::Gui Qt6::Qml)
    set_target_properties(FakeCroc CrocRegression PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FLOWDROP_OUTPUT_DIR}")
    add_dependencies(CrocRegression FakeCroc)
    add_test(NAME acceptance-regressions COMMAND CrocRegression)
    qt_add_executable(CrocQueueRegression src/tests/transfer/QueueTests.cpp src/logic/transfer/AppControllerQueue.cpp src/logic/application/AppController.cpp src/logic/application/AppController.h
            src/engine/adapter/CrocAdapter.cpp src/engine/adapter/CrocAdapter.h src/engine/common/CrocTypes.h src/models/transfer/TransferTaskModel.cpp src/models/transfer/TransferTaskModel.h
            src/models/transfer/TaskFilterModel.cpp src/models/transfer/TaskFilterModel.h src/logic/settings/ConfigStore.cpp src/logic/settings/ConfigStore.h
            src/models/history/HistoryModel.cpp src/models/history/HistoryModel.h src/logic/safety/TransferSafety.cpp src/logic/safety/TransferSafety.h
            src/engine/dll/CrocLibrary.cpp src/engine/dll/CrocLibrary.h src/engine/dll/CrocLibraryTasks.cpp src/engine/mapping/CrocJsonMap.cpp src/engine/dll/CrocEventPump.cpp src/engine/dll/CrocDllSession.cpp)
    target_link_libraries(CrocQueueRegression PRIVATE Qt6::Concurrent Qt6::Core Qt6::Gui Qt6::Qml Qt6::Network)
    set_target_properties(CrocQueueRegression PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FLOWDROP_OUTPUT_DIR}")
endif ()

# Croc Go 引擎动态库：构建后复制到输出目录。目录可用 -DCROCD_ENGINE_LIB_DIR 覆盖。
# Windows 使用 croc_engine.dll；Linux/Android 由对应平台的构建提供 libcroc_engine.so。
set(CROCD_ENGINE_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Croc-lib/dist/windows-x64" CACHE PATH "Croc 引擎动态库目录")
if (EXISTS "${CROCD_ENGINE_LIB_DIR}/croc_engine.dll")
    add_custom_command(TARGET Croc-DawnUI POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CROCD_ENGINE_LIB_DIR}/croc_engine.dll" "$<TARGET_FILE_DIR:Croc-DawnUI>")
endif ()

if (CROCD_BUILD_TESTS)
    qt_add_executable(CrocDllRegression src/tests/engine/DllSessionTests.cpp src/engine/dll/CrocEventPump.cpp src/engine/dll/CrocDllSession.cpp src/engine/dll/CrocLibrary.cpp src/engine/dll/CrocLibraryTasks.cpp src/engine/mapping/CrocJsonMap.cpp src/logic/safety/TransferSafety.cpp)
    target_link_libraries(CrocDllRegression PRIVATE Qt6::Concurrent Qt6::Core Qt6::Gui Qt6::Network)
    qt_add_executable(CrocDllLanRegression src/tests/engine/DllLanTests.cpp src/engine/dll/CrocDllSession.cpp src/engine/dll/CrocDllSession.h
        src/engine/dll/CrocLibrary.cpp src/engine/dll/CrocLibraryTasks.cpp src/engine/mapping/CrocJsonMap.cpp src/engine/dll/CrocEventPump.cpp src/engine/dll/CrocEventPump.h src/logic/safety/TransferSafety.cpp)
    target_link_libraries(CrocDllLanRegression PRIVATE Qt6::Concurrent Qt6::Core Qt6::Gui Qt6::Network)
    set_target_properties(CrocDllLanRegression PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FLOWDROP_OUTPUT_DIR}")
    set_target_properties(CrocDllRegression PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FLOWDROP_OUTPUT_DIR}")
endif ()
