/**
****************************************************************
* @file    AppController.cpp
* @brief   应用控制器实现
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
*          V1.0.1       Ray       2026-11-01    2. 接入 CrocAdapter 与任务模型
****************************************************************
*/
#include "AppController.h"  // 包含控制器头文件

#include "TransferSafety.h"
#include <QUuid>
#include <QTimer>
#include <QTemporaryFile>
#include <QDesktopServices>
#include <QStorageInfo>
#include <QUrl>
#include <QRegularExpression>
#include <QClipboard>  // 复制口令
#include <QCoreApplication>  // 应用目录
#include <QDateTime>  // 历史时间
#include <QDir>  // 目录操作
#include <QFileInfo>  // 引擎路径检查
#include <QGuiApplication>  // 剪贴板
#include <QProcess>  // 读取引擎版本
#include <QStandardPaths>  // 用户数据目录

#include "CrocAdapter.h"
#include "CrocLibrary.h"  // 库模式引擎状态  // 引擎适配层
#include "TaskFilterModel.h"  // 方向过滤模型
#include "TransferTaskModel.h"  // 任务模型
#ifdef Q_OS_ANDROID
#include "AndroidFiles.h"
#include <QFutureWatcher>
#include <QtConcurrentRun>
#endif

namespace
{
/**
****************************************************************
* @brief 读取引擎版本号
* @param [IN] _strCrocPath 引擎路径
* @return 版本文本；失败返回空字符串
* @others 只在启动时调用一次，用于界面显示真实版本
* @author Ray
****************************************************************
*/
QString ReadCrocVersion(const QString& _strCrocPath)
{
    QProcess procVersion;
    procVersion.setProgram(_strCrocPath);
    procVersion.setArguments(QStringList() << QStringLiteral("--version"));
    procVersion.start();
    if (!procVersion.waitForStarted(2000))
    {
        return QString();
    }
    if (!procVersion.waitForFinished(3000))
    {
        procVersion.kill();
        return QString();
    }
    const QString strStdOut = QString::fromUtf8(procVersion.readAllStandardOutput()).trimmed();
    if (!strStdOut.isEmpty())
    {
        return strStdOut;
    }
    return QString::fromUtf8(procVersion.readAllStandardError()).trimmed();
}
}

AppController::AppController(QObject* _pParent)  // 构造控制器并准备模型
    : QObject(_pParent)
{
    m_pTaskModel = new TransferTaskModel(this);
    m_pSendTasks = new TaskFilterModel(this);
    m_pSendTasks->setSourceModel(m_pTaskModel);
    m_pSendTasks->SetDirection(QStringLiteral("send"));
    m_pReceiveTasks = new TaskFilterModel(this);
    m_pReceiveTasks->setSourceModel(m_pTaskModel);
    m_pReceiveTasks->SetDirection(QStringLiteral("receive"));
    m_pHistoryModel = new HistoryModel(this);
    m_pHistoryModel->Load();
    m_pConfigStore = new ConfigStore(this);
    const bool bConfigLoaded = m_pConfigStore->Load();
    m_bConfigLoaded = bConfigLoaded;
    connect(m_pHistoryModel, &HistoryModel::storageError, this, &AppController::ShowError);
    PrepareEngine();
    InitializeTaskQueue();
    if (!bConfigLoaded) { QTimer::singleShot(0, this, [this]() { ShowError(QStringLiteral("配置读取或敏感字段迁移失败，请检查应用数据目录权限。")); }); }
}

AppController::~AppController()
{
    m_bQueueClosing = true;
    if (m_pQueueTimer) { m_pQueueTimer->stop(); }
    if (m_pJournalTimer) { m_pJournalTimer->stop(); }
    if (m_pAdapter && m_bJournalWritable)
    {
        // Windows 写入句柄关闭后才稳定报告最终时间，退出时保存这份快照。
        for (const STaskSnapshot& stFinal : m_pAdapter->StopAndSnapshotReceives()) { ApplyManagedSnapshot(stFinal); }
    }
    PersistTaskJournal();
    // QObject 将按创建顺序销毁模型，必须在此前停止适配层，防止回调访问已销毁模型。
    if (m_pAdapter) { m_pAdapter->disconnect(this); m_pAdapter->Shutdown(); delete m_pAdapter; m_pAdapter = nullptr; }
    delete m_pJournalLock; m_pJournalLock = nullptr;
}

QString AppController::GetEngineStatus() const  // 返回引擎状态
{
    return m_strEngineStatus;
}

TransferTaskModel* AppController::GetTaskModel() const  // 全部任务模型
{
    return m_pTaskModel;
}

TaskFilterModel* AppController::GetSendTasks() const  // 发送任务模型
{
    return m_pSendTasks;
}

TaskFilterModel* AppController::GetReceiveTasks() const  // 接收任务模型
{
    return m_pReceiveTasks;
}

HistoryModel* AppController::GetHistory() const  // 历史记录模型
{
    return m_pHistoryModel;
}

int AppController::GetTaskRevision() const  // 任务变更计数
{
    return m_nTaskRevision;
}

QString AppController::GetShareCode() const  // 最近口令
{
    return m_strShareCode;
}

bool AppController::GetPromptVisible() const  // 确认弹窗状态
{
    return m_bPromptVisible;
}

QString AppController::GetPromptTitle() const  // 弹窗标题
{
    return m_strPromptTitle;
}

QString AppController::GetPromptBody() const  // 弹窗内容
{
    return m_strPromptBody;
}

QString AppController::GetLastEventText() const  // 最近事件文本
{
    return m_strLastEventText;
}

