/**
****************************************************************
* @file    CrocAdapter.cpp
* @brief   Croc 引擎适配层实现
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
****************************************************************
*/
#include "CrocAdapter.h"  // 包含适配层头文件

#include "CrocDllSession.h"  // DLL 后端
#include "CrocLibrary.h"  // C ABI
#include "TransferSafety.h"
#include <QTimer>
#include <QDateTime>
#include <QUuid>
#include <QDir>  // 目录创建
#include <QDirIterator>  // 递归核对接收结果
#include <QDebug>  // 调试输出
#include <QFileInfo>  // 文件信息
#include <QProcess>  // 子进程
#include <QProcessEnvironment>  // 子进程环境变量
#include <QRegularExpression>  // 输出解析

namespace
{
// 进度行实测形态（多文件时行尾带 1/3 计数）：
//   selftest.bin   0% |                    | ( 0 B/8.4 MB)
//   b.txt 100% |████████████████████| (2.1/2.1 MB, 234 MB/s) 2/3
// 速度、剩余时间、文件计数都可能缺省，因此都按可选处理。
const QRegularExpression& GetProgressRegex()
{
    static const QRegularExpression s_reProgress(QStringLiteral(
        "^\\s*(?<name>.+?)\\s+(?<pct>\\d{1,3})%\\s*\\|[^|]*\\|\\s*\\(\\s*(?<done>[\\d.]+)\\s*(?<du>[KMGT]?B)?\\s*/\\s*(?<total>[\\d.]+)\\s*(?<tu>[KMGT]?B)(?:\\s*,\\s*(?<speed>[\\d.]+)\\s*(?<su>[KMGT]?B)/s)?\\s*\\)(?:\\s*\\[(?<eta>[^\\]]*)\\])?(?:\\s+(?<fidx>\\d+)/(?<fcnt>\\d+))?"));
    return s_reProgress;
}

// 接收确认实测有两种形态（多文件时名字部分不带引号）：
//   Accept 'payload.bin' (20.0 MB) from 'machine-id'? (Y/n)
//   Accept 3 files (6.0 MB) from 'machine-id'? (Y/n)
const QRegularExpression& GetAcceptRegex()
{
    static const QRegularExpression s_reAccept(QStringLiteral(
        "^\\s*Accept (?<name>(?:'[^']+')|[^()]+?) \\((?<size>[^)]+)\\) from '(?<peer>[^']+)'\\?"));
    return s_reAccept;
}

// 续传确认：Resume 'payload.bin' (0.0%)? (y/N)   (use --overwrite to omit)
const QRegularExpression& GetResumeRegex()
{
    static const QRegularExpression s_reResume(QStringLiteral(
        "^\\s*Resume (?<name>(?:'[^']+')|[^()]+?) \\((?<pct>[\\d.]+)%\\)\\?"));
    return s_reResume;
}

// 文件清单：Sending 'payload.bin' (20.0 MB) / Receiving 'payload.bin' (20.0 MB)
const QRegularExpression& GetFileRegex()
{
    static const QRegularExpression s_reFile(QStringLiteral(
        "^\\s*(?:Sending|Receiving) '(?<name>.+)' \\((?<size>[^)]+)\\)\\s*$"));
    return s_reFile;
}

// 连接信息：Sending (127.0.0.1->100.113.188.103)
const QRegularExpression& GetPeerRegex()
{
    static const QRegularExpression s_rePeer(QStringLiteral(
        "^\\s*(?:Sending|Receiving) \\((?<local>[^)]*)(?:->|<-)(?<peer>[^)]*)\\)\\s*$"));
    return s_rePeer;
}

/**
****************************************************************
* @brief 把 croc 显示用的容量文本换算成字节数
* @param [IN] _strText 例如 "21 MB"、"512 KB"
* @return 字节数；无法识别时返回 0
* @others croc 使用十进制单位，20 MiB 的文件显示为 21 MB
* @author Ray
****************************************************************
*/
qint64 ParseSizeText(const QString& _strText)
{
    static const QRegularExpression s_reSize(QStringLiteral("^\\s*(?<value>[\\d.]+)\\s*(?<unit>[KMGT]?B)\\s*$"));
    const QRegularExpressionMatch stMatch = s_reSize.match(_strText);
    if (!stMatch.hasMatch())
    {
        return 0;
    }
    const double dValue = stMatch.captured(QStringLiteral("value")).toDouble();
    const QString strUnit = stMatch.captured(QStringLiteral("unit"));
    double dScale = 1.0;
    if (QStringLiteral("KB") == strUnit)
    {
        dScale = 1e3;
    }
    else if (QStringLiteral("MB") == strUnit)
    {
        dScale = 1e6;
    }
    else if (QStringLiteral("GB") == strUnit)
    {
        dScale = 1e9;
    }
    else if (QStringLiteral("TB") == strUnit)
    {
        dScale = 1e12;
    }
    return static_cast<qint64>(dValue * dScale);
}
}

CrocAdapter::CrocAdapter(const SCrocConfig& _stConfig, QObject* _pParent)  // 保存配置快照
    : QObject(_pParent)
    , m_stConfig(_stConfig)
{
}

