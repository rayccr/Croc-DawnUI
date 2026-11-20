/**
****************************************************************
* @file    EngineSelfTest.cpp
* @brief   引擎联调自检实现：通过应用控制器跑一次真实的发送与接收
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
*          V1.0.2       Ray       2026-11-01    2. 改为经 AppController 驱动
****************************************************************
*/
#include "EngineSelfTest.h"  // 包含自检头文件

#include <QCoreApplication>  // 进程信息
#include <QDir>  // 目录操作
#include <QDirIterator>  // 递归核对接收文件
#include <QElapsedTimer>  // 超时计时
#include <QEventLoop>  // 局部事件循环
#include <QFile>  // 测试文件
#include <QFileInfo>  // 文件信息
#include <QHash>  // 进度记录
#include <QProcess>  // 中继进程
#include <QTcpSocket>  // 中继就绪探测
#include <QThread>  // 等待间隔
#include <QTimer>  // 超时控制
#include <QVariantMap>  // 任务详情
#include <QJsonArray>  // 发送配置
#include <QJsonDocument>  // JSON 序列化
#include <QJsonObject>  // 发送配置
#include <cstdio>  // 标准输出

#include <QCryptographicHash>
#include "AppController.h"  // 应用控制器
#include "CrocEventPump.h"  // 事件线程
#include "CrocJsonMap.h"  // 快照映射与后端选择
#include "CrocLibrary.h"  // 引擎动态库
#include "TransferTaskModel.h"  // 任务模型（读取行数）

namespace
{
/**
****************************************************************
* @brief 向标准输出打印一行并立即刷新
* @param [IN] _strText 文本
* @return 无
* @others 自检不依赖日志系统，直接用 stdout，便于重定向到文件
* @author Ray
****************************************************************
*/
void PrintLine(const QString& _strText)
{
    std::fputs((_strText + QStringLiteral("\n")).toUtf8().constData(), stdout);
    std::fflush(stdout);
}

/**
****************************************************************
* @brief 等待本机中继端口可连接
* @param [IN] _nPort 端口
* @param [IN] _nTimeoutMs 最长等待时间
* @return 可连接返回 true
* @others 进程启动不等于端口已监听，必须先探测再启动发送端
* @author Ray
****************************************************************
*/
bool WaitForRelay(quint16 _nPort, int _nTimeoutMs)
{
    QElapsedTimer timerElapsed;
    timerElapsed.start();
    while (timerElapsed.elapsed() < _nTimeoutMs)
    {
        QTcpSocket socketProbe;
        socketProbe.connectToHost(QStringLiteral("127.0.0.1"), _nPort);
        if (socketProbe.waitForConnected(300))
        {
            socketProbe.disconnectFromHost();
            return true;
        }
        QThread::msleep(200);
    }
    return false;
}
}