QVariantMap AppController::GetSettings() const  // 当前设置
{
    return (nullptr != m_pConfigStore) ? m_pConfigStore->GetValues() : QVariantMap();
}

QString AppController::GetDefaultSaveDir() const  // 默认接收目录
{
    return (nullptr != m_pConfigStore) ? m_pConfigStore->GetDefaultSaveDir() : QString();
}

QString AppController::GetLanguage() const  // 界面语言
{
    return (nullptr != m_pConfigStore) ? m_pConfigStore->GetLanguage() : QStringLiteral("zh");
}

QString AppController::GetAppVersion() const  // 应用版本
{
    return QStringLiteral("Croc-DawnUI 0.11（验收修复版）");
}

QString AppController::GetEngineVersion() const  // 引擎版本
{
    return m_strEngineVersion.isEmpty() ? QStringLiteral("未检测到引擎") : m_strEngineVersion;
}

void AppController::resetSettings()  // 恢复出厂设置
{
    if (nullptr == m_pConfigStore)
    {
        return;
    }
    if (saveSettings(ConfigStore().GetValues())) { ShowError(QStringLiteral("已恢复默认设置；不会删除文件或历史。")); }
}

bool AppController::saveSettings(const QVariantMap& _hashValues)  // 保存设置并热更新引擎配置
{
    if (!m_pConfigStore) { return false; }
    const QString strError = ConfigStore::Validate(_hashValues);
    if (!strError.isEmpty()) { ShowError(strError); return false; }
    const QVariantMap hashPrevious = m_pConfigStore->GetValues();
    m_pConfigStore->SetValues(_hashValues);
    if (!m_pConfigStore->Save())
    {
        m_pConfigStore->SetValues(hashPrevious);
        ShowError(QStringLiteral("设置未保存：请检查应用数据目录权限和磁盘空间。原设置仍然有效。"));
        return false;
    }
    m_bConfigLoaded = true;
    m_stConfig = m_pConfigStore->ToCrocConfig(m_strCrocPath, m_stConfig.strWorkHome);
    ApplyEnvOverrides();
    if (m_pAdapter) { m_pAdapter->SetConfig(m_stConfig); }
    emit settingsChanged();
    ShowError(QStringLiteral("设置已保存，仅影响新任务；中继密码仅保留当前会话。"));
    return true;
}