void CrocAdapter::SetBackend(ECrocBackend _enBackend)  // 选择后端
{
    m_enBackend = _enBackend;
    if ((ECrocBackend::BACKEND_DLL == _enBackend) && (nullptr == m_pDllSession))
    {
        CrocLibrary& libEngine = CrocLibrary::Instance();
        if (libEngine.Load() && libEngine.IsApiCompatible() && (0 == libEngine.Initialize()))
        {
            m_pDllSession = new CrocDllSession(&libEngine, this);
            connect(m_pDllSession, &CrocDllSession::promptRequested, this, &CrocAdapter::DllPromptRequested);
            connect(m_pDllSession, &CrocDllSession::taskUpdated, this, [this](const STaskSnapshot& _stSnapshot)
            {
                if (!_stSnapshot.strCode.isEmpty() && (ETaskDirection::DIRECTION_SEND == _stSnapshot.enDirection)
                    && _stSnapshot.bShareReady && !m_setDllCodeReady.contains(_stSnapshot.strTaskId))
                {
                    m_setDllCodeReady.insert(_stSnapshot.strTaskId);
                    emit CodeReady(_stSnapshot.strTaskId, _stSnapshot.strCode);
                }
                emit TaskUpdated(_stSnapshot);
            });
            connect(m_pDllSession, &CrocDllSession::taskFinished, this,
                    [this](const QString& _strTaskId, int _nExitCode, bool _bCancelled, const QString& _strError)
            {
                m_setDllCodeReady.remove(_strTaskId);
                emit TaskFinished(_strTaskId, _nExitCode, _bCancelled, _strError);
            });
        }
    }
}

CrocAdapter::~CrocAdapter()  // 结束全部子进程
{
    Shutdown();
}

QString CrocAdapter::GetLastStartError() const
{
    return m_pDllSession ? m_pDllSession->GetLastError() : QString();
}

QList<STaskSnapshot> CrocAdapter::StopAndSnapshotReceives()
{
    return m_pDllSession ? m_pDllSession->StopAndSnapshotReceives() : QList<STaskSnapshot>{};
}

void CrocAdapter::Shutdown()
{
    delete m_pDllSession;
    m_pDllSession = nullptr;
    const auto hashContexts = m_hashContexts;
    m_hashContexts.clear();  // 取走所有权后断信号，等待期间不能回调删除上下文
    for (SProcessContext* pContext : hashContexts)
    {
        if (pContext->pProcess != nullptr)
        {
            pContext->pProcess->disconnect(this);
            if (pContext->pProcess->state() != QProcess::NotRunning)
            {
                pContext->pProcess->kill();
                pContext->pProcess->waitForFinished(2000);
            }
            delete pContext->pProcess;
        }
        delete pContext;
    }
}

void CrocAdapter::SetNextConfigSnapshot(const QVariantMap& _hashConfig)
{
    m_hashNextConfig = _hashConfig;
    m_hashNextConfig.remove(QStringLiteral("password"));
}

void CrocAdapter::SetConfig(const SCrocConfig& _stConfig)  // 更新配置，只影响之后启动的任务
{
    m_stConfig = _stConfig;
}

bool CrocAdapter::StartSend(const QString& _strTaskId, const QStringList& _lstPaths, const QString& _strCode)  // 启动发送任务
{
    if ((ECrocBackend::BACKEND_DLL == m_enBackend) && (nullptr != m_pDllSession))
    {
        return m_pDllSession->StartSend(_strTaskId, _lstPaths, _strCode, m_stConfig, m_hashNextConfig);
    }
    if (_lstPaths.isEmpty() || m_hashContexts.contains(_strTaskId))
    {
        return false;
    }

    SProcessContext* pContext = new SProcessContext();
    pContext->stSnapshot.strTaskId = _strTaskId;
    pContext->stSnapshot.enDirection = ETaskDirection::DIRECTION_SEND;
    pContext->stConfig = m_stConfig;
    pContext->stSnapshot.hashConfig = m_hashNextConfig;
    const QVariantMap hashManifest = TransferSafety::InspectFiles(_lstPaths);
    if (!hashManifest.value(QStringLiteral("error")).toString().isEmpty())
    {
        delete pContext;
        return false;
    }
    pContext->stSnapshot.lstFiles = hashManifest.value(QStringLiteral("files")).toList();
    pContext->stSnapshot.lstSourcePaths = hashManifest.value(QStringLiteral("paths")).toStringList();
    pContext->stSnapshot.llTaskBytes = hashManifest.value(QStringLiteral("bytes")).toLongLong();
    pContext->stSnapshot.bSizeExact = true;
    pContext->stSnapshot.nFileCount = pContext->stSnapshot.lstFiles.size();
    pContext->stSnapshot.strCode = _strCode;
    pContext->stSnapshot.strDisplayName = QFileInfo(_lstPaths.first()).fileName();
    m_hashContexts.insert(_strTaskId, pContext);
    SetStage(pContext, ETaskStage::STAGE_STARTING);

    QStringList lstArguments = BuildCommonArguments(false);
    lstArguments << QStringLiteral("send");
    if (m_stConfig.bZipFolders)  // 目录先打包
    {
        lstArguments << QStringLiteral("--zip");
    }
    if (!_strCode.isEmpty())
    {
        lstArguments << QStringLiteral("--code") << _strCode;
    }
    lstArguments << _lstPaths;

    if (!StartProcess(pContext, lstArguments))
    {
        if (pContext->pProcess) { pContext->pProcess->disconnect(this); }
        FinishContext(_strTaskId, 1, false);
        return false;
    }
    return true;
}