int RunEngineSelfTest(const QString& _strCrocPath, const QString& _strWorkDir, bool _bLongSingleFile, bool _bSendOnlyTimeout)  // 运行联调自检
{
    if (!QFileInfo::exists(_strCrocPath))
    {
        PrintLine(QStringLiteral("找不到 croc 引擎：") + _strCrocPath);
        return 1;
    }

    QDir dirWork(_strWorkDir);
    // 不再删除任何调用方目录；即使目录为空也拒绝复用，防止路径错误造成损失。
    if (!QFileInfo(_strWorkDir).isAbsolute() || QFileInfo::exists(_strWorkDir))
    {
        PrintLine(QStringLiteral("自检目录必须是尚不存在的绝对路径；不会删除已有目录。"));
        return 2;
    }
    QDir().mkpath(_strWorkDir + QStringLiteral("/src"));
    QDir().mkpath(_strWorkDir + QStringLiteral("/dst"));
    QDir().mkpath(_strWorkDir + QStringLiteral("/home"));

    QDir().mkpath(_strWorkDir + QStringLiteral("/src/sub"));

    // 三个文件（含一个子目录），用于验证多文件清单与逐文件计数
    const QStringList lstSourceFiles = _bLongSingleFile ? QStringList{
        _strWorkDir + QStringLiteral("/src/41135911C8237A7A69F9968D4A19E311.jpg")
    } : QStringList{
        _strWorkDir + QStringLiteral("/src/a.txt"),
        _strWorkDir + QStringLiteral("/src/b.txt"),
        _strWorkDir + QStringLiteral("/src/sub/c.bin")
    };
    const QList<int> lstFileBlocks = _bLongSingleFile ? QList<int>{4} : QList<int>{4, 8, 12};  // 每块 256 KB
    QByteArray arrBlock(256 * 1024, Qt::Uninitialized);
    for (int nIndex = 0; nIndex < arrBlock.size(); ++nIndex)
    {
        arrBlock[nIndex] = static_cast<char>((nIndex * 31 + 7) & 0xFF);
    }
    for (int nFile = 0; nFile < lstSourceFiles.size(); ++nFile)
    {
        QFile fileSource(lstSourceFiles.at(nFile));
        if (!fileSource.open(QIODevice::WriteOnly))
        {
            PrintLine(QStringLiteral("无法创建测试文件：") + lstSourceFiles.at(nFile));
            return 1;
        }
        for (int nBlock = 0; nBlock < lstFileBlocks.at(nFile); ++nBlock)
        {
            fileSource.write(arrBlock);
        }
        fileSource.close();
    }
    qint64 llSourceBytes = 0;
    for (const QString& strSource : lstSourceFiles) { llSourceBytes += QFileInfo(strSource).size(); }
    PrintLine(QStringLiteral("测试文件：%1 个，共 %2 字节").arg(lstSourceFiles.size()).arg(llSourceBytes));

    QProcess procRelay;  // 本机中继，避免依赖公网
    procRelay.setProgram(_strCrocPath);
    procRelay.setArguments(QStringList() << QStringLiteral("relay") << QStringLiteral("--port") << QStringLiteral("9021"));
    procRelay.start();
    if (!procRelay.waitForStarted(5000) || !WaitForRelay(9021, 8000))
    {
        PrintLine(QStringLiteral("中继启动失败或端口未就绪"));
        procRelay.kill();
        procRelay.waitForFinished(2000);
        return 1;
    }
    PrintLine(QStringLiteral("中继已启动：127.0.0.1:9021"));

    // 通过配置文件把中继交给控制器：这条路径同时验证"设置 → 引擎参数"的映射
    const QString strConfigPath = _strWorkDir + QStringLiteral("/config.json");
    {
        QFile fileConfig(strConfigPath);
        if (!fileConfig.open(QIODevice::WriteOnly))
        {
            PrintLine(QStringLiteral("无法写入测试配置：") + strConfigPath);
            procRelay.kill();
            procRelay.waitForFinished(2000);
            return 1;
        }
        fileConfig.write(_bSendOnlyTimeout
            ? "{\"relay\":\"127.0.0.1:9021\",\"curve\":\"p256\",\"rate\":\"0\",\"codeTimeout\":\"30\"}"
            : "{\"relay\":\"127.0.0.1:9021\",\"curve\":\"p256\",\"rate\":\"0\"}");
        fileConfig.close();
    }
    qputenv("FLOWDROP_CONFIG_PATH", strConfigPath.toUtf8());
    qputenv("FLOWDROP_CROC_PATH", _strCrocPath.toUtf8());
    qputenv("FLOWDROP_ENGINE_HOME", (_strWorkDir + QStringLiteral("/home")).toUtf8());
    const QString strHistoryPath = _strWorkDir + QStringLiteral("/history.jsonl");
    qputenv("FLOWDROP_HISTORY_PATH", strHistoryPath.toUtf8());  // 历史写到自检目录，避免污染真实用户数据

    AppController appController;
    PrintLine(QStringLiteral("引擎状态：") + appController.GetEngineStatus());

    QEventLoop loopEvent;
    QTimer::singleShot(120000, &loopEvent, &QEventLoop::quit);

    bool bReceiveFinished = false;
    bool bTimeoutOk = false;  // 保留旧自检参数，改验旧上限后仍可用并正常取消。
    bool bSendWaitCheckScheduled = false;
    bool bSendWaitVerified = false;
    bool bSendFinished = false;
    QString strSendId;
    QString strRecvId;
    QHash<QString, int> hashLastPercent;

    QObject::connect(&appController, &AppController::taskRevisionChanged, [&]()
    {
        if (strSendId.isEmpty() && strRecvId.isEmpty())
        {
            return;
        }
        for (const QString& strTaskId : {strSendId, strRecvId})
        {
            if (strTaskId.isEmpty())
            {
                continue;
            }
            const QVariantMap hashTask = appController.getTask(strTaskId);
            if (hashTask.isEmpty())
            {
                continue;
            }
            const int nPercent = static_cast<int>(hashTask.value(QStringLiteral("percent")).toDouble());
            const QString strStage = hashTask.value(QStringLiteral("stage")).toString();
            const QString strKey = strTaskId + strStage;
            if (hashLastPercent.value(strKey, -1) == nPercent)
            {
                continue;
            }
            hashLastPercent.insert(strKey, nPercent);
            PrintLine(QStringLiteral("[%1] %2 百分比=%3 速度=%4 对端=%5")
                      .arg(strTaskId, strStage)
                      .arg(nPercent)
                      .arg(hashTask.value(QStringLiteral("speed")).toString(),
                           hashTask.value(QStringLiteral("peer")).toString()));
        }
    });

    QObject::connect(&appController, &AppController::notice, [](const QString& _strText)
    {
        PrintLine(QStringLiteral("提示：") + _strText);
    });

    QObject::connect(&appController, &AppController::codeReady, [&](const QString& _strTaskId, const QString& _strCode)
    {
        PrintLine(QStringLiteral("[%1] 口令就绪（已脱敏）").arg(_strTaskId));
        if (_bSendOnlyTimeout)
        {
            PrintLine(QStringLiteral("仅发送模式：越过旧等待上限后检查原口令，再主动取消"));
            if (bSendWaitCheckScheduled) { return; }
            bSendWaitCheckScheduled = true;
            QTimer::singleShot(35000, &loopEvent, [&, strTaskId = _strTaskId, strCode = _strCode]()
            {
                const QVariantMap hashTask = appController.getTask(strTaskId);
                bSendWaitVerified = hashTask.value(QStringLiteral("shareReady")).toBool()
                    && hashTask.value(QStringLiteral("code")).toString() == strCode;
                appController.cancelTask(strTaskId);
            });
            return;
        }
        if (!strRecvId.isEmpty())
        {
            return;
        }
        // 信号参数是引用，延时执行必须按值拷贝
        const QString strCodeCopy = _strCode;
        const QString strOutDirCopy = _strWorkDir + QStringLiteral("/dst");
        QTimer::singleShot(800, &loopEvent, [&appController, &loopEvent, &strRecvId, strCodeCopy, strOutDirCopy]()
        {
            strRecvId = appController.startReceive(strCodeCopy, strOutDirCopy);
            PrintLine(QStringLiteral("接收任务：%1").arg(strRecvId.isEmpty() ? QStringLiteral("启动失败") : strRecvId));
            if (strRecvId.isEmpty())
            {
                loopEvent.quit();
            }
        });
    });

    QObject::connect(&appController, &AppController::receivePrompt,
                     [&](const QString& _strTaskId, const QString& _strName, const QString& _strSize, const QString& _strPeer)
    {
        PrintLine(QStringLiteral("[%1] 接收确认：%2（%3）来自 %4 → 同意").arg(_strTaskId, _strName, _strSize, _strPeer));
        appController.answerPrompt(true);
    });

    QObject::connect(&appController, &AppController::resumePrompt,
                     [&](const QString& _strTaskId, const QString& _strName, double _dPercent)
    {
        PrintLine(QStringLiteral("[%1] 续传确认：%2（%3%）→ 同意").arg(_strTaskId, _strName).arg(_dPercent, 0, 'f', 1));
        appController.answerPrompt(true);
    });

    QObject::connect(&appController, &AppController::taskFinished,
                     [&](const QString& _strTaskId, int _nExitCode, bool _bCancelled, const QString& _strError)
    {
        PrintLine(QStringLiteral("[%1] 结束：退出码=%2 取消=%3 错误=%4")
                  .arg(_strTaskId).arg(_nExitCode).arg(_bCancelled ? QStringLiteral("是") : QStringLiteral("否"), _strError));
        if (_bSendOnlyTimeout && _strTaskId == strSendId)
        {
            bTimeoutOk = bSendWaitVerified && _bCancelled;
            PrintLine(QStringLiteral("等待检查：已主动取消发送，取消标志=%1").arg(_bCancelled ? QStringLiteral("是") : QStringLiteral("否")));
            loopEvent.quit();
            return;
        }
        if (_strTaskId == strRecvId) { bReceiveFinished = (0 == _nExitCode) && !_bCancelled && _strError.isEmpty(); }
        if (_strTaskId == strSendId) { bSendFinished = (0 == _nExitCode) && !_bCancelled && _strError.isEmpty(); }
        if ((bReceiveFinished && bSendFinished) || _nExitCode != 0) { loopEvent.quit(); }
    });

    strSendId = appController.startSend(lstSourceFiles);
    if (strSendId.isEmpty())
    {
        PrintLine(QStringLiteral("发送任务启动失败"));
        procRelay.kill();
        procRelay.waitForFinished(2000);
        return 1;
    }
    PrintLine(QStringLiteral("发送任务：") + strSendId);

    loopEvent.exec();
    PrintLine(QStringLiteral("事件循环已退出"));

    if (_bSendOnlyTimeout)
    {
        appController.cancelTask(strSendId);
        procRelay.kill();
        procRelay.waitForFinished(2000);
        PrintLine(QStringLiteral("自检结果：%1").arg(bTimeoutOk ? QStringLiteral("通过") : QStringLiteral("未通过")));
        return bTimeoutOk ? 0 : 1;
    }

    // 结果核对：模型里有任务，且每个文件都真实落盘、大小一致。
    // 实测发现引擎可能把子目录内容展平到输出目录，因此按文件名递归查找，不假设目录结构。
    const int nRows = appController.GetTaskModel()->rowCount();
    PrintLine(QStringLiteral("模型行数：%1").arg(nRows));

    QHash<QString, qint64> hashReceived;  // 文件名 -> 大小
    QDirIterator itReceived(_strWorkDir + QStringLiteral("/dst"), QDir::Files, QDirIterator::Subdirectories);
    while (itReceived.hasNext())
    {
        const QString strPath = itReceived.next();
        hashReceived.insert(QFileInfo(strPath).fileName(), QFileInfo(strPath).size());
    }

    bool bSizeOk = true;
    for (const QString& strSourceFile : lstSourceFiles)
    {
        const qint64 llSourceSize = QFileInfo(strSourceFile).size();
        const QString strName = QFileInfo(strSourceFile).fileName();
        const qint64 llReceivedSize = hashReceived.value(strName, -1);
        // 明确验证 CLI 逐个传入的三个文件对应目标文件名，而不是仅递归找同名。
        QFile fileSourceHash(strSourceFile);
        QFile fileTargetHash(_strWorkDir + QStringLiteral("/dst/") + strName);
        QCryptographicHash hashSource(QCryptographicHash::Sha256);
        QCryptographicHash hashTarget(QCryptographicHash::Sha256);
        const bool bHashOk = fileSourceHash.open(QIODevice::ReadOnly) && fileTargetHash.open(QIODevice::ReadOnly)
            && hashSource.addData(&fileSourceHash) && hashTarget.addData(&fileTargetHash) && hashSource.result() == hashTarget.result();
        const bool bSame = (llSourceSize == llReceivedSize) && (llSourceSize > 0) && bHashOk;
        bSizeOk = bSizeOk && bSame;
        PrintLine(QStringLiteral("  %1：源 %2 / 收 %3 %4")
                  .arg(strName).arg(llSourceSize).arg(llReceivedSize)
                  .arg(bSame ? QStringLiteral("一致") : QStringLiteral("不一致")));
    }

    if (!bSendFinished) { appController.cancelTask(strSendId); }
    procRelay.kill();
    procRelay.waitForFinished(2000);

    const bool bModelOk = (nRows >= 2);

    // 历史记录核对：任务结束应留下一条本地记录
    int nHistoryLines = 0;
    QFile fileHistory(strHistoryPath);
    if (fileHistory.open(QIODevice::ReadOnly))
    {
        while (!fileHistory.atEnd())
        {
            if (!fileHistory.readLine().trimmed().isEmpty())
            {
                ++nHistoryLines;
            }
        }
        fileHistory.close();
    }
    PrintLine(QStringLiteral("历史记录行数：%1").arg(nHistoryLines));
    const bool bHistoryOk = (nHistoryLines == 2);

    // 配置落盘核对：改一项设置并确认写进了文件
    QVariantMap hashSettings = appController.GetSettings();
    hashSettings.insert(QStringLiteral("rate"), QStringLiteral("3"));
    appController.saveSettings(hashSettings);
    bool bConfigOk = false;
    QFile fileConfigCheck(strConfigPath);
    if (fileConfigCheck.open(QIODevice::ReadOnly))
    {
        const QByteArray arrConfig = fileConfigCheck.readAll();
        fileConfigCheck.close();
        bConfigOk = arrConfig.contains("\"rate\": \"3\"");
    }
    PrintLine(QStringLiteral("配置落盘核对：%1").arg(bConfigOk ? QStringLiteral("通过") : QStringLiteral("失败")));

    PrintLine(QStringLiteral("自检结果：%1").arg((bReceiveFinished && bSendFinished && bSizeOk && bModelOk && bHistoryOk && bConfigOk)
                                                ? QStringLiteral("通过") : QStringLiteral("未通过")));
    return (bReceiveFinished && bSendFinished && bSizeOk && bModelOk && bHistoryOk && bConfigOk) ? 0 : 1;
}