void AppController::PrepareEngine()  // 定位引擎并建立适配层
{
    m_strCrocPath = ResolveCrocPath();
    const bool bDllBackend = (ECrocBackend::BACKEND_DLL == ResolveCrocBackend(qEnvironmentVariable("FLOWDROP_ENGINE_BACKEND")));

    QString strWorkHome = qEnvironmentVariable("FLOWDROP_ENGINE_HOME");
    if (strWorkHome.isEmpty())
    {
        strWorkHome = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                      + QStringLiteral("/engine-home");  // 避免写用户主目录的 .config
    }
    m_stConfig = m_pConfigStore->ToCrocConfig(m_strCrocPath, strWorkHome);  // 设置来自配置文件
    ApplyEnvOverrides();

    if (!bDllBackend && !QFileInfo::exists(m_strCrocPath))
    {
        m_strEngineStatus = QStringLiteral("未找到 Croc 引擎");
        return;
    }

    m_bDllBackend = bDllBackend;
    m_pAdapter = new CrocAdapter(m_stConfig, this);
    m_pAdapter->SetBackend(ResolveCrocBackend(qEnvironmentVariable("FLOWDROP_ENGINE_BACKEND")));  // 默认 process，可用环境变量切库
    connect(m_pAdapter, &CrocAdapter::TaskUpdated, this, [this](const STaskSnapshot& _stSnapshot)
    {
        if (m_hashManagedTasks.contains(_stSnapshot.strTaskId)) { ApplyManagedSnapshot(_stSnapshot); return; }
        m_pTaskModel->ApplySnapshot(_stSnapshot);
        ++m_nTaskRevision;
        emit taskRevisionChanged();
    });
    connect(m_pAdapter, &CrocAdapter::CodeReady, this, [this](const QString& _strTaskId, const QString& _strCode)
    {
        if (m_hashManagedTasks.contains(_strTaskId)) { m_hashManagedTasks[_strTaskId].m_strCode = _strCode; }
        m_strShareCode = _strCode;
        emit shareCodeChanged();
        m_strLastEventText = QStringLiteral("发送连接已就绪，请到对应任务查看或复制口令。");
        emit lastEventTextChanged();
        emit codeReady(_strTaskId, _strCode);
    });
    connect(m_pAdapter, &CrocAdapter::DllPromptRequested, this,
        [this](const QString& _strTaskId, const QString& _strPromptId, const QString& _strKind, const QVariantMap& _hashDetails)
    {
        auto itManaged = m_hashManagedTasks.find(_strTaskId);
        // 收到引擎确认意味着会话已连上，等待用户或队列期间不再受旧重连截止时间约束。
        if (itManaged != m_hashManagedTasks.end()) { itManaged->m_llRetryDeadline = 0; }
        if (_strKind == QStringLiteral("send_approval") && itManaged != m_hashManagedTasks.end()) {
            SManagedTask& stManaged = m_hashManagedTasks[_strTaskId];
            stManaged.m_strGatePromptId = _strPromptId; stManaged.m_nGateDecision = 1;
            TryGrantTransfers(); return;
        }
        const QVariantMap hashFile = _hashDetails.value(QStringLiteral("file")).toMap();
        SPendingPrompt stPrompt;
        stPrompt.strId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        stPrompt.strTaskId = _strTaskId;
        stPrompt.m_strEnginePromptId = _strPromptId;
        stPrompt.m_strKind = _strKind;
        stPrompt.m_bCanResume = _strKind == QStringLiteral("overwrite") && _hashDetails.value(QStringLiteral("resume_percent")).toDouble() > 0;
        QStringList lstPreview;
        const QVariantList lstIncoming = _hashDetails.value(QStringLiteral("files")).toList();
        for (const QVariant& varFile : lstIncoming) {
            if (lstPreview.size() == 5) { break; }
            lstPreview.append(varFile.toMap().value(QStringLiteral("name")).toString());
        }
        if (lstIncoming.size() > 5) { lstPreview.append(QStringLiteral("另有 %1 个文件").arg(lstIncoming.size() - 5)); }
        const QString strFileName = hashFile.value(QStringLiteral("name")).toString();
        const QString strPreview = strFileName.isEmpty() ? lstPreview.join(QLatin1Char('\n')) : strFileName;
        stPrompt.strTitle = _strKind == QStringLiteral("receive_accept") ? QStringLiteral("接收确认") : QStringLiteral("同名文件操作确认");
        stPrompt.strBody = QStringLiteral("任务 %1\n文件：%2\n数量：%3，大小：%4 字节\n保存目录：%5\n目标：%6\n来源地址：%7（不代表身份认证）\n%8")
            .arg(_strTaskId, strPreview)
            .arg(_hashDetails.value(QStringLiteral("files_count")).toLongLong())
            .arg(_hashDetails.value(QStringLiteral("bytes_total")).toLongLong())
            .arg(m_pTaskModel->Snapshot(_strTaskId).strTargetDir, hashFile.value(QStringLiteral("destination")).toString())
            .arg(_hashDetails.value(QStringLiteral("peer_address")).toString(), _strKind == QStringLiteral("receive_accept")
                ? QStringLiteral("同意后进入接收队列；同名文件将单独询问。") : QStringLiteral("覆盖或续传会改写目标文件；不确定时请跳过或拒绝。"));
        m_lstPrompts.append(stPrompt);
        ShowNextPrompt();
        // 兼容原有自检信号；实际回答仍验证当前界面提示ID和库提示ID。
        if (_strKind == QStringLiteral("receive_accept")) { emit receivePrompt(_strTaskId, hashFile.value(QStringLiteral("name")).toString(), QString::number(_hashDetails.value(QStringLiteral("bytes_total")).toLongLong()), QString()); }
    });
    connect(m_pAdapter, &CrocAdapter::PromptAccept, this, [this](const QString& _strTaskId, const QString& _strName,
                                                                 const QString& _strSizeText, const QString& _strPeerId)
    {
        const STaskSnapshot stTask = m_pTaskModel->Snapshot(_strTaskId);
        QueuePrompt(_strTaskId, QStringLiteral("接收确认"),
            QStringLiteral("任务 %1\n%2（%3）\n来源标识：%4\n保存目录：%5\n\n地址不代表身份认证。接受后开始接收，不自动解压。")
                .arg(_strTaskId, _strName, _strSizeText, _strPeerId, stTask.strTargetDir));
        emit receivePrompt(_strTaskId, _strName, _strSizeText, _strPeerId);
    });
    connect(m_pAdapter, &CrocAdapter::PromptResume, this, [this](const QString& _strTaskId, const QString& _strName, double _dPercent)
    {
        const STaskSnapshot stTask = m_pTaskModel->Snapshot(_strTaskId);
        QueuePrompt(_strTaskId, QStringLiteral("确认复用或覆盖同名文件"),
            QStringLiteral("任务 %1\n%2：引擎报告可复用 %3%\n目录：%4\n\n接受会改写现有文件；拒绝将跳过。无法确认是否同一文件时，请拒绝并改用保留两份。").arg(_strTaskId, _strName).arg(_dPercent, 0, 'f', 1).arg(stTask.strTargetDir));
        emit resumePrompt(_strTaskId, _strName, _dPercent);
    });
    connect(m_pAdapter, &CrocAdapter::TaskFinished, this, [this](const QString& _strTaskId, int _nExitCode,
                                                                 bool _bCancelled, const QString& _strError)
    {
        if (HandleManagedFinish(_strTaskId, _nExitCode, _bCancelled, _strError)) { return; }
        CompleteTask(_strTaskId, _nExitCode, _bCancelled, _strError);
    });

    if (bDllBackend)
    {
        // 库模式：不依赖 croc.exe，版本与就绪状态都来自 C ABI
        CrocLibrary& libEngine = CrocLibrary::Instance();
        if (libEngine.Load() && libEngine.IsApiCompatible() && (0 == libEngine.Initialize()))
        {
            m_strEngineVersion = libEngine.GetEngineVersion();
            m_strEngineStatus = QStringLiteral("Croc 引擎已就绪 · ") + m_strEngineVersion;
        }
        else
        {
            m_strEngineStatus = QStringLiteral("引擎库加载失败：") + libEngine.GetLastErrorText();
        }
        return;
    }
    const QString strDetectedVersion = ReadCrocVersion(m_strCrocPath);  // 显示真实版本
    const QString strVersion = strDetectedVersion.startsWith(QStringLiteral("croc version ")) ? strDetectedVersion : QString();
    m_strEngineVersion = strVersion;
    m_strEngineStatus = strVersion.isEmpty() ? QStringLiteral("Croc 引擎检测失败，请检查版本与运行环境")
                                             : QStringLiteral("Croc 引擎已就绪 · ") + strVersion;
}