bool CrocAdapter::StartReceive(const QString& _strTaskId, const QString& _strCode, const QString& _strOutDir)  // 启动接收任务
{
    if ((ECrocBackend::BACKEND_DLL == m_enBackend) && (nullptr != m_pDllSession))
    {
        return m_pDllSession->StartReceive(_strTaskId, _strCode, _strOutDir, m_stConfig, m_hashNextConfig);
    }
    if (_strCode.isEmpty())
    {
        return false;
    }
    if (m_hashContexts.contains(_strTaskId))
    {
        return false;
    }

    SProcessContext* pContext = new SProcessContext();
    pContext->stSnapshot.strTaskId = _strTaskId;
    pContext->stSnapshot.enDirection = ETaskDirection::DIRECTION_RECEIVE;
    pContext->stConfig = m_stConfig;
    pContext->stSnapshot.hashConfig = m_hashNextConfig;
    pContext->stSnapshot.strTargetDir = QDir(_strOutDir).canonicalPath();
    pContext->stSnapshot.strCode = _strCode;
    m_hashContexts.insert(_strTaskId, pContext);
    SetStage(pContext, ETaskStage::STAGE_STARTING);

    QStringList lstArguments = BuildCommonArguments(true);
    lstArguments << QStringLiteral("--ask");
    lstArguments << QStringLiteral("--out") << _strOutDir;
    lstArguments << _strCode;

    if (!StartProcess(pContext, lstArguments))
    {
        if (pContext->pProcess) { pContext->pProcess->disconnect(this); }
        FinishContext(_strTaskId, 1, false);
        return false;
    }
    return true;
}

void CrocAdapter::AnswerPrompt(const QString& _strTaskId, bool _bAccept)  // 回答交互提问
{
    SProcessContext* pContext = m_hashContexts.value(_strTaskId, nullptr);
    if ((nullptr == pContext) || (nullptr == pContext->pProcess))
    {
        return;
    }
    if (QProcess::Running != pContext->pProcess->state())
    {
        return;
    }
    pContext->bRejected = pContext->bRejected || !_bAccept;
    const QByteArray arrAnswer = _bAccept ? QByteArrayLiteral("y\n") : QByteArrayLiteral("n\n");
    pContext->pProcess->write(arrAnswer);
    // 由事件循环异步写入，避免等待期间 finished 重入释放当前上下文。
}

void CrocAdapter::Cancel(const QString& _strTaskId)  // 取消任务
{
    if ((ECrocBackend::BACKEND_DLL == m_enBackend) && (nullptr != m_pDllSession))
    {
        m_pDllSession->Cancel(_strTaskId);
        return;
    }
    SProcessContext* pContext = m_hashContexts.value(_strTaskId, nullptr);
    if ((nullptr == pContext) || (nullptr == pContext->pProcess))
    {
        return;
    }
    if (pContext->bCancelRequested) { return; }
    pContext->bCancelRequested = true;
    SetStage(pContext, ETaskStage::STAGE_CANCELLING);
    pContext->pProcess->terminate();
    QTimer::singleShot(1500, this, [this, _strTaskId]()
    {
        SProcessContext* pCurrent = m_hashContexts.value(_strTaskId, nullptr);
        if (pCurrent && pCurrent->pProcess && pCurrent->pProcess->state() != QProcess::NotRunning) { pCurrent->pProcess->kill(); }
    });
}

bool CrocAdapter::IsRunning(const QString& _strTaskId) const  // 查询任务是否运行中
{
    if (m_enBackend == ECrocBackend::BACKEND_DLL) { return m_pDllSession && m_pDllSession->IsRunning(_strTaskId); }
    SProcessContext* pContext = m_hashContexts.value(_strTaskId, nullptr);
    if ((nullptr == pContext) || (nullptr == pContext->pProcess))
    {
        return false;
    }
    return QProcess::Running == pContext->pProcess->state();
}

