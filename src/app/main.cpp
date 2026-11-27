/**
****************************************************************
* @file    main.cpp
* @brief   FlowDrop 桌面程序入口
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
****************************************************************
*/
#include <QGuiApplication>  // 图形应用入口
#include <QQmlApplicationEngine>  // QML 加载引擎
#include <QQmlContext>  // QML 上下文
#include <QDir>  // 路径清理
#include <QStringList>  // 命令行参数
#include <QIcon>
#include <QQuickWindow>
#include <QScreen>
#ifdef CROCD_ENABLE_SELFTEST
#include <QTimer>
#include <QFontDatabase>
#include "SendPageCheck.h"

#endif
#include <QUrl>  // QML 资源地址

#include "EngineSelfTest.h"  // 引擎联调自检

/**
****************************************************************
* @brief 启动图形程序并加载 QML 主界面
* @param [IN] _nArgc 命令行参数数量
* @param [IN] _ppArgv 命令行参数数组
* @return 程序退出代码；界面加载失败时返回 1
* @others 无
* @author Ray
****************************************************************
*/
int main(int _nArgc, char* _ppArgv[])
{
    QGuiApplication appApplication(_nArgc, _ppArgv);  // 创建图形应用
    appApplication.setWindowIcon(QIcon(QStringLiteral(":/ui/icons/dawn-logo.svg")));

    // 引擎联调自检：不加载界面，用两个真实进程跑一次收发后退出。
    const QStringList lstArguments = appApplication.arguments();
#ifdef CROCD_ENABLE_SELFTEST
    if (lstArguments.contains(QStringLiteral("--ui-snapshot")))
    {
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/segoeui.ttf"));
    }
#endif
    if (lstArguments.contains(QStringLiteral("--engine-api-check")))
    {
        return RunEngineApiCheck();  // 只验证动态库加载与初始化
    }
    if (lstArguments.contains(QStringLiteral("--engine-lib-only-check")))
    {
        return RunEngineLibOnlyCheck();  // 无 croc.exe 的库模式检查
    }
    if (lstArguments.contains(QStringLiteral("--engine-dll-local-check")))
    {
        return RunEngineDllLocalCheck();
    }
    if (lstArguments.contains(QStringLiteral("--engine-api-task-check")))
    {
        const QString strRelayCroc = QDir::cleanPath(QCoreApplication::applicationDirPath()
                                      + QStringLiteral("/../../../engine/croc.exe"));
        return RunEngineApiTaskCheck(strRelayCroc);
    }
    if (lstArguments.contains(QStringLiteral("--engine-dll-transfer")))
    {
        const QString strRelayCroc = QDir::cleanPath(QCoreApplication::applicationDirPath()
                                      + QStringLiteral("/../../../engine/croc.exe"));
        return RunEngineDllTransferCheck(strRelayCroc);  // 任务级 ABI 自检（需本机 croc.exe 起中继）
    }
    const int nSelfTestIndex = lstArguments.indexOf(QStringLiteral("--engine-selftest"));
    if (nSelfTestIndex >= 0)
    {
#ifndef CROCD_ENABLE_SELFTEST
        qCritical("正式构建未启用自检入口。请使用专用开发构建。");
        return 2;
#else
        QString strWorkDir = QDir::tempPath() + QStringLiteral("/Croc-DawnUI-selftest-") + QString::number(QCoreApplication::applicationPid());  // 默认工作目录必须全新
        if (nSelfTestIndex + 1 < lstArguments.size())
        {
            strWorkDir = lstArguments.at(nSelfTestIndex + 1);  // 允许命令行指定
        }
        const QString strCrocPath = qEnvironmentVariable("FLOWDROP_CROC_PATH", QCoreApplication::applicationDirPath() + QStringLiteral("/../../../engine/croc.exe"));
        return RunEngineSelfTest(QDir::cleanPath(strCrocPath), QDir::cleanPath(strWorkDir), lstArguments.contains(QStringLiteral("--single-long-file")), lstArguments.contains(QStringLiteral("--send-only-timeout")));
#endif
    }

    // 控制器由 QML 侧作为单例创建（见 Main.qml 的 AppController），这里不再持有实例。
    QQmlApplicationEngine qmlEngine;  // 创建 QML 加载引擎
    const QUrl urlMain(QStringLiteral("qrc:/qt/qml/FlowDrop/Main.qml"));  // 设置主界面资源
    qmlEngine.load(urlMain);  // 加载主界面
    if (qmlEngine.rootObjects().isEmpty())  // 检查主界面是否加载成功
    {
        return 1;  // 加载失败
    }
    // QScreen 返回逻辑像素，Qt 已处理系统缩放；不再额外乘设备像素比。
    // 预留标题栏、窗口边框和边缘空间，低分辨率下同步降低最小尺寸。
    QQuickWindow* pMainWindow = qobject_cast<QQuickWindow*>(qmlEngine.rootObjects().first());
    if (nullptr != pMainWindow)
    {
#ifdef Q_OS_ANDROID
        pMainWindow->showMaximized();
#else
        QScreen* pScreen = pMainWindow->screen();
        if (nullptr == pScreen) { pScreen = QGuiApplication::primaryScreen(); }
        if (nullptr != pScreen)
        {
            const QRect rectAvailable = pScreen->availableGeometry();
            const int nUsableWidth = qMax(1, rectAvailable.width() - 48);
            const int nUsableHeight = qMax(1, rectAvailable.height() - 64);
            pMainWindow->setMinimumWidth(qMin(360, nUsableWidth));
            pMainWindow->setMinimumHeight(qMin(480, nUsableHeight));
            // pMainWindow->resize(qMin(1120, nUsableWidth), qMin(760, nUsableHeight));
            pMainWindow->resize(qMin(pMainWindow->width(), nUsableWidth),
                                qMin(pMainWindow->height(), nUsableHeight));
            pMainWindow->setPosition(rectAvailable.x() + (rectAvailable.width() - pMainWindow->width()) / 2,
                                     rectAvailable.y() + (rectAvailable.height() - pMainWindow->height()) / 2);
        }
        pMainWindow->show();
#endif
    }
#ifdef CROCD_ENABLE_SELFTEST
    const int nSendPageCheck = lstArguments.indexOf(QStringLiteral("--send-page-check"));
    if (nSendPageCheck >= 0 && nSendPageCheck + 2 < lstArguments.size())
    {
        StartSendPageCheck(pMainWindow, lstArguments.at(nSendPageCheck + 1), lstArguments.at(nSendPageCheck + 2));
    }
    // 仅开发构建提供原生 QML 渲染快照，用于核对桌面与窄屏；正式构建没有此入口。
    const int nSnapshot = lstArguments.indexOf(QStringLiteral("--ui-snapshot"));
    if (nSnapshot >= 0 && nSnapshot + 4 < lstArguments.size())
    {
        QQuickWindow* pWindow = qobject_cast<QQuickWindow*>(qmlEngine.rootObjects().first());
        const QString strOutput = lstArguments.at(nSnapshot + 1);
        const QString strPage = lstArguments.at(nSnapshot + 2);
        const int nCaptureWidth = lstArguments.at(nSnapshot + 3).toInt();
        const int nCaptureHeight = lstArguments.at(nSnapshot + 4).toInt();
        if (nCaptureWidth > 0 && nCaptureHeight > 0) { pWindow->resize(nCaptureWidth, nCaptureHeight); }
        if (pWindow->screen())
        {
            const QRect rectScreen = pWindow->screen()->availableGeometry();
            qInfo("WINDOW_FIT screen=%dx%d window=%dx%d", rectScreen.width(), rectScreen.height(), pWindow->width(), pWindow->height());
        }
        pWindow->setProperty("currentPage", strPage);
        if (strPage == QStringLiteral("settings"))
        {
            QObject* pSettings = pWindow->findChild<QObject*>(QStringLiteral("settingsPage"));
            if (pSettings) { pSettings->setProperty("tab", 4); }
        }
        QTimer::singleShot(1200, &appApplication, [pWindow, strOutput]()
        { QCoreApplication::exit(pWindow->grabWindow().save(strOutput) ? 0 : 3); });
    }
#endif
    return appApplication.exec();  // 运行事件循环
}