void AppController::ApplyEnvOverrides()  // 联调用环境变量覆盖设置
{
    const QString strEnvRelay = qEnvironmentVariable("FLOWDROP_RELAY");
    if (!strEnvRelay.isEmpty())
    {
        m_stConfig.strRelay = strEnvRelay;
    }
    const QString strEnvPass = qEnvironmentVariable("FLOWDROP_RELAY_PASS");
    if (!strEnvPass.isEmpty())
    {
        m_stConfig.strRelayPass = strEnvPass;
    }
    const QString strEnvCurve = qEnvironmentVariable("FLOWDROP_CURVE");
    if (!strEnvCurve.isEmpty())
    {
        m_stConfig.strCurve = strEnvCurve;
    }
}

QString AppController::ResolveCrocPath() const  // 解析引擎路径
{
    const QString strFromEnv = qEnvironmentVariable("FLOWDROP_CROC_PATH");  // 联调用途
    if (!strFromEnv.isEmpty())
    {
        return strFromEnv;
    }

    const QString strAppDir = QCoreApplication::applicationDirPath();
    const QString strDeployed = strAppDir + QStringLiteral("/croc.exe");  // 随包分发的位置
    if (QFileInfo::exists(strDeployed))
    {
        return strDeployed;
    }
    return QDir::cleanPath(strAppDir + QStringLiteral("/../../../engine/croc.exe"));  // 开发目录
}