bool CrocAdapter::StartProcess(SProcessContext* _pContext, const QStringList& _lstArguments)  // 启动子进程
{
    if (!QFileInfo::exists(m_stConfig.strCrocPath))
    {
        AppendError(_pContext, QStringLiteral("找不到 croc 引擎：") + m_stConfig.strCrocPath);
        SetStage(_pContext, ETaskStage::STAGE_FAILED);
        return false;
    }

    QProcess* pProcess = new QProcess(this);
    _pContext->pProcess = pProcess;
    pProcess->setProgram(m_stConfig.strCrocPath);
    pProcess->setArguments(_lstArguments);
    pProcess->setProcessChannelMode(QProcess::SeparateChannels);
    if (qEnvironmentVariableIsSet("FLOWDROP_ADAPTER_DEBUG"))
    {
        qInfo("[argv] 参数已脱敏，参数数量=%d", int(_lstArguments.size()));
    }

    if (!m_stConfig.strWorkHome.isEmpty())  // 把子进程的 HOME 指到应用目录
    {
        QDir().mkpath(m_stConfig.strWorkHome);
        QProcessEnvironment enEnvironment = QProcessEnvironment::systemEnvironment();
        enEnvironment.insert(QStringLiteral("HOME"), m_stConfig.strWorkHome);
        enEnvironment.insert(QStringLiteral("USERPROFILE"), m_stConfig.strWorkHome);
        // 显式设置优先，避免引擎从父进程环境继承另一组中继/代理/密码。
        for (const QString& strKey : QStringList{QStringLiteral("CROC_RELAY"), QStringLiteral("CROC_RELAY6"), QStringLiteral("CROC_PASS"),
            QStringLiteral("SOCKS5_PROXY"), QStringLiteral("HTTP_PROXY"), QStringLiteral("HTTPS_PROXY"), QStringLiteral("ALL_PROXY"),
            QStringLiteral("http_proxy"), QStringLiteral("https_proxy"), QStringLiteral("all_proxy")}) { enEnvironment.remove(strKey); }
        pProcess->setProcessEnvironment(enEnvironment);
    }

    // 实测：croc 会把「重定向」的 stdin 当成要发送的文件内容，导致静默等待或误发 stdin。
    // 发送任务因此不能给管道 stdin；这里支持三种方式，便于联调时逐项验证。
    if (ETaskDirection::DIRECTION_SEND == _pContext->stSnapshot.enDirection)
    {
        const QString strStdinMode = qEnvironmentVariable("FLOWDROP_STDIN", QStringLiteral("nul"));
        if (QStringLiteral("forward") == strStdinMode)
        {
            pProcess->setInputChannelMode(QProcess::ForwardedInputChannel);  // 继承父进程 stdin
        }
        else if (QStringLiteral("nul") == strStdinMode)
        {
            pProcess->setStandardInputFile(QStringLiteral("\\\\.\\NUL"));  // NUL 设备
        }
        // 其余情况保持默认管道，仅用于对照实验
    }

    const QString strTaskId = _pContext->stSnapshot.strTaskId;
    connect(pProcess, &QProcess::readyReadStandardError, this, [this, strTaskId]()
    {
        SProcessContext* pTarget = m_hashContexts.value(strTaskId, nullptr);
        if (nullptr != pTarget)
        {
            if (qEnvironmentVariableIsSet("FLOWDROP_ADAPTER_DEBUG"))
            {
                qInfo("[signal] readyReadStandardError bytes=%lld", static_cast<long long>(pTarget->pProcess->bytesAvailable()));
            }
            DrainChannels(pTarget);
        }
    });
    connect(pProcess, &QProcess::readyReadStandardOutput, this, [this, strTaskId]()
    {
        SProcessContext* pTarget = m_hashContexts.value(strTaskId, nullptr);
        if (nullptr != pTarget)
        {
            DrainChannels(pTarget);
        }
    });
    connect(pProcess, &QProcess::errorOccurred, this, [this, strTaskId](QProcess::ProcessError _enError)
    {
        SProcessContext* pTarget = m_hashContexts.value(strTaskId, nullptr);
        if (nullptr == pTarget)
        {
            return;
        }
        if (QProcess::FailedToStart == _enError)
        {
            AppendError(pTarget, QStringLiteral("引擎进程启动失败"));
            SetStage(pTarget, ETaskStage::STAGE_FAILED);
            FinishContext(strTaskId, 1, false);
        }
    });
    connect(pProcess, &QProcess::finished, this, [this, strTaskId](int _nExitCode, QProcess::ExitStatus _enStatus)
    {
        SProcessContext* pTarget = m_hashContexts.value(strTaskId, nullptr);
        if (nullptr == pTarget)
        {
            return;
        }
        DrainChannels(pTarget);
        pTarget->bCrashed = (_enStatus != QProcess::NormalExit);
        FlushTail(strTaskId);
        FinishContext(strTaskId, _nExitCode, pTarget->bCancelRequested);
    });

    if (_pContext->stSnapshot.enDirection == ETaskDirection::DIRECTION_SEND)
    {
        QTimer* pWatch = new QTimer(pProcess);
        pWatch->setInterval(1000);
        connect(pWatch, &QTimer::timeout, this, [this, strTaskId]()
        {
            SProcessContext* pTarget = m_hashContexts.value(strTaskId, nullptr);
            if (pTarget && !pTarget->bCancelRequested && !TransferSafety::FilesUnchanged(pTarget->stSnapshot.lstFiles))
            {
                AppendError(pTarget, QStringLiteral("源文件已删除、替换或修改，请重新选择；当前任务已停止。"));
                pTarget->bInternalAbort = true;
                Cancel(strTaskId);
            }
        });
        pWatch->start();
    }
    // 所有连接和监测器先安装，再异步启动；start() 后不访问可能已收尾的上下文。
    _pContext->llStartedMs = QDateTime::currentMSecsSinceEpoch();  // 记录任务启动时间，供接收结果核对兜底
    pProcess->start();
    return true;
}

void CrocAdapter::FlushTail(const QString& _strTaskId)
{
    SProcessContext* pTarget = m_hashContexts.value(_strTaskId, nullptr);
    if (!pTarget) { return; }
    if (!pTarget->strErrBuffer.isEmpty()) { HandleStdErrLine(pTarget, pTarget->strErrBuffer); pTarget->strErrBuffer.clear(); }
    if (!pTarget->strOutBuffer.isEmpty()) { HandleStdOutLine(pTarget, pTarget->strOutBuffer); pTarget->strOutBuffer.clear(); }
}

void CrocAdapter::DrainChannels(SProcessContext* _pContext)  // 读取并分发输出
{
    if ((nullptr == _pContext) || (nullptr == _pContext->pProcess))
    {
        return;
    }

    _pContext->strErrBuffer += _pContext->decoderErr.decode(_pContext->pProcess->readAllStandardError());
    QStringList lstErrLines = _pContext->strErrBuffer.split(QRegularExpression(QStringLiteral("[\\r\\n]")));
    _pContext->strErrBuffer = lstErrLines.takeLast();
    for (const QString& strLine : lstErrLines)
    {
        HandleStdErrLine(_pContext, strLine);
    }

    // 交互提示不带换行，会一直留在未成行缓冲里，必须单独识别一次；重复上报由特征串去重。
    const QString strTail = _pContext->strErrBuffer.trimmed();
    if (!strTail.isEmpty())
    {
        if (GetAcceptRegex().match(strTail).hasMatch() || GetResumeRegex().match(strTail).hasMatch())
        {
            HandleStdErrLine(_pContext, strTail);
        }
    }

    _pContext->strOutBuffer += _pContext->decoderOut.decode(_pContext->pProcess->readAllStandardOutput());
    QStringList lstOutLines = _pContext->strOutBuffer.split(QRegularExpression(QStringLiteral("[\\r\\n]")));
    _pContext->strOutBuffer = lstOutLines.takeLast();
    for (const QString& strLine : lstOutLines)
    {
        HandleStdOutLine(_pContext, strLine);
    }
}