int RunEngineApiCheck()  // 动态库加载与初始化自检
{
    CrocLibrary& libEngine = CrocLibrary::Instance();
    if (!libEngine.Load())
    {
        PrintLine(QStringLiteral("加载引擎库失败：") + libEngine.GetLastErrorText());
        return 1;
    }
    PrintLine(QStringLiteral("引擎库已加载"));
    PrintLine(QStringLiteral("ABI 版本：%1").arg(libEngine.GetApiVersion()));
    PrintLine(QStringLiteral("引擎版本：%1").arg(libEngine.GetEngineVersion()));
    if (!libEngine.IsApiCompatible())
    {
        PrintLine(QStringLiteral("ABI 版本不兼容，拒绝调用"));
        return 1;
    }
    const int32_t nResult = libEngine.Initialize();
    PrintLine(QStringLiteral("初始化返回码：%1").arg(nResult));
    if (0 != nResult)
    {
        PrintLine(QStringLiteral("初始化失败：") + libEngine.GetLastErrorText());
        return 1;
    }
    PrintLine(QStringLiteral("任务数=%1 活动任务=%2")
              .arg(libEngine.GetTaskCount()).arg(libEngine.GetActiveTaskCount()));
    libEngine.Shutdown();
    PrintLine(QStringLiteral("引擎已关闭"));
    PrintLine(QStringLiteral("自检结果：通过"));
    return 0;
}