QString AppController::startSend(const QStringList& _lstPaths, const QString& _strCode)  // 启动发送任务
{
    const QString strStartError = ValidateStart();
    if (!strStartError.isEmpty()) { ShowError(strStartError); return QString(); }
    QStringList lstPaths;
    for (const QString& strInput : _lstPaths)
    {
        const QString strPath = localPath(strInput);
        if (strPath.isEmpty()) { ShowError(QStringLiteral("仅支持本地文件路径，请重新选择文件。")); return QString(); }
        lstPaths.append(strPath);
    }
    const QVariantMap hashManifest = TransferSafety::InspectFiles(lstPaths);
    const QString strError = hashManifest.value(QStringLiteral("error")).toString();
    if (!strError.isEmpty()) { ShowError(strError); return QString(); }
    if (!_strCode.isEmpty() && !QRegularExpression(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9-]{5,127}$")).match(_strCode).hasMatch())
    { ShowError(QStringLiteral("自定义口令须为 6–128 个英文字母、数字或短横线；双方需使用兼容配置。")); return QString(); }
    const QStringList lstCanonical = hashManifest.value(QStringLiteral("paths")).toStringList();
    if (m_bDllBackend) {
        STaskSnapshot stTask; stTask.enDirection = ETaskDirection::DIRECTION_SEND;
        stTask.strTaskId = QStringLiteral("send-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        stTask.lstSourcePaths = lstCanonical; stTask.lstFiles = hashManifest.value(QStringLiteral("files")).toList();
        stTask.strDisplayName = QFileInfo(lstCanonical.first()).fileName(); stTask.nFileCount = stTask.lstFiles.size();
        stTask.llTaskBytes = hashManifest.value(QStringLiteral("bytes")).toLongLong(); stTask.bSizeExact = true;
        return EnqueueTask(stTask, _strCode);
    }
    int nActive = 0;
    for (int nRow = 0; nRow < m_pTaskModel->rowCount(); ++nRow)
    {
        const QString strExistingId = m_pTaskModel->data(m_pTaskModel->index(nRow, 0), TransferTaskModel::ROLE_TASK_ID).toString();
        const STaskSnapshot stTask = m_pTaskModel->Snapshot(strExistingId);
        if (stTask.bTerminal) { continue; }
        ++nActive;
        if (stTask.enDirection == ETaskDirection::DIRECTION_SEND && stTask.lstSourcePaths == lstCanonical)
        { ShowError(QStringLiteral("同一文件清单已有活动发送任务，请返回首页查看，避免重复发送。")); return QString(); }
    }
    if (nActive >= 16) { ShowError(QStringLiteral("活动与等待任务已达 16 项，请先完成或取消部分任务。")); return QString(); }
    const QString strTaskId = QStringLiteral("send-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_pAdapter->SetNextConfigSnapshot(m_pConfigStore->GetValues());
    if (!m_pAdapter->StartSend(strTaskId, lstCanonical, _strCode)) { ShowError(m_pAdapter->GetLastStartError().isEmpty() ? QStringLiteral("发送未能启动，请检查引擎与源文件后重试。") : m_pAdapter->GetLastStartError()); return QString(); }
    return strTaskId;
}

QString AppController::startReceive(const QString& _strCode, const QString& _strOutDir)  // 启动接收任务
{
    const QString strStartError = ValidateStart();
    if (!strStartError.isEmpty()) { ShowError(strStartError); return QString(); }
    if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9-]{5,127}$")).match(_strCode).hasMatch()
        || QStringList{QStringLiteral("update"), QStringLiteral("upgrade"), QStringLiteral("version")}.contains(_strCode.toLower()))
    { ShowError(QStringLiteral("请输入有效口令，长度至少 6 个字符且不含空白。")); return QString(); }
    const QString strOutDir = localPath(_strOutDir);
#ifdef Q_OS_ANDROID
    if (strOutDir != GetDefaultSaveDir()) { ShowError(QStringLiteral("请使用应用的接收位置。")); return QString(); }
#endif
    if (strOutDir.isEmpty() || !QDir().mkpath(strOutDir) || !QFileInfo(strOutDir).isDir())
    { ShowError(QStringLiteral("保存目录不可用，请更换现有的本地目录。")); return QString(); }
    QTemporaryFile fileProbe(QDir(strOutDir).filePath(QStringLiteral(".crocd-write-XXXXXX")));
    if (!fileProbe.open() || fileProbe.write("probe") != 5 || !fileProbe.flush())
    { ShowError(QStringLiteral("保存目录不可写，请检查权限或更换目录。")); return QString(); }
    fileProbe.close();
    const QStorageInfo stStorage(strOutDir);
    if (stStorage.isValid() && stStorage.isReady() && stStorage.bytesAvailable() <= 0)
    { ShowError(QStringLiteral("目标磁盘没有可用空间，请释放空间或更换目录。")); return QString(); }
    if (m_bDllBackend) {
        STaskSnapshot stTask; stTask.enDirection = ETaskDirection::DIRECTION_RECEIVE;
        stTask.strTaskId = QStringLiteral("recv-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        stTask.strTargetDir = QDir(strOutDir).canonicalPath();
#ifdef Q_OS_ANDROID
        // 每个接收任务独占目录，历史中的导出只会复制该任务的文件。
        stTask.strTargetDir = QDir(stTask.strTargetDir).filePath(stTask.strTaskId);
        if (!QDir().mkpath(stTask.strTargetDir)) { ShowError(QStringLiteral("无法创建接收目录。")); return QString(); }
        const QString strTaskId = EnqueueTask(stTask, _strCode);
        if (strTaskId.isEmpty()) { QDir().rmdir(stTask.strTargetDir); }
        return strTaskId;
#else
        return EnqueueTask(stTask, _strCode);
#endif
    }
    int nActive = 0;
    const QString strCanonical = QDir(strOutDir).canonicalPath();
    for (int nRow = 0; nRow < m_pTaskModel->rowCount(); ++nRow)
    {
        const QString strId = m_pTaskModel->data(m_pTaskModel->index(nRow, 0), TransferTaskModel::ROLE_TASK_ID).toString();
        const STaskSnapshot stTask = m_pTaskModel->Snapshot(strId);
        if (stTask.bTerminal) { continue; }
        ++nActive;
        if (stTask.enDirection == ETaskDirection::DIRECTION_RECEIVE && stTask.strTargetDir.compare(strCanonical, Qt::CaseInsensitive) == 0)
        { ShowError(QStringLiteral("此目录已有活动接收任务。请等待其结束，或为新任务选择独立目录以避免重名竞争。")); return QString(); }
    }
    if (nActive >= 16) { ShowError(QStringLiteral("活动与等待任务已达 16 项，请先完成或取消部分任务。")); return QString(); }
    const QString strTaskId = QStringLiteral("recv-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_pAdapter->SetNextConfigSnapshot(m_pConfigStore->GetValues());
    if (!m_pAdapter->StartReceive(strTaskId, _strCode, strCanonical)) { ShowError(m_pAdapter->GetLastStartError().isEmpty() ? QStringLiteral("接收未能启动，请检查引擎与保存目录。") : m_pAdapter->GetLastStartError()); return QString(); }
    return strTaskId;
}

void AppController::cancelTask(const QString& _strTaskId)  // 取消任务
{
    if (nullptr == m_pAdapter)
    {
        return;
    }
    if (m_hashManagedTasks.contains(_strTaskId)) {
        SManagedTask& stManaged = m_hashManagedTasks[_strTaskId];
        stManaged.m_bUserCancelled = true; stManaged.m_bHoldForUser = false; stManaged.m_strGatePromptId.clear();
        stManaged.m_bExpired = false; stManaged.m_bInternalReconnect = false; stManaged.m_llRetryDeadline = 0;
        STaskSnapshot stTask = m_pTaskModel->Snapshot(_strTaskId);
        if (stTask.bTerminal) { return; }
        stTask.bShareReady = false; stTask.enStage = stManaged.m_bEngineRunning ? ETaskStage::STAGE_CANCELLING : ETaskStage::STAGE_CANCELLED;
        stTask.bTerminal = !stManaged.m_bEngineRunning; stTask.strCode.clear(); m_pTaskModel->ApplySnapshot(stTask);
        ++m_nTaskRevision; emit taskRevisionChanged(); RemovePrompts(_strTaskId);
        if (!stManaged.m_bEngineRunning) { CompleteTask(_strTaskId, 0, true, QString()); PersistTaskJournal(); return; }
    }
    m_pAdapter->Cancel(_strTaskId);
}

void AppController::answerPrompt(bool _bAccept)  // 回答确认
{
    answerPromptFor(m_strPromptId, _bAccept);  // 兼容自检；界面必须传入明确的提示 ID
}

void AppController::answerPromptFor(const QString& _strPromptId, bool _bAccept)
{
    if (_strPromptId.isEmpty() || _strPromptId != m_strPromptId || m_lstPrompts.isEmpty()) { return; }
    if (!m_lstPrompts.first().m_strEnginePromptId.isEmpty())
    {
        const QString strKind = m_lstPrompts.first().m_strKind;
        answerPromptDecisionFor(_strPromptId, _bAccept ? ((strKind == QStringLiteral("overwrite") || strKind == QStringLiteral("folder_overwrite")) ? 2 : 1) : 0);
        return;
    }
    const SPendingPrompt stPrompt = m_lstPrompts.takeFirst();
    m_bPromptVisible = false;
    m_strPromptTaskId.clear();
    m_strPromptId.clear();
    emit promptChanged();
    if (m_pAdapter && m_pAdapter->IsRunning(stPrompt.strTaskId)) { m_pAdapter->AnswerPrompt(stPrompt.strTaskId, _bAccept); }
    ShowNextPrompt();
}

void AppController::dismissPrompt()  // 关闭确认弹窗
{
    answerPrompt(false);
}

void AppController::copyToClipboard(const QString& _strText)  // 复制口令
{
    if (_strText.isEmpty())
    {
        return;
    }
    QGuiApplication::clipboard()->setText(_strText);
    const int nVersion = ++m_nClipboardVersion;
    if (GetSettings().value(QStringLiteral("clipboard")).toString() == QStringLiteral("60 秒后有条件清理"))
    {
        QTimer::singleShot(60000, this, [this, nVersion, _strText]()
        {
            if (nVersion == m_nClipboardVersion && QGuiApplication::clipboard()->text() == _strText) { QGuiApplication::clipboard()->clear(); }
        });
    }
}

QVariantMap AppController::getTask(const QString& _strTaskId) const  // 读取任务详情
{
    QVariantMap hashTask;
    const int nRow = m_pTaskModel->FindRow(_strTaskId);
    if (nRow < 0)
    {
        return hashTask;
    }
    const QModelIndex stIndex = m_pTaskModel->index(nRow, 0);
    hashTask.insert(QStringLiteral("taskId"), m_pTaskModel->data(stIndex, TransferTaskModel::ROLE_TASK_ID));
    hashTask.insert(QStringLiteral("direction"), m_pTaskModel->data(stIndex, TransferTaskModel::ROLE_DIRECTION));
    hashTask.insert(QStringLiteral("name"), m_pTaskModel->data(stIndex, TransferTaskModel::ROLE_NAME));
    hashTask.insert(QStringLiteral("stage"), m_pTaskModel->data(stIndex, TransferTaskModel::ROLE_STAGE_TEXT));
    hashTask.insert(QStringLiteral("percent"), m_pTaskModel->data(stIndex, TransferTaskModel::ROLE_PERCENT));
    hashTask.insert(QStringLiteral("speed"), m_pTaskModel->data(stIndex, TransferTaskModel::ROLE_SPEED));
    hashTask.insert(QStringLiteral("peer"), m_pTaskModel->data(stIndex, TransferTaskModel::ROLE_PEER));
    hashTask.insert(QStringLiteral("code"), m_pTaskModel->data(stIndex, TransferTaskModel::ROLE_CODE));
    hashTask.insert(QStringLiteral("fileIndex"), m_pTaskModel->data(stIndex, TransferTaskModel::ROLE_FILE_INDEX));
    hashTask.insert(QStringLiteral("fileCount"), m_pTaskModel->data(stIndex, TransferTaskModel::ROLE_FILE_COUNT));
    hashTask.insert(QStringLiteral("error"), m_pTaskModel->data(stIndex, TransferTaskModel::ROLE_ERROR));
    const STaskSnapshot stTask = m_pTaskModel->Snapshot(_strTaskId);
    hashTask.insert(QStringLiteral("terminal"), stTask.bTerminal);
    hashTask.insert(QStringLiteral("cancelled"), m_pTaskModel->data(stIndex, TransferTaskModel::ROLE_CANCELLED));
    hashTask.insert(QStringLiteral("shareReady"), stTask.bShareReady && !stTask.bTerminal);
    hashTask.insert(QStringLiteral("targetDir"), stTask.strTargetDir);
    hashTask.insert(QStringLiteral("config"), stTask.hashConfig);
    hashTask.insert(QStringLiteral("bytes"), stTask.llTaskBytes);
    hashTask.insert(QStringLiteral("sizeExact"), stTask.bSizeExact);
    hashTask.insert(QStringLiteral("files"), stTask.m_lstResultFiles);
    hashTask.insert(QStringLiteral("aggregateProgress"), stTask.m_bAggregateProgress);
    hashTask.insert(QStringLiteral("transferPercent"), stTask.dPercent);
    hashTask.insert(QStringLiteral("filePercent"), stTask.m_dFilePercent);
    hashTask.insert(QStringLiteral("verifyPercent"), stTask.m_dVerifyPercent);
    hashTask.insert(QStringLiteral("verifyStatus"), stTask.m_strVerifyStatus);
    const auto itManaged = m_hashManagedTasks.constFind(_strTaskId);
    hashTask.insert(QStringLiteral("canRecover"), itManaged != m_hashManagedTasks.cend() && !itManaged->m_bEngineRunning && itManaged->m_bHoldForUser);
    hashTask.insert(QStringLiteral("needsCode"), stTask.enDirection == ETaskDirection::DIRECTION_RECEIVE);
    hashTask.insert(QStringLiteral("attempt"), itManaged != m_hashManagedTasks.cend() ? itManaged->m_nAttempt : 0);
    hashTask.insert(QStringLiteral("retrySeconds"), itManaged != m_hashManagedTasks.cend() && itManaged->m_llRetryDeadline > 0 ? qMax<qint64>(0, (itManaged->m_llRetryDeadline - m_timerQueueClock.elapsed()) / 1000) : -1);
    hashTask.insert(QStringLiteral("localRelayPort"), stTask.m_strLocalRelayPort);
    hashTask.insert(QStringLiteral("verifying"), stTask.enStage == ETaskStage::STAGE_VERIFYING);
    hashTask.insert(QStringLiteral("resultSummary"), QStringLiteral("完成 %1 / 跳过 %2 / 失败 %3；中断文件保留，不能当作正式完成文件。")
        .arg(stTask.m_nCompletedFiles).arg(stTask.m_nSkippedFiles).arg(stTask.m_nFailedFiles));
    return hashTask;
}

QString AppController::GetPromptId() const { return m_strPromptId; }
bool AppController::EngineAvailable() const { return m_pAdapter != nullptr && !m_strEngineVersion.isEmpty(); }
bool AppController::HasActiveTasks() const
{
    for (int nRow = 0; nRow < m_pTaskModel->rowCount(); ++nRow)
    {
        const QString strId = m_pTaskModel->data(m_pTaskModel->index(nRow, 0), TransferTaskModel::ROLE_TASK_ID).toString();
        if (m_hashManagedTasks.contains(strId) && m_hashManagedTasks.value(strId).m_bHoldForUser) { continue; }
        if (!m_pTaskModel->Snapshot(strId).bTerminal) { return true; }
    }
    return false;
}

void AppController::ShowError(const QString& _strText)
{
    m_strLastEventText = TransferSafety::Redact(_strText, {m_stConfig.strRelayPass});
    emit lastEventTextChanged();
    emit notice(m_strLastEventText);
}

QString AppController::ValidateStart() const
{
    if (!m_bConfigLoaded) { return QStringLiteral("配置未能可靠读取，请检查权限，并在设置中确认后保存；本次不会自动连接。"); }
#ifdef Q_OS_ANDROID
    if (!EngineAvailable()) { return QStringLiteral("未检测到可用 Croc 引擎，请重新安装安卓应用。"); }
#else
    if (!EngineAvailable()) { return QStringLiteral("未检测到可用 Croc 引擎，请检查程序目录中的 croc.exe 后重启。"); }
#endif
    return ConfigStore::Validate(GetSettings());
}

QString AppController::localPath(const QString& _strUrl) const
{
    const QUrl urlInput(_strUrl);
    const QString strPath = urlInput.isLocalFile() ? urlInput.toLocalFile() : _strUrl;
    if (!QFileInfo(strPath).isAbsolute() || (!urlInput.isLocalFile() && _strUrl.contains(QStringLiteral("://")))) { return QString(); }
    return QDir::cleanPath(strPath);
}

QVariantList AppController::describeSelection(const QStringList& _lstUrls) const
{
    QVariantList lstResult;
    QSet<QString> setPaths;
    for (const QString& strUrl : _lstUrls)
    {
        const QString strPath = localPath(strUrl);
        const QFileInfo stInfo(strPath);
        if (strPath.isEmpty() || !stInfo.exists() || setPaths.contains(strPath.toCaseFolded())) { continue; }
        setPaths.insert(strPath.toCaseFolded());
        QVariantMap hashItem;
        hashItem.insert(QStringLiteral("path"), strPath);
        hashItem.insert(QStringLiteral("name"), stInfo.fileName());
        hashItem.insert(QStringLiteral("directory"), stInfo.isDir());
        hashItem.insert(QStringLiteral("bytes"), stInfo.isFile() ? stInfo.size() : -1);
        lstResult.append(hashItem);
    }
    return lstResult;
}

void AppController::openLocation(const QString& _strPath)
{
    const QString strPath = localPath(_strPath);
    if (strPath.isEmpty() || !QFileInfo::exists(strPath)) { ShowError(QStringLiteral("位置已移动或删除，请检查文件是否仍存在。")); return; }
    const QString strFolder = QFileInfo(strPath).isDir() ? strPath : QFileInfo(strPath).absolutePath();
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(strFolder))) { ShowError(QStringLiteral("无法打开文件管理器，请手动查看此目录。")); }
}

void AppController::importAndroidDocuments(const QStringList& _lstUris)
{
#ifdef Q_OS_ANDROID
    if (_lstUris.isEmpty() || m_bAndroidFileBusy) { return; }
    m_bAndroidFileBusy = true;
    emit androidFileBusyChanged();
    auto* pWatcher = new QFutureWatcher<QStringList>(this);
    connect(pWatcher, &QFutureWatcher<QStringList>::finished, this, [this, pWatcher]()
    {
        const QStringList lstPaths = pWatcher->result();
        pWatcher->deleteLater();
        m_bAndroidFileBusy = false;
        emit androidFileBusyChanged();
        if (lstPaths.isEmpty()) { ShowError(QStringLiteral("无法导入文件，请检查存储空间或重新选择。")); }
        else { emit androidDocumentsImported(lstPaths); }
    });
    pWatcher->setFuture(QtConcurrent::run(AndroidFiles::Import, _lstUris));
#else
    Q_UNUSED(_lstUris);
#endif
}

void AppController::exportAndroidReceive(const QString& _strPath, const QString& _strTreeUri)
{
#ifdef Q_OS_ANDROID
    if (m_bAndroidFileBusy) { return; }
    const QFileInfo stSource(_strPath);
    const QString strSource = stSource.canonicalFilePath();
    const QRegularExpression regexTaskDir(QStringLiteral("^recv-[0-9a-f-]{36}$"));
    if (strSource.isEmpty() || !stSource.isDir() || stSource.isSymLink()
        || QFileInfo(strSource).absolutePath() != QDir(GetDefaultSaveDir()).canonicalPath()
        || !regexTaskDir.match(stSource.fileName()).hasMatch())
    {
        ShowError(QStringLiteral("找不到该任务的接收文件。"));
        return;
    }
    m_bAndroidFileBusy = true;
    emit androidFileBusyChanged();
    auto* pWatcher = new QFutureWatcher<bool>(this);
    connect(pWatcher, &QFutureWatcher<bool>::finished, this, [this, pWatcher]()
    {
        const bool bExported = pWatcher->result();
        pWatcher->deleteLater();
        m_bAndroidFileBusy = false;
        emit androidFileBusyChanged();
        ShowError(bExported ? QStringLiteral("已导出到所选文件夹。")
                            : QStringLiteral("导出失败，请检查存储空间或选择其他文件夹。"));
    });
    pWatcher->setFuture(QtConcurrent::run(AndroidFiles::Export, strSource, _strTreeUri));
#else
    Q_UNUSED(_strPath);
    Q_UNUSED(_strTreeUri);
#endif
}

void AppController::QueuePrompt(const QString& _strTaskId, const QString& _strTitle, const QString& _strBody)
{
    SPendingPrompt stPrompt;
    stPrompt.strId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    stPrompt.strTaskId = _strTaskId;
    stPrompt.strTitle = _strTitle;
    stPrompt.strBody = _strBody;
    m_lstPrompts.append(stPrompt);
    ShowNextPrompt();
}

void AppController::ShowNextPrompt()
{
    if (m_bPromptVisible) { return; }
    while (!m_lstPrompts.isEmpty() && (!m_pAdapter || !m_pAdapter->IsRunning(m_lstPrompts.first().strTaskId))) { m_lstPrompts.removeFirst(); }
    if (m_lstPrompts.isEmpty()) { return; }
    const SPendingPrompt stPrompt = m_lstPrompts.first();
    m_strPromptTaskId = stPrompt.strTaskId;
    m_strPromptId = stPrompt.strId;
    m_strPromptTitle = stPrompt.strTitle;
    m_strPromptBody = stPrompt.strBody;
    m_bPromptVisible = true;
    emit promptChanged();
}

void AppController::RemovePrompts(const QString& _strTaskId)
{
    const bool bCurrent = m_strPromptTaskId == _strTaskId;
    for (int nIndex = m_lstPrompts.size() - 1; nIndex >= 0; --nIndex) { if (m_lstPrompts.at(nIndex).strTaskId == _strTaskId) { m_lstPrompts.removeAt(nIndex); } }
    if (bCurrent) { m_bPromptVisible = false; m_strPromptTaskId.clear(); m_strPromptId.clear(); emit promptChanged(); }
    ShowNextPrompt();
}

void AppController::answerPromptDecisionFor(const QString& _strPromptId, int _nDecision)
{
    if (!m_bPromptVisible || m_lstPrompts.isEmpty() || m_strPromptId != _strPromptId || _strPromptId.isEmpty()) { return; }
    const SPendingPrompt stPrompt = m_lstPrompts.first();
    if (stPrompt.m_strEnginePromptId.isEmpty()) { answerPromptFor(_strPromptId, _nDecision == 1); return; }
    if (_nDecision == 4 && !stPrompt.m_bCanResume) { return; }
    if (!m_pAdapter || !(m_hashManagedTasks.contains(stPrompt.strTaskId) ? AnswerManagedPrompt(stPrompt.strTaskId, stPrompt.m_strEnginePromptId, _nDecision) : m_pAdapter->AnswerDllPrompt(stPrompt.strTaskId, stPrompt.m_strEnginePromptId, _nDecision)))
    { ShowError(QStringLiteral("确认操作已失效或未被引擎接受，请检查对应任务状态。")); return; }
    m_lstPrompts.removeFirst();
    m_bPromptVisible = false; m_strPromptTaskId.clear(); m_strPromptId.clear(); emit promptChanged();
    ShowNextPrompt();
}

void AppController::CompleteTask(const QString& _strTaskId, int _nExitCode, bool _bCancelled, const QString& _strError)
{
        if (m_setFinishedTasks.contains(_strTaskId)) { return; }
        m_setFinishedTasks.insert(_strTaskId);
        RemovePrompts(_strTaskId);
        const STaskSnapshot stTask = m_pTaskModel->Snapshot(_strTaskId);
        const QVariantMap hashTask = getTask(_strTaskId);
        m_strShareCode.clear();
        emit shareCodeChanged();
        ShowError(QStringLiteral("%1：%2%3").arg(_strTaskId, hashTask.value(QStringLiteral("stage")).toString(),
            _strError.isEmpty() ? QString() : QLatin1Char('\n') + _strError));
        emit taskFinished(_strTaskId, _nExitCode, _bCancelled, _strError);

        // 写入历史：只记结果与文件信息，不记口令。
        SHistoryRecord stRecord;
        stRecord.strTaskId = _strTaskId;
        stRecord.strDirection = stTask.enDirection == ETaskDirection::DIRECTION_SEND ? QStringLiteral("发送") : QStringLiteral("接收");
        stRecord.llBytes = stTask.llTaskBytes;
        stRecord.bSizeExact = stTask.bSizeExact;
        stRecord.nFileCount = stTask.nFileCount;
        stRecord.strName = stTask.nFileCount > 1 ? QStringLiteral("%1 个文件").arg(stTask.nFileCount) : stTask.strDisplayName;
        stRecord.strPeer = stTask.strPeer;
        stRecord.strError = TransferSafety::Redact(stTask.strError);
        stRecord.strResult = hashTask.value(QStringLiteral("stage")).toString();
        stRecord.strFinishedAt = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        stRecord.strLocation = stTask.enDirection == ETaskDirection::DIRECTION_RECEIVE ? stTask.strTargetDir
            : (stTask.lstSourcePaths.isEmpty() ? QString() : QFileInfo(stTask.lstSourcePaths.first()).absolutePath());
        const QVariantList lstHistoryFiles = stTask.m_lstResultFiles.isEmpty() ? stTask.lstFiles : stTask.m_lstResultFiles;
        for (const QVariant& varFile : lstHistoryFiles) {
            const QVariantMap hashFile = varFile.toMap();
            stRecord.lstFiles.append(hashFile.value(QStringLiteral("path")).toString() + QStringLiteral(" [") + hashFile.value(QStringLiteral("status"), QStringLiteral("源文件")).toString() + QLatin1Char(']'));
        }
        m_pHistoryModel->Append(stRecord);

}