void CrocAdapter::HandleStdErrLine(SProcessContext* _pContext, const QString& _strLine)  // 处理 stderr 一行
{
    const QString strLine = _strLine.trimmed();
    if (strLine.isEmpty())
    {
        return;
    }
    if (qEnvironmentVariableIsSet("FLOWDROP_ADAPTER_DEBUG"))  // 联调时打开：打印收到的原始行
    {
        qInfo("[stderr] 收到状态行，长度=%d", int(strLine.size()));
    }

    if (_pContext->bAwaitCodeLine)  // 上一行是 "On the other computer, run:"，本行是命令示例
    {
        _pContext->bAwaitCodeLine = false;
        const QStringList lstTokens = strLine.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (!lstTokens.isEmpty())
        {
            const QString strCode = lstTokens.last();
            if (strCode.length() >= 6)
            {
                _pContext->stSnapshot.strCode = strCode;
                _pContext->stSnapshot.bShareReady = true;
                SetStage(_pContext, ETaskStage::STAGE_WAITING_PEER);
                emit CodeReady(_pContext->stSnapshot.strTaskId, strCode);
            }
        }
        return;
    }

    if (strLine.startsWith(QStringLiteral("On the other computer")))
    {
        _pContext->bAwaitCodeLine = true;
        return;
    }

    if (_pContext->stSnapshot.strCode.isEmpty() && strLine.contains(QStringLiteral("getcroc.com/?code=")))  // 兜底：从网页地址取口令
    {
        const int nIndex = strLine.indexOf(QStringLiteral("?code=")) + 6;
        QString strCode = strLine.mid(nIndex).trimmed();
        strCode.remove(QRegularExpression(QStringLiteral("[^A-Za-z0-9-]")));
        if (strCode.length() >= 6)
        {
            _pContext->stSnapshot.strCode = strCode;
            _pContext->stSnapshot.bShareReady = true;
                SetStage(_pContext, ETaskStage::STAGE_WAITING_PEER);
                emit CodeReady(_pContext->stSnapshot.strTaskId, strCode);
        }
        return;
    }

    QRegularExpressionMatch stMatch = GetAcceptRegex().match(strLine);  // 接收前确认
    if (stMatch.hasMatch())
    {
        const QString strSignature = stMatch.captured(QStringLiteral("name")) + QLatin1Char('|')
                                     + stMatch.captured(QStringLiteral("size")) + QLatin1Char('|')
                                     + stMatch.captured(QStringLiteral("peer"));
        if (_pContext->strAcceptSignature == strSignature)
        {
            return;  // 同一个确认只上报一次
        }
        _pContext->strAcceptSignature = strSignature;
        QString strName = stMatch.captured(QStringLiteral("name"));  // 去掉可能存在的引号
        if (strName.startsWith(QLatin1Char('\'')) && strName.endsWith(QLatin1Char('\'')) && (strName.length() > 2))
        {
            strName = strName.mid(1, strName.length() - 2);
            _pContext->strExpectedSingleFile = strName;
            _pContext->stSnapshot.nFileCount = 1;
        }
        _pContext->stSnapshot.strDisplayName = strName;
        _pContext->stSnapshot.llTotalBytes = ParseSizeText(stMatch.captured(QStringLiteral("size")));
        _pContext->stSnapshot.llTaskBytes = _pContext->stSnapshot.llTotalBytes;  // 任务总量以确认提示为准
        _pContext->stSnapshot.strPeer = stMatch.captured(QStringLiteral("peer"));
        SetStage(_pContext, ETaskStage::STAGE_CONNECTED);
        emit PromptAccept(_pContext->stSnapshot.strTaskId,
                          _pContext->stSnapshot.strDisplayName,
                          stMatch.captured(QStringLiteral("size")),
                          _pContext->stSnapshot.strPeer);
        return;
    }

    stMatch = GetResumeRegex().match(strLine);  // 续传 / 覆盖确认
    if (stMatch.hasMatch())
    {
        const QString strSignature = stMatch.captured(QStringLiteral("name")) + QLatin1Char('|')
                                     + stMatch.captured(QStringLiteral("pct"));
        if (_pContext->strResumeSignature == strSignature)
        {
            return;
        }
        _pContext->strResumeSignature = strSignature;
        _pContext->stSnapshot.strDisplayName = stMatch.captured(QStringLiteral("name"));
        emit PromptResume(_pContext->stSnapshot.strTaskId,
                          _pContext->stSnapshot.strDisplayName,
                          stMatch.captured(QStringLiteral("pct")).toDouble());
        return;
    }

    if (GetProgressRegex().match(strLine).hasMatch())  // 进度行
    {
        HandleProgress(_pContext, strLine);
        return;
    }

    stMatch = GetFileRegex().match(strLine);  // 文件清单
    if (stMatch.hasMatch())
    {
        _pContext->stSnapshot.strDisplayName = stMatch.captured(QStringLiteral("name"));
        _pContext->stSnapshot.llTotalBytes = ParseSizeText(stMatch.captured(QStringLiteral("size")));
        SetStage(_pContext, ETaskStage::STAGE_CONNECTED);
        return;
    }

    stMatch = GetPeerRegex().match(strLine);  // 连接信息
    if (stMatch.hasMatch())
    {
        _pContext->stSnapshot.strPeer = stMatch.captured(QStringLiteral("peer"));
        SetStage(_pContext, ETaskStage::STAGE_CONNECTED);
        return;
    }

    if (strLine.startsWith(QStringLiteral("connecting")) || strLine.startsWith(QStringLiteral("looking for sender"))
        || strLine.startsWith(QStringLiteral("waiting for sender")))
    {
        SetStage(_pContext, ETaskStage::STAGE_WAITING_PEER);
        return;
    }

    if (strLine.startsWith(QStringLiteral("authenticating code")) || strLine.startsWith(QStringLiteral("opening transfer channels"))
        || strLine.startsWith(QStringLiteral("waiting for file list")))
    {
        SetStage(_pContext, ETaskStage::STAGE_CONNECTED);
        return;
    }

    if (strLine.startsWith(QStringLiteral("Sender detected a transfer interruption")))
    {
        AppendError(_pContext, QStringLiteral("对端中断，引擎正在重试"));
        SetStage(_pContext, ETaskStage::STAGE_WAITING_PEER);
        return;
    }

    if (strLine.startsWith(QStringLiteral("No files transferred")))
    {
        _pContext->bNoFiles = true;
        AppendError(_pContext, QStringLiteral("没有文件被传输，请确认对方接受了文件或检查同名文件处理结果。"));
        return;
    }
}