int RunEngineApiTaskCheck(const QString& _strCrocPath)  // 任务级自检：发送任务 + 事件 + 快照 + 取消
{
    QProcess procRelay;  // 测试夹具：本机中继
    procRelay.setProgram(_strCrocPath);
    procRelay.setArguments(QStringList() << QStringLiteral("relay") << QStringLiteral("--port") << QStringLiteral("9021"));
    procRelay.start();
    if (!procRelay.waitForStarted(5000) || !WaitForRelay(9021, 8000))
    {
        PrintLine(QStringLiteral("中继未就绪"));
        procRelay.kill();
        procRelay.waitForFinished(2000);
        return 1;
    }

    const QString strWorkDir = QDir::tempPath() + QStringLiteral("/CrocApiTask-") + QString::number(QCoreApplication::applicationPid());
    QDir().mkpath(strWorkDir);
    const QString strFile = strWorkDir + QStringLiteral("/api.bin");
    QFile fileSource(strFile);
    if (!fileSource.open(QIODevice::WriteOnly))
    {
        PrintLine(QStringLiteral("无法创建测试文件"));
        procRelay.kill();
        procRelay.waitForFinished(2000);
        return 1;
    }
    fileSource.write(QByteArray(1024 * 1024, 'A'));
    fileSource.close();

    CrocLibrary& libEngine = CrocLibrary::Instance();
    if (!libEngine.Load() || !libEngine.IsApiCompatible() || (0 != libEngine.Initialize()))
    {
        PrintLine(QStringLiteral("引擎库不可用：") + libEngine.GetLastErrorText());
        procRelay.kill();
        procRelay.waitForFinished(2000);
        return 1;
    }

    QJsonObject objConfig;
    objConfig.insert(QStringLiteral("code"), QStringLiteral("api-task-1"));
    QJsonArray arrFiles;
    arrFiles.append(strFile);
    objConfig.insert(QStringLiteral("files"), arrFiles);
    objConfig.insert(QStringLiteral("relay"), QStringLiteral("127.0.0.1:9021"));
    objConfig.insert(QStringLiteral("relay_password"), QStringLiteral("pass123"));
    objConfig.insert(QStringLiteral("no_compress"), true);
    objConfig.insert(QStringLiteral("disable_local"), true);

    quint64 nTaskId = 0;
    QString strError;
    const QByteArray arrJson = QJsonDocument(objConfig).toJson(QJsonDocument::Compact);
    if (!libEngine.StartSend(arrJson, nTaskId, strError))
    {
        PrintLine(QStringLiteral("启动发送失败：") + strError);
        libEngine.Shutdown();
        procRelay.kill();
        procRelay.waitForFinished(2000);
        return 1;
    }
    PrintLine(QStringLiteral("发送任务已创建：id=%1").arg(nTaskId));

    int nEventCount = 0;
    QString strFirstEvent;
    bool bWrongThread = false;
    CrocEventPump pumpEvents(&libEngine);
    QObject::connect(&pumpEvents, &CrocEventPump::eventReceived, QCoreApplication::instance(),
                     [&](const QByteArray& _arrEvent)
    {
        if (QThread::currentThread() != QCoreApplication::instance()->thread()) { bWrongThread = true; }
        ++nEventCount;
        if (strFirstEvent.isEmpty()) { strFirstEvent = QString::fromUtf8(_arrEvent); }
    });
    pumpEvents.start();
    QElapsedTimer timerPoll;
    timerPoll.start();
    while (timerPoll.elapsed() < 6000)
    {
        QCoreApplication::processEvents();
        QThread::msleep(20);
    }
    pumpEvents.Stop();
    PrintLine(QStringLiteral("事件回调线程错误：%1").arg(bWrongThread ? QStringLiteral("是") : QStringLiteral("否")));
    PrintLine(QStringLiteral("6 秒内收到事件数：%1").arg(nEventCount));
    if (!strFirstEvent.isEmpty())
    {
        PrintLine(QStringLiteral("首个事件：") + strFirstEvent.left(120));
    }
    const QByteArray arrSnapshot = libEngine.GetTaskSnapshot(nTaskId);
    PrintLine(QStringLiteral("快照长度：%1").arg(arrSnapshot.size()));
    if (!arrSnapshot.isEmpty())
    {
        PrintLine(QStringLiteral("快照片段：") + QString::fromUtf8(arrSnapshot).left(120));
    }

    STaskSnapshot stMapped;  // 结构化映射结果
    QString strMapError;
    if (MapCrocSnapshotJson(arrSnapshot, stMapped, strMapError))
    {
        PrintLine(QStringLiteral("映射：任务=%1 方向=%2 阶段=%3 百分比=%4 速度=%5 剩余=%6 总量=%7 文件数=%8")
                  .arg(stMapped.strTaskId)
                  .arg(stMapped.enDirection == ETaskDirection::DIRECTION_SEND ? QStringLiteral("send") : QStringLiteral("receive"))
                  .arg(static_cast<int>(stMapped.enStage))
                  .arg(stMapped.dPercent, 0, 'f', 1)
                  .arg(stMapped.strSpeed, stMapped.strEta)
                  .arg(stMapped.llTaskBytes)
                  .arg(stMapped.nFileCount));
    }
    else
    {
        PrintLine(QStringLiteral("映射失败：") + strMapError);
    }
    libEngine.CancelTask(nTaskId);
    bool bFinished = false;
    QElapsedTimer timerCancel;
    timerCancel.start();
    while (timerCancel.elapsed() < 15000)
    {
        if (libEngine.IsTaskFinished(nTaskId, bFinished) && bFinished) { break; }
        while (!libEngine.PollEvent().isEmpty()) { }
        QThread::msleep(100);
    }
    PrintLine(QStringLiteral("取消后已结束：%1").arg(bFinished ? QStringLiteral("是") : QStringLiteral("否")));
    libEngine.ReleaseTask(nTaskId);
    libEngine.Shutdown();
    procRelay.kill();
    procRelay.waitForFinished(2000);
    QDir(strWorkDir).removeRecursively();

    const bool bPass = (nEventCount > 0) && bFinished && (arrSnapshot.size() > 0) && !bWrongThread;
    PrintLine(QStringLiteral("自检结果：%1").arg(bPass ? QStringLiteral("通过") : QStringLiteral("未通过")));
    return bPass ? 0 : 1;
}