void CrocAdapter::HandleStdOutLine(SProcessContext* _pContext, const QString& _strLine)  // 处理 stdout 一行
{
    const QString strLine = _strLine.trimmed();
    if (strLine.isEmpty())
    {
        return;
    }
    if (qEnvironmentVariableIsSet("FLOWDROP_ADAPTER_DEBUG"))
    {
        qInfo("[stdout] 收到诊断行，长度=%d", int(strLine.size()));
    }
    if (strLine.startsWith(QStringLiteral("[error]")))
    {
        if (strLine.contains(QStringLiteral(".config")))  // 配置目录不可写不影响传输，只记录不报警
        {
            return;
        }
        AppendError(_pContext, strLine);
        return;
    }
    if (strLine.startsWith(QStringLiteral("GetFileAttributesEx")) || strLine.contains(QStringLiteral("could not connect"))
        || strLine.contains(QStringLiteral("local discovery failed")))
    {
        AppendError(_pContext, strLine);
    }
}

void CrocAdapter::HandleProgress(SProcessContext* _pContext, const QString& _strLine)  // 解析进度行
{
    const QRegularExpressionMatch stMatch = GetProgressRegex().match(_strLine);
    if (!stMatch.hasMatch())
    {
        return;
    }
    QString strName = stMatch.captured(QStringLiteral("name"));
    strName.remove(QRegularExpression(QStringLiteral("^\\d+/\\d+\\s+")));  // 去掉行首的 "n/N " 计数前缀
    // 接收完成后引擎会另起一行打印 Hashing <文件> x%，那是校验而不是传输
    const bool bHashing = strName.startsWith(QStringLiteral("Hashing "), Qt::CaseInsensitive);
    if (bHashing) { strName = strName.mid(QStringLiteral("Hashing ").length()).trimmed(); }
    _pContext->stSnapshot.strDisplayName = strName;
    if (_pContext->stSnapshot.enDirection == ETaskDirection::DIRECTION_RECEIVE) { _pContext->setReceivedNames.insert(strName); }
    _pContext->bSawProgress = true;
    _pContext->stSnapshot.dPercent = stMatch.captured(QStringLiteral("pct")).toDouble();

    const QString strTotalUnit = stMatch.captured(QStringLiteral("tu"));  // 总量单位
    const QString strDoneUnit = stMatch.captured(QStringLiteral("du"));   // 已传单位，可能缺省
    _pContext->stSnapshot.llTotalBytes = ParseSizeText(stMatch.captured(QStringLiteral("total")) + QLatin1Char(' ')
                                                       + strTotalUnit);
    _pContext->stSnapshot.llDoneBytes = ParseSizeText(stMatch.captured(QStringLiteral("done")) + QLatin1Char(' ')
                                                      + (strDoneUnit.isEmpty() ? strTotalUnit : strDoneUnit));

    const QString strSpeed = stMatch.captured(QStringLiteral("speed"));  // 速度可能缺省
    if (strSpeed.isEmpty())
    {
        _pContext->stSnapshot.strSpeed.clear();
    }
    else
    {
        _pContext->stSnapshot.strSpeed = strSpeed + QLatin1Char(' ') + stMatch.captured(QStringLiteral("su"))
                                         + QStringLiteral("/s");
    }
    _pContext->stSnapshot.strEta = stMatch.captured(QStringLiteral("eta"));
    const QString strFileIndex = stMatch.captured(QStringLiteral("fidx"));  // 多文件计数，可能缺省
    if (!strFileIndex.isEmpty())
    {
        _pContext->stSnapshot.nFileIndex = strFileIndex.toInt();
        _pContext->stSnapshot.nFileCount = stMatch.captured(QStringLiteral("fcnt")).toInt();
    }
    if (bHashing)
    {
        SetStage(_pContext, ETaskStage::STAGE_VERIFYING);
    }
    else if ((_pContext->stSnapshot.enDirection == ETaskDirection::DIRECTION_SEND)
             && (_pContext->stSnapshot.dPercent >= 100.0))
    {
        SetStage(_pContext, ETaskStage::STAGE_AWAITING_CONFIRM);  // 已发完，等待对方确认
    }
    else
    {
        SetStage(_pContext, ETaskStage::STAGE_TRANSFERRING);
    }
}