int RunEngineDllTransferCheck(const QString& _strCrocPath)  // DLL 到 DLL 的真实传输 + 结构化进度
{
    QProcess procRelay;
    procRelay.setProgram(_strCrocPath);
    procRelay.setArguments(QStringList() << QStringLiteral("relay") << QStringLiteral("--port") << QStringLiteral("9022"));
    procRelay.start();
    if (!procRelay.waitForStarted(5000) || !WaitForRelay(9022, 8000))
    {
        PrintLine(QStringLiteral("中继未就绪"));
        procRelay.kill(); procRelay.waitForFinished(2000); return 1;
    }
    const QString strWorkDir = QDir::tempPath() + QStringLiteral("/CrocDllTransfer-") + QString::number(QCoreApplication::applicationPid());
    const QString strSrcDir = strWorkDir + QStringLiteral("/src");
    const QString strDstDir = strWorkDir + QStringLiteral("/dst");
    QDir().mkpath(strSrcDir); QDir().mkpath(strDstDir);
    const QString strFile = strSrcDir + QStringLiteral("/payload.bin");
    QFile fileSource(strFile);
    if (!fileSource.open(QIODevice::WriteOnly))
    {
        PrintLine(QStringLiteral("无法创建测试文件"));
        procRelay.kill(); procRelay.waitForFinished(2000); return 1;
    }
    fileSource.write(QByteArray(24 * 1024 * 1024, 'B'));  // 24 MB，足够产生多帧进度
    fileSource.close();

    CrocLibrary& libEngine = CrocLibrary::Instance();
    if (!libEngine.Load() || !libEngine.IsApiCompatible() || (0 != libEngine.Initialize()))
    {
        PrintLine(QStringLiteral("引擎库不可用：") + libEngine.GetLastErrorText());
        procRelay.kill(); procRelay.waitForFinished(2000); return 1;
    }

    QJsonObject objSend;
    objSend.insert(QStringLiteral("code"), QStringLiteral("dll-transfer-1"));
    QJsonArray arrFiles; arrFiles.append(strFile);
    objSend.insert(QStringLiteral("files"), arrFiles);
    objSend.insert(QStringLiteral("relay"), QStringLiteral("127.0.0.1:9022"));
    objSend.insert(QStringLiteral("relay_password"), QStringLiteral("pass123"));
    objSend.insert(QStringLiteral("no_compress"), true);
    objSend.insert(QStringLiteral("disable_local"), true);
    objSend.insert(QStringLiteral("throttle_upload"), QStringLiteral("12m"));  // 限速，让进度可观察

    quint64 nSendId = 0; QString strError;
    if (!libEngine.StartSend(QJsonDocument(objSend).toJson(QJsonDocument::Compact), nSendId, strError))
    {
        PrintLine(QStringLiteral("启动发送失败：") + strError);
        libEngine.Shutdown(); procRelay.kill(); procRelay.waitForFinished(2000); return 1;
    }

    QJsonObject objRecv;
    objRecv.insert(QStringLiteral("code"), QStringLiteral("dll-transfer-1"));
    objRecv.insert(QStringLiteral("save_path"), strDstDir);
    objRecv.insert(QStringLiteral("auto_accept"), true);
    objRecv.insert(QStringLiteral("relay"), QStringLiteral("127.0.0.1:9022"));
    objRecv.insert(QStringLiteral("relay_password"), QStringLiteral("pass123"));
    objRecv.insert(QStringLiteral("no_compress"), true);
    objRecv.insert(QStringLiteral("disable_local"), true);
    quint64 nRecvId = 0;
    if (!libEngine.StartReceive(QJsonDocument(objRecv).toJson(QJsonDocument::Compact), nRecvId, strError))
    {
        PrintLine(QStringLiteral("启动接收失败：") + strError);
        libEngine.CancelTask(nSendId); libEngine.Shutdown(); procRelay.kill(); procRelay.waitForFinished(2000); return 1;
    }
    PrintLine(QStringLiteral("DLL 任务：发送=%1 接收=%2").arg(nSendId).arg(nRecvId));

    int nSamples = 0;
    bool bSendFinished = false; bool bRecvFinished = false;
    bool bSawProgress = false; bool bSawSpeed = false; bool bSawEta = false;
    QElapsedTimer timerRun; timerRun.start();
    while (timerRun.elapsed() < 60000)
    {
        QCoreApplication::processEvents();
        while (!libEngine.PollEvent().isEmpty()) { }
        bool bFinished = false;
        if (!bSendFinished && libEngine.IsTaskFinished(nSendId, bFinished)) { bSendFinished = bFinished; }
        bFinished = false;
        if (!bRecvFinished && libEngine.IsTaskFinished(nRecvId, bFinished)) { bRecvFinished = bFinished; }
        STaskSnapshot stRecv;
        QString strMapError;
        if (MapCrocSnapshotJson(libEngine.GetTaskSnapshot(nRecvId), stRecv, strMapError) && (nSamples < 6)
            && (stRecv.enStage == ETaskStage::STAGE_TRANSFERRING || stRecv.enStage == ETaskStage::STAGE_VERIFYING))
        {
            ++nSamples;
            bSawProgress = bSawProgress || (stRecv.dPercent >= 0);
            bSawSpeed = bSawSpeed || !stRecv.strSpeed.isEmpty();
            bSawEta = bSawEta || !stRecv.strEta.isEmpty();
            PrintLine(QStringLiteral("接收进度样本 %1：阶段=%2 百分比=%3 速度=%4 剩余=%5")
                      .arg(nSamples).arg(static_cast<int>(stRecv.enStage))
                      .arg(stRecv.dPercent, 0, 'f', 1).arg(stRecv.strSpeed, stRecv.strEta));
        }
        if (bSendFinished && bRecvFinished) { break; }
        QThread::msleep(200);
    }

    libEngine.CancelTask(nSendId);
    libEngine.CancelTask(nRecvId);
    QThread::msleep(300);
    while (!libEngine.PollEvent().isEmpty()) { }
    libEngine.ReleaseTask(nSendId);
    libEngine.ReleaseTask(nRecvId);
    libEngine.Shutdown();
    procRelay.kill(); procRelay.waitForFinished(2000);

    const QString strReceived = strDstDir + QStringLiteral("/payload.bin");
    const qint64 llSrc = QFileInfo(strFile).size();
    const qint64 llDst = QFileInfo::exists(strReceived) ? QFileInfo(strReceived).size() : -1;
    const bool bSizeOk = (llSrc == llDst) && (llSrc > 0);
    PrintLine(QStringLiteral("发送结束=%1 接收结束=%2 源=%3 收=%4")
              .arg(bSendFinished ? QStringLiteral("是") : QStringLiteral("否"),
                   bRecvFinished ? QStringLiteral("是") : QStringLiteral("否")).arg(llSrc).arg(llDst));
    PrintLine(QStringLiteral("结构化字段：进度=%1 速度=%2 剩余=%3")
              .arg(bSawProgress ? QStringLiteral("有") : QStringLiteral("无"),
                   bSawSpeed ? QStringLiteral("有") : QStringLiteral("无"),
                   bSawEta ? QStringLiteral("有") : QStringLiteral("无")));
    QDir(strWorkDir).removeRecursively();
    const bool bPass = bRecvFinished && bSizeOk && bSawProgress;
    PrintLine(QStringLiteral("自检结果：%1").arg(bPass ? QStringLiteral("通过") : QStringLiteral("未通过")));
    return bPass ? 0 : 1;
}

int RunEngineLibOnlyCheck()  // 库模式检查：完全不依赖 croc.exe
{
    qputenv("FLOWDROP_CROC_PATH", "D:/nonexistent/croc.exe");
    AppController controller;
    PrintLine(QStringLiteral("引擎状态：") + controller.GetEngineStatus());
    PrintLine(QStringLiteral("引擎版本：") + controller.GetEngineVersion());
    const bool bPass = controller.GetEngineStatus().contains(QStringLiteral("库模式"))
                       && !controller.GetEngineVersion().isEmpty();
    PrintLine(QStringLiteral("自检结果：%1").arg(bPass ? QStringLiteral("通过") : QStringLiteral("未通过")));
    return bPass ? 0 : 1;
}

int RunEngineDllLocalCheck()  // 无 croc.exe：库内本地中继收发
{
    qputenv("FLOWDROP_ENGINE_BACKEND", "dll");
    qputenv("FLOWDROP_CROC_PATH", "D:/nonexistent/croc.exe");
    CrocLibrary& libEngine = CrocLibrary::Instance();
    if (!libEngine.Load() || !libEngine.IsApiCompatible() || (0 != libEngine.Initialize()))
    {
        PrintLine(QStringLiteral("引擎库不可用：") + libEngine.GetLastErrorText());
        return 1;
    }
    const QString strWorkDir = QDir::tempPath() + QStringLiteral("/CrocLocal-") + QString::number(QCoreApplication::applicationPid());
    const QString strSrcDir = strWorkDir + QStringLiteral("/src");
    const QString strDstDir = strWorkDir + QStringLiteral("/dst");
    QDir().mkpath(strSrcDir); QDir().mkpath(strDstDir);
    const QString strFile = strSrcDir + QStringLiteral("/local.bin");
    QFile fileSource(strFile);
    if (!fileSource.open(QIODevice::WriteOnly)) { PrintLine(QStringLiteral("无法创建测试文件")); return 1; }
    fileSource.write(QByteArray(8 * 1024 * 1024, 'C'));
    fileSource.close();

    QJsonObject objSend;
    objSend.insert(QStringLiteral("code"), QStringLiteral("local-relay-1"));
    QJsonArray arrFiles; arrFiles.append(strFile);
    objSend.insert(QStringLiteral("files"), arrFiles);
    objSend.insert(QStringLiteral("only_local"), true);
    objSend.insert(QStringLiteral("disable_local"), false);
    QJsonArray arrPorts; arrPorts.append(QStringLiteral("9041")); arrPorts.append(QStringLiteral("9042"));
    objSend.insert(QStringLiteral("local_relay_ports"), arrPorts);
    objSend.insert(QStringLiteral("no_compress"), true);
    quint64 nSendId = 0; QString strError;
    if (!libEngine.StartSend(QJsonDocument(objSend).toJson(QJsonDocument::Compact), nSendId, strError))
    {
        PrintLine(QStringLiteral("启动发送失败：") + strError);
        return 1;
    }
    QJsonObject objRecv;
    objRecv.insert(QStringLiteral("code"), QStringLiteral("local-relay-1"));
    objRecv.insert(QStringLiteral("save_path"), strDstDir);
    objRecv.insert(QStringLiteral("auto_accept"), true);
    objRecv.insert(QStringLiteral("only_local"), true);
    objRecv.insert(QStringLiteral("disable_local"), false);
    objRecv.insert(QStringLiteral("local_relay_ports"), arrPorts);
    objRecv.insert(QStringLiteral("no_compress"), true);
    quint64 nRecvId = 0;
    if (!libEngine.StartReceive(QJsonDocument(objRecv).toJson(QJsonDocument::Compact), nRecvId, strError))
    {
        PrintLine(QStringLiteral("启动接收失败：") + strError);
        libEngine.CancelTask(nSendId);
        return 1;
    }
    bool bSendDone = false; bool bRecvDone = false;
    QElapsedTimer timerRun; timerRun.start();
    while (timerRun.elapsed() < 40000)
    {
        QCoreApplication::processEvents();
        while (!libEngine.PollEvent().isEmpty()) { }
        bool bFinished = false;
        if (!bSendDone && libEngine.IsTaskFinished(nSendId, bFinished)) { bSendDone = bFinished; }
        bFinished = false;
        if (!bRecvDone && libEngine.IsTaskFinished(nRecvId, bFinished)) { bRecvDone = bFinished; }
        if (bSendDone && bRecvDone) { break; }
        QThread::msleep(200);
    }
    const QString strReceived = strDstDir + QStringLiteral("/local.bin");
    const qint64 llSrc = QFileInfo(strFile).size();
    const qint64 llDst = QFileInfo::exists(strReceived) ? QFileInfo(strReceived).size() : -1;
    PrintLine(QStringLiteral("本地中继：发送结束=%1 接收结束=%2 源=%3 收=%4")
              .arg(bSendDone ? QStringLiteral("是") : QStringLiteral("否"), bRecvDone ? QStringLiteral("是") : QStringLiteral("否")).arg(llSrc).arg(llDst));
    libEngine.CancelTask(nSendId); libEngine.CancelTask(nRecvId);
    QThread::msleep(200);
    while (!libEngine.PollEvent().isEmpty()) { }
    libEngine.ReleaseTask(nSendId); libEngine.ReleaseTask(nRecvId);
    QDir(strWorkDir).removeRecursively();
    const bool bPass = bRecvDone && (llSrc == llDst) && (llSrc > 0);
    PrintLine(QStringLiteral("自检结果：%1").arg(bPass ? QStringLiteral("通过") : QStringLiteral("未通过")));
    return bPass ? 0 : 1;
}