void CrocAdapter::SetStage(SProcessContext* _pContext, ETaskStage _enStage)  // 更新阶段并上报
{
    if (nullptr == _pContext)
    {
        return;
    }
    if (_pContext->stSnapshot.enStage != _enStage)
    {
        _pContext->stSnapshot.enStage = _enStage;
    }
    emit TaskUpdated(_pContext->stSnapshot);
}

void CrocAdapter::AppendError(SProcessContext* _pContext, const QString& _strError)  // 追加错误说明
{
    if (nullptr == _pContext)
    {
        return;
    }
    if (_pContext->stSnapshot.strError.isEmpty())
    {
        _pContext->stSnapshot.strError = TransferSafety::Redact(_strError, {_pContext->stSnapshot.strCode, _pContext->stConfig.strRelayPass});
    }
    else if (!_pContext->stSnapshot.strError.contains(_strError))
    {
        _pContext->stSnapshot.strError += QStringLiteral("；") + TransferSafety::Redact(_strError, {_pContext->stSnapshot.strCode, _pContext->stConfig.strRelayPass});
        _pContext->stSnapshot.strError = _pContext->stSnapshot.strError.left(4096);
    }
}

void CrocAdapter::FinishContext(const QString& _strTaskId, int _nExitCode, bool _bCancelled)  // 收尾并上报
{
    SProcessContext* pContext = m_hashContexts.take(_strTaskId);
    if (nullptr == pContext)
    {
        return;
    }

    pContext->stSnapshot.nExitCode = _nExitCode;
    pContext->stSnapshot.bTerminal = true;
    pContext->stSnapshot.bShareReady = false;
    pContext->stSnapshot.strCode.clear();
    pContext->stSnapshot.strSpeed.clear();
    pContext->stSnapshot.strEta.clear();
    if (pContext->stSnapshot.enDirection == ETaskDirection::DIRECTION_SEND && pContext->stSnapshot.nFileCount == 1
        && !pContext->stSnapshot.lstFiles.isEmpty())
    {
        pContext->stSnapshot.strDisplayName = pContext->stSnapshot.lstFiles.first().toMap().value(QStringLiteral("name")).toString();
    }
    if (pContext->bInternalAbort) { _bCancelled = false; }
    if (_bCancelled) { pContext->stSnapshot.enStage = ETaskStage::STAGE_CANCELLED; }
    else if (_nExitCode != 0 || pContext->bCrashed || pContext->bInternalAbort)
    {
        pContext->stSnapshot.enStage = ETaskStage::STAGE_FAILED;
        if (pContext->stSnapshot.strError.isEmpty()) { pContext->stSnapshot.strError = QStringLiteral("引擎异常结束，退出码 %1").arg(_nExitCode); }
        pContext->stSnapshot.strError = TransferSafety::ExplainError(pContext->stSnapshot.strError)
            + QLatin1Char('\n') + pContext->stSnapshot.strError;
    }
    else if (pContext->bRejected || pContext->bNoFiles)
    {
        pContext->stSnapshot.enStage = ETaskStage::STAGE_SKIPPED;
        pContext->stSnapshot.strError = QStringLiteral("未接收或已跳过，请查看目标文件后再决定是否重试。");
    }
    else if (!pContext->stSnapshot.strError.isEmpty())
    { pContext->stSnapshot.enStage = ETaskStage::STAGE_FAILED; }
    else if (pContext->stSnapshot.enDirection == ETaskDirection::DIRECTION_SEND)
    {
        // 引擎退出码为 0 即代表本端已把所有数据交给引擎；此时不应再停在某个中间百分比。
        pContext->stSnapshot.enStage = ETaskStage::STAGE_SENT;
        pContext->stSnapshot.dPercent = 100;
    }
    else
    {
        // 引擎退出码为 0 就代表引擎侧已完成。是否解析到进度行只影响进度显示，
        // 不能因为“没看到进度”或“显示名被缩写”就把已经落盘的文件判成待检查。
        pContext->stSnapshot.enStage = ETaskStage::STAGE_COMPLETED;
        pContext->stSnapshot.dPercent = 100;
    }

    if (pContext->stSnapshot.enDirection == ETaskDirection::DIRECTION_RECEIVE
        && pContext->stSnapshot.enStage == ETaskStage::STAGE_COMPLETED)
    {
        // 核对策略：先按名称（单文件名/进度显示名）找，找不到再用“任务开始后写入的文件”兜底。
        // 这样兼容：整个文件夹接收（文件在子目录）、长名被缩写，以及“已是最新”没有进度行的情况。
        const bool bExactSingleName = !pContext->strExpectedSingleFile.isEmpty() && !pContext->stConfig.bRenameOnConflict;
        QSet<QString> setCandidates = pContext->setReceivedNames;
        if (bExactSingleName) { setCandidates = {pContext->strExpectedSingleFile}; }

        const QString strRoot = QDir(pContext->stSnapshot.strTargetDir).canonicalPath();
        QVariantList lstReceived;
        qint64 llBytes = 0;

        // 收集一个路径下的文件；若是目录则递归，兼容“发送整个文件夹”
        const auto fnCollect = [&](const QFileInfo& _stInfo)
        {
            if (!_stInfo.exists() || _stInfo.isSymLink()) { return; }
            if (_stInfo.isDir())
            {
                QDirIterator itDir(_stInfo.canonicalFilePath(), QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
                int nGuard = 0;
                while (itDir.hasNext() && (nGuard++ < 20000))
                {
                    const QFileInfo stChild(itDir.next());
                    const QString strPath = stChild.canonicalFilePath();
                    if (strPath.isEmpty()) { continue; }
                    if (!strRoot.isEmpty() && !strPath.startsWith(strRoot + QLatin1Char('/'), Qt::CaseInsensitive)) { continue; }
                    QVariantMap hashFile;
                    hashFile.insert(QStringLiteral("path"), strPath);
                    hashFile.insert(QStringLiteral("size"), stChild.size());
                    lstReceived.append(hashFile);
                    llBytes += stChild.size();
                }
                return;
            }
            const QString strPath = _stInfo.canonicalFilePath();
            if (strPath.isEmpty()) { return; }
            if (!strRoot.isEmpty() && !strPath.startsWith(strRoot + QLatin1Char('/'), Qt::CaseInsensitive)) { return; }
            QVariantMap hashFile;
            hashFile.insert(QStringLiteral("path"), strPath);
            hashFile.insert(QStringLiteral("size"), _stInfo.size());
            lstReceived.append(hashFile);
            llBytes += _stInfo.size();
        };

        for (const QString& strName : setCandidates)
        {
            if (strName.isEmpty() || QFileInfo(strName).isAbsolute()
                || QDir::cleanPath(strName).startsWith(QStringLiteral("../"))) { continue; }
            fnCollect(QFileInfo(QDir(strRoot).filePath(strName)));
        }

        if (lstReceived.isEmpty() && !strRoot.isEmpty())
        {
            QDirIterator itRoot(strRoot, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
            int nGuard = 0;
            while (itRoot.hasNext() && (nGuard++ < 20000))
            {
                const QFileInfo stChild(itRoot.next());
                if (stChild.lastModified().toMSecsSinceEpoch() + 2000 < pContext->llStartedMs) { continue; }
                QVariantMap hashFile;
                hashFile.insert(QStringLiteral("path"), stChild.canonicalFilePath());
                hashFile.insert(QStringLiteral("size"), stChild.size());
                lstReceived.append(hashFile);
                llBytes += stChild.size();
            }
        }

        pContext->stSnapshot.lstFiles = lstReceived;
        if (!lstReceived.isEmpty()
            && (bExactSingleName
                || (pContext->stSnapshot.nFileCount > 0 && lstReceived.size() == pContext->stSnapshot.nFileCount)))
        {
            pContext->stSnapshot.llTaskBytes = llBytes;
            pContext->stSnapshot.bSizeExact = true;
        }
        if (bExactSingleName) { pContext->stSnapshot.strDisplayName = pContext->strExpectedSingleFile; }
        if (lstReceived.isEmpty())
        {
            pContext->stSnapshot.enStage = ETaskStage::STAGE_UNCONFIRMED;
            pContext->stSnapshot.strError = QStringLiteral("引擎已确认接收结束，但在保存目录里没有找到本次任务的文件；请检查保存位置，或文件是否已被移动。");
        }
    }
    emit TaskUpdated(pContext->stSnapshot);
    emit TaskFinished(_strTaskId, _nExitCode, _bCancelled, pContext->stSnapshot.strError);

    if (nullptr != pContext->pProcess)
    {
        pContext->pProcess->deleteLater();
    }
    delete pContext;
}

QStringList CrocAdapter::BuildCommonArguments(bool _bInteractive) const  // 组装公共参数
{
    QStringList lstArguments;
    if (!m_stConfig.strRelay.isEmpty())
    {
        lstArguments << QStringLiteral("--relay") << m_stConfig.strRelay;
    }
    if (!m_stConfig.strRelayPass.isEmpty())
    {
        lstArguments << QStringLiteral("--pass") << m_stConfig.strRelayPass;
    }
    if (!m_stConfig.strCurve.isEmpty())
    {
        lstArguments << QStringLiteral("--curve") << m_stConfig.strCurve;
    }
    if (m_stConfig.bNoCompress)
    {
        lstArguments << QStringLiteral("--no-compress");
    }
    if (m_stConfig.bLocalOnly)  // 仅局域网，不偷偷转公网
    {
        lstArguments << QStringLiteral("--local");
    }
    if (!m_stConfig.strSocks5.isEmpty())
    {
        lstArguments << QStringLiteral("--socks5") << m_stConfig.strSocks5;
    }
    if (!m_stConfig.strHttpProxy.isEmpty())
    {
        lstArguments << QStringLiteral("--connect") << m_stConfig.strHttpProxy;
    }
    if (m_stConfig.dUploadLimitMbps > 0.0)  // 限速单位是 MB/s，引擎接受 "4m" 这种写法
    {
        lstArguments << QStringLiteral("--throttleUpload")
                     << (QString::number(m_stConfig.dUploadLimitMbps, 'f', 1) + QStringLiteral("m"));
    }
    if (_bInteractive && m_stConfig.bRenameOnConflict)  // 同名保留两份
    {
        lstArguments << QStringLiteral("--rename");
    }
    if (!_bInteractive)  // 接收端要用 --ask 自己回答，不能同时给 --yes
    {
        lstArguments << QStringLiteral("--yes");
    }
    lstArguments << QStringLiteral("--disable-clipboard");
    return lstArguments;
}

bool CrocAdapter::AnswerDllPrompt(const QString& _strTaskId, const QString& _strPromptId, int _nDecision)
{
    return m_pDllSession && m_pDllSession->AnswerPrompt(_strTaskId, _strPromptId, _nDecision);
}
