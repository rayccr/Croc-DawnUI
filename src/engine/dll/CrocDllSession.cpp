/**
****************************************************************
* @file    CrocDllSession.cpp
* @brief   DLL 后端任务会话实现
* @author  Ray
* @date    2026-11-02
* @note    V1.0.0       Ray       2026-11-02    1. 迁移第 3 轮：适配层 DLL 后端
****************************************************************
*/
#include "CrocDllSession.h"

#include <QDateTime>
#include <QDeadlineTimer>
#include <QThread>
#include <QFileSystemWatcher>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>
#include <QDir>  // 路径
#include <QFileInfo>  // 目录检查
#include <QJsonArray>  // 发送文件数组
#include <QJsonDocument>  // JSON 序列化
#include <QJsonObject>  // 配置对象
#include <QTimer>  // 轮询
#include <QUuid>  // 任务标识与口令

#include "CrocJsonMap.h"  // 快照映射
#include "CrocLibrary.h"  // C ABI
#include "TransferSafety.h"
#include "CrocEventPump.h"

namespace
{
const int g_nPollIntervalMs = 200;  // 快照轮询间隔
}

CrocDllSession::CrocDllSession(CrocLibrary* _pLibrary, QObject* _pParent)  // 构造
    : QObject(_pParent)
    , m_pLibrary(_pLibrary)
{
    m_pSourceWatcher = new QFileSystemWatcher(this);
    connect(m_pSourceWatcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString&) {
        for (auto itTask = m_hashTasks.begin(); itTask != m_hashTasks.end(); ++itTask) { itTask->m_llLastSourceCheck = 0; }
    });
    m_pEventPump = new CrocEventPump(m_pLibrary, this);
    connect(m_pEventPump, &CrocEventPump::eventReceived, this, &CrocDllSession::HandleEvent, Qt::QueuedConnection);
    m_pEventPump->start();
    m_pTimerPoll = new QTimer(this);
    m_pTimerPoll->setInterval(g_nPollIntervalMs);
    connect(m_pTimerPoll, &QTimer::timeout, this, &CrocDllSession::PollAll);
}

CrocDllSession::~CrocDllSession()  // 析构时取消并释放全部任务
{
    if (nullptr == m_pLibrary)
    {
        return;
    }
    m_pEventPump->Stop();
    const QStringList lstTaskIds = m_hashTasks.keys();
    for (const QString& strTaskId : lstTaskIds)
    {
        Cancel(strTaskId);
    }
    m_pTimerPoll->stop();
    if (!m_hashTasks.isEmpty()) { m_pLibrary->Shutdown(); }
    m_hashTasks.clear();
}

QList<STaskSnapshot> CrocDllSession::StopAndSnapshotReceives()
{
    QList<STaskSnapshot> lstFinal;
    if (!IsAvailable()) { return lstFinal; }
    QStringList lstReceiveIds;
    for (auto itTask = m_hashTasks.cbegin(); itTask != m_hashTasks.cend(); ++itTask)
    {
        if (itTask->stSnapshot.enDirection == ETaskDirection::DIRECTION_RECEIVE) { lstReceiveIds.append(itTask.key()); }
    }
    if (lstReceiveIds.isEmpty()) { return lstFinal; }
    for (const QString& strTaskId : m_hashTasks.keys()) { Cancel(strTaskId); }
    QDeadlineTimer timerWait(2000);
    while (!lstReceiveIds.isEmpty() && !timerWait.hasExpired())
    {
        for (int nIndex = lstReceiveIds.size() - 1; nIndex >= 0; --nIndex)
        {
            const auto itTask = m_hashTasks.constFind(lstReceiveIds.at(nIndex));
            bool bFinished = false;
            if (!m_pLibrary->IsTaskFinished(itTask->nEngineId, bFinished) || !bFinished) { continue; }
            STaskSnapshot stFinal = itTask->stSnapshot;
            QString strError;
            if (MapCrocSnapshotJson(m_pLibrary->GetTaskSnapshot(itTask->nEngineId), stFinal, strError))
            {
                stFinal.strTaskId = itTask.key();
                stFinal.bTerminal = false;  // 正常退出仍以中断记录保存，无须在析构中写历史。
                stFinal.enStage = ETaskStage::STAGE_INTERRUPTED;
                lstFinal.append(stFinal);
            }
            lstReceiveIds.removeAt(nIndex);
        }
        if (!lstReceiveIds.isEmpty()) { QThread::msleep(25); }
    }
    return lstFinal;
}

bool CrocDllSession::IsAvailable() const  // 库可用性
{
    return (nullptr != m_pLibrary) && m_pLibrary->IsLoaded() && m_pLibrary->IsInitialized();
}

QString CrocDllSession::GetLastError() const  // 最近错误
{
    return m_strLastError;
}

QString CrocDllSession::MakeCode(const QString& _strSeed)  // 生成口令
{
    const QString strUuid = QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-'));
    return QStringLiteral("croc-") + _strSeed.left(8) + QStringLiteral("-") + strUuid.left(12);
}

bool CrocDllSession::StartSend(const QString& _strTaskId, const QStringList& _lstPaths, const QString& _strCode,
                               const SCrocConfig& _stConfig, const QVariantMap& _hashConfig)  // 启动发送
{
    if (!IsAvailable())
    {
        m_strLastError = QStringLiteral("引擎库不可用");
        return false;
    }
    m_strLastError.clear();
    const QVariantMap hashManifest = TransferSafety::InspectFiles(_lstPaths);
    if (!hashManifest.value(QStringLiteral("error")).toString().isEmpty())
    { m_strLastError = hashManifest.value(QStringLiteral("error")).toString(); return false; }
    QJsonObject objConfig;
    objConfig.insert(QStringLiteral("code"), _strCode.isEmpty() ? MakeCode(_strTaskId) : _strCode);
    QJsonArray arrFiles;
    for (const QString& strPath : _lstPaths)
    {
        arrFiles.append(QFileInfo(strPath).absoluteFilePath());
    }
    objConfig.insert(QStringLiteral("files"), arrFiles);
    if (!_stConfig.strRelay.isEmpty()) { objConfig.insert(QStringLiteral("relay"), _stConfig.strRelay); }
    if (!_stConfig.strRelayPass.isEmpty()) { objConfig.insert(QStringLiteral("relay_password"), _stConfig.strRelayPass); }
    objConfig.insert(QStringLiteral("curve"), _stConfig.strCurve);
    objConfig.insert(QStringLiteral("no_compress"), _stConfig.bNoCompress);
    objConfig.insert(QStringLiteral("disable_local"), !_stConfig.bLocalOnly);
    if (_stConfig.bLocalOnly) {
        // 仅提供起始端口；库绑定并保留套接字，从9841起自动跳过占用端口。
        objConfig.insert(QStringLiteral("local_relay_ports"), QJsonArray{QStringLiteral("9841"), QStringLiteral("9842")});
    }
    objConfig.insert(QStringLiteral("only_local"), _stConfig.bLocalOnly);
    objConfig.insert(QStringLiteral("zip_folder"), _stConfig.bZipFolders);
    objConfig.insert(QStringLiteral("ask"), _hashConfig.value(QStringLiteral("queueManaged")).toBool());
    objConfig.insert(QStringLiteral("prompt_timeout_seconds"), 0);  // 等待用户应答或任务结束，不按时间拒绝
    if (_stConfig.dUploadLimitMbps > 0.0)
    {
        objConfig.insert(QStringLiteral("throttle_upload"),
                         QString::number(qMax<qint64>(1, qRound64(_stConfig.dUploadLimitMbps * 1000000.0))));
    }

    quint64 nEngineId = 0;
    QString strError;
    if (!m_pLibrary->StartSend(QJsonDocument(objConfig).toJson(QJsonDocument::Compact), nEngineId, strError))
    {
        m_strLastError = TransferSafety::Redact(strError, {objConfig.value(QStringLiteral("code")).toString(), _stConfig.strRelayPass});
        return false;
    }
    SDllTask stTask;
    stTask.nEngineId = nEngineId;
    stTask.stSnapshot.strTaskId = _strTaskId;
    stTask.stSnapshot.hashConfig = _hashConfig;
    stTask.stSnapshot.enDirection = ETaskDirection::DIRECTION_SEND;
    stTask.stSnapshot.enStage = ETaskStage::STAGE_STARTING;
    stTask.stSnapshot.strCode = objConfig.value(QStringLiteral("code")).toString();
    stTask.stSnapshot.strDisplayName = QFileInfo(_lstPaths.first()).fileName();
    stTask.stSnapshot.lstSourcePaths = hashManifest.value(QStringLiteral("paths")).toStringList();
    stTask.stSnapshot.lstFiles = hashManifest.value(QStringLiteral("files")).toList();
    stTask.stSnapshot.llTaskBytes = hashManifest.value(QStringLiteral("bytes")).toLongLong();
    stTask.stSnapshot.bSizeExact = true;
    stTask.stSnapshot.nFileCount = stTask.stSnapshot.lstFiles.size();
    QSet<QString> setDirectories;
    for (const QVariant& varFile : stTask.stSnapshot.lstFiles) { setDirectories.insert(QFileInfo(varFile.toMap().value(QStringLiteral("path")).toString()).absolutePath()); }
    for (const QString& strDir : setDirectories) { if (!m_pSourceWatcher->directories().contains(strDir)) { m_pSourceWatcher->addPath(strDir); } }
    m_hashTasks.insert(_strTaskId, stTask);
    m_pTimerPoll->start();
    emit taskUpdated(stTask.stSnapshot);
    return true;
}

bool CrocDllSession::StartReceive(const QString& _strTaskId, const QString& _strCode, const QString& _strOutDir,
                                  const SCrocConfig& _stConfig, const QVariantMap& _hashConfig)  // 启动接收
{
    if (!IsAvailable())
    {
        m_strLastError = QStringLiteral("引擎库不可用");
        return false;
    }
    QDir().mkpath(_strOutDir);
    m_strLastError.clear();
    QJsonObject objConfig;
    objConfig.insert(QStringLiteral("code"), _strCode);
    objConfig.insert(QStringLiteral("save_path"), QDir(_strOutDir).absolutePath());
    objConfig.insert(QStringLiteral("prompt_timeout_seconds"), 0);  // 等待用户应答或任务结束，不按时间拒绝
    objConfig.insert(QStringLiteral("auto_accept"), false);  // 接收确认通过事件和明确应答闭环
    if (!_stConfig.strRelay.isEmpty()) { objConfig.insert(QStringLiteral("relay"), _stConfig.strRelay); }
    if (!_stConfig.strRelayPass.isEmpty()) { objConfig.insert(QStringLiteral("relay_password"), _stConfig.strRelayPass); }
    objConfig.insert(QStringLiteral("curve"), _stConfig.strCurve);
    objConfig.insert(QStringLiteral("no_compress"), _stConfig.bNoCompress);
    objConfig.insert(QStringLiteral("disable_local"), !_stConfig.bLocalOnly);
    if (_stConfig.bLocalOnly) {
        // 仅提供起始端口；库绑定并保留套接字，从9841起自动跳过占用端口。
        objConfig.insert(QStringLiteral("local_relay_ports"), QJsonArray{QStringLiteral("9841"), QStringLiteral("9842")});
    }
    objConfig.insert(QStringLiteral("only_local"), _stConfig.bLocalOnly);
    objConfig.insert(QStringLiteral("rename"), _stConfig.bRenameOnConflict);
    if (_stConfig.bLocalOnly && !_stConfig.m_strLanAddress.isEmpty()) { objConfig.insert(QStringLiteral("ip"), _stConfig.m_strLanAddress); }

    quint64 nEngineId = 0;
    QString strError;
    if (!m_pLibrary->StartReceive(QJsonDocument(objConfig).toJson(QJsonDocument::Compact), nEngineId, strError))
    {
        m_strLastError = TransferSafety::Redact(strError, {objConfig.value(QStringLiteral("code")).toString(), _stConfig.strRelayPass});
        return false;
    }
    SDllTask stTask;
    stTask.nEngineId = nEngineId;
    stTask.stSnapshot.strTaskId = _strTaskId;
    stTask.stSnapshot.hashConfig = _hashConfig;
    stTask.stSnapshot.enDirection = ETaskDirection::DIRECTION_RECEIVE;
    stTask.stSnapshot.enStage = ETaskStage::STAGE_STARTING;
    stTask.stSnapshot.strCode = _strCode;
    stTask.stSnapshot.strTargetDir = QDir(_strOutDir).absolutePath();
    m_hashTasks.insert(_strTaskId, stTask);
    m_pTimerPoll->start();
    emit taskUpdated(stTask.stSnapshot);
    return true;
}

void CrocDllSession::Cancel(const QString& _strTaskId)  // 取消任务
{
    auto itTask = m_hashTasks.find(_strTaskId);
    if (itTask == m_hashTasks.end())
    {
        return;
    }
    itTask->bCancelRequested = true;
    if (nullptr != m_pLibrary)
    {
        m_pLibrary->CancelTask(itTask->nEngineId);
    }
}

bool CrocDllSession::IsRunning(const QString& _strTaskId) const  // 是否运行中
{
    return m_hashTasks.contains(_strTaskId);
}

void CrocDllSession::PollAll()  // 轮询全部任务
{
    if (nullptr == m_pLibrary)
    {
        return;
    }
    const QStringList lstTaskIds = m_hashTasks.keys();
    for (const QString& strTaskId : lstTaskIds)
    {
        auto itTask = m_hashTasks.find(strTaskId);
        if (itTask == m_hashTasks.end())
        {
            continue;
        }
        // 目录通知加周期复核；全清单检查在工作线程，避免大任务阻塞界面。
        if (!itTask->bCancelRequested && itTask->m_strSafetyError.isEmpty()
            && itTask->stSnapshot.enDirection == ETaskDirection::DIRECTION_SEND)
        {
            if (itTask->m_futureSourceCheck.isValid() && itTask->m_futureSourceCheck.isFinished())
            {
                if (!itTask->m_futureSourceCheck.result())
                {
                    itTask->m_strSafetyError = QStringLiteral("源文件已删除、替换或修改，任务已停止。请重新选择并分享口令；对端部分文件需重新核对。");
                    m_pLibrary->CancelTask(itTask->nEngineId);
                }
                itTask->m_futureSourceCheck = QFuture<bool>();
            }
            const qint64 llNow = QDateTime::currentMSecsSinceEpoch();
            if (itTask->m_strSafetyError.isEmpty() && !itTask->m_futureSourceCheck.isValid() && llNow - itTask->m_llLastSourceCheck >= 1000)
            {
                const QVariantList lstFiles = itTask->stSnapshot.lstFiles;
                itTask->m_futureSourceCheck = QtConcurrent::run([lstFiles] { return TransferSafety::FilesUnchanged(lstFiles); });
                itTask->m_llLastSourceCheck = llNow;
            }
        }
        STaskSnapshot stMapped = itTask->stSnapshot;
        QString strMapError;
        const QByteArray arrSnapshot = m_pLibrary->GetTaskSnapshot(itTask->nEngineId);

        if (!arrSnapshot.isEmpty() && MapCrocSnapshotJson(arrSnapshot, stMapped, strMapError))
        {
            // 库句柄只用于 ABI；界面 UUID 必须贯穿任务模型、口令、取消和历史。
            stMapped.strTaskId = strTaskId;
            if (stMapped.enStage == ETaskStage::STAGE_COMPLETED) { stMapped.enStage = stMapped.enDirection == ETaskDirection::DIRECTION_SEND ? ETaskStage::STAGE_AWAITING_CONFIRM : ETaskStage::STAGE_SAVING; }
            stMapped.bTerminal = false;  // 等 IsTaskFinished 确认退出后再收尾
            const QJsonObject objSnapshot = QJsonDocument::fromJson(arrSnapshot).object();
            const QString strPhase = objSnapshot.value(QStringLiteral("phase")).toString();

            stMapped.bShareReady = stMapped.enDirection == ETaskDirection::DIRECTION_SEND
                && (strPhase == QStringLiteral("waiting_peer") || strPhase == QStringLiteral("negotiating")
                    || strPhase == QStringLiteral("transferring") || strPhase == QStringLiteral("awaiting_confirmation"));
            stMapped.strCode = itTask->stSnapshot.strCode;
            stMapped.strTargetDir = itTask->stSnapshot.strTargetDir;
            if (stMapped.strDisplayName.isEmpty())
            {
                stMapped.strDisplayName = itTask->stSnapshot.strDisplayName;
            }
            itTask->stSnapshot = stMapped;
            emit taskUpdated(itTask->stSnapshot);
        }


        itTask = m_hashTasks.find(strTaskId);
        if (itTask == m_hashTasks.end()) { continue; }
        bool bFinished = false;
        if (m_pLibrary->IsTaskFinished(itTask->nEngineId, bFinished) && bFinished)
        {
            const bool bCancelled = itTask->bCancelRequested;
            const QJsonObject objFinal = QJsonDocument::fromJson(m_pLibrary->GetTaskSnapshot(itTask->nEngineId)).object();
            QString strError = objFinal.value(QStringLiteral("error")).toString();
            const QString strPhase = objFinal.value(QStringLiteral("phase")).toString();
            if (strError.isEmpty() && strPhase != QStringLiteral("completed") && !bCancelled)
            { strError = QStringLiteral("引擎任务未成功完成（阶段：%1），请检查连接和目标文件。").arg(strPhase); }
            { MapCrocSnapshotJson(QJsonDocument(objFinal).toJson(), itTask->stSnapshot, strMapError); itTask->stSnapshot.strTaskId = strTaskId; }
            if (!bCancelled && itTask->stSnapshot.enDirection == ETaskDirection::DIRECTION_SEND
                && !TransferSafety::FilesUnchanged(itTask->stSnapshot.lstFiles))
            { itTask->m_strSafetyError = QStringLiteral("结束前检测到源文件已删除、替换或修改。请重新选择文件发送，不能视为本次成功。"); }
            if (!itTask->m_strSafetyError.isEmpty()) { strError = itTask->m_strSafetyError; }
            FinishTask(strTaskId, bCancelled || (strPhase == QStringLiteral("cancelled") && itTask->m_strSafetyError.isEmpty()),
                       TransferSafety::Redact(strError, {itTask->stSnapshot.strCode}));
        }
    }
    if (m_hashTasks.isEmpty())
    {
        m_pTimerPoll->stop();
    }
}

void CrocDllSession::FinishTask(const QString& _strTaskId, bool _bCancelled, const QString& _strError)  // 收尾
{
    auto itTask = m_hashTasks.find(_strTaskId);
    if (itTask == m_hashTasks.end())
    {
        return;
    }
    STaskSnapshot stSnapshot = itTask->stSnapshot;
    stSnapshot.bTerminal = true;
    stSnapshot.nExitCode = (!_bCancelled && !_strError.isEmpty()) ? 1 : 0;
    stSnapshot.bShareReady = false;
    stSnapshot.strCode.clear();
    stSnapshot.strSpeed.clear();
    stSnapshot.strEta.clear();
    stSnapshot.strError = _strError;
    if (_bCancelled)
    {
        stSnapshot.enStage = ETaskStage::STAGE_CANCELLED;
    }
    else if (!_strError.isEmpty())
    {
        stSnapshot.enStage = ETaskStage::STAGE_FAILED;
    }
    else if (stSnapshot.enDirection == ETaskDirection::DIRECTION_SEND)
    {
        stSnapshot.enStage = ETaskStage::STAGE_SENT;
        // 百分比保留引擎真实值，跳过文件不应补成100%。
    }
    else
    {
        stSnapshot.enStage = ETaskStage::STAGE_COMPLETED;
        // 百分比保留引擎真实值，跳过文件不应补成100%。
    }
    if (!_bCancelled && _strError.isEmpty())
    {
        if (stSnapshot.m_nSkippedFiles > 0)
        { stSnapshot.enStage = stSnapshot.m_nCompletedFiles > 0 ? ETaskStage::STAGE_PARTIAL : ETaskStage::STAGE_SKIPPED; }
        if (stSnapshot.enDirection == ETaskDirection::DIRECTION_RECEIVE && stSnapshot.enStage == ETaskStage::STAGE_COMPLETED)
        {
            bool bAllPresent = stSnapshot.nFileCount == stSnapshot.m_lstResultFiles.size() && !stSnapshot.m_lstResultFiles.isEmpty();
            const QString strRoot = QFileInfo(stSnapshot.strTargetDir).canonicalFilePath();
            for (const QVariant& varFile : stSnapshot.m_lstResultFiles)
            {
                const QVariantMap hashFile = varFile.toMap();
                const QFileInfo infoTarget(hashFile.value(QStringLiteral("path")).toString());
                const QString strActual = infoTarget.canonicalFilePath();
                bAllPresent &= infoTarget.isFile() && !infoTarget.isSymLink() && !strRoot.isEmpty()
                    && strActual.startsWith(strRoot + QLatin1Char('/'), Qt::CaseInsensitive)
                    && infoTarget.size() == hashFile.value(QStringLiteral("size")).toLongLong();
            }
            stSnapshot.enStage = ETaskStage::STAGE_SAVING;
            emit taskUpdated(stSnapshot);
            stSnapshot.enStage = ETaskStage::STAGE_COMPLETED;
            if (!bAllPresent) { stSnapshot.enStage = ETaskStage::STAGE_UNCONFIRMED; stSnapshot.strError = QStringLiteral("引擎已结束，但目标清单存在缺失或路径/大小变化。请检查文件，不能确认全部接收成功。"); }
        }
    }
    const quint64 nEngineId = itTask->nEngineId;
    for (const QString& strPromptId : m_hashPromptTasks.keys())
    { if (m_hashPromptTasks.value(strPromptId) == _strTaskId) { m_hashPromptTasks.remove(strPromptId); m_hashPromptKinds.remove(strPromptId); } }
    m_hashTasks.erase(itTask);
    QSet<QString> setNeeded;
    for (const SDllTask& stRemaining : m_hashTasks) { for (const QVariant& varFile : stRemaining.stSnapshot.lstFiles) { setNeeded.insert(QFileInfo(varFile.toMap().value(QStringLiteral("path")).toString()).absolutePath()); } }
    for (const QString& strDir : m_pSourceWatcher->directories()) { if (!setNeeded.contains(strDir)) { m_pSourceWatcher->removePath(strDir); } }
    m_pLibrary->ReleaseTask(nEngineId);
    emit taskUpdated(stSnapshot);
    emit taskFinished(_strTaskId, stSnapshot.nExitCode, _bCancelled, _strError);
}

// 全局ABI事件只由本会话的事件泵消费，再按引擎句柄分派到界面UUID。
void CrocDllSession::HandleEvent(const QByteArray& _arrEvent)
{
    const QJsonObject objEvent = QJsonDocument::fromJson(_arrEvent).object();
    if (objEvent.value(QStringLiteral("event")).toString() != QStringLiteral("prompt")) { return; }
    const quint64 nEngineId = objEvent.value(QStringLiteral("task_id")).toInteger();
    for (auto itTask = m_hashTasks.cbegin(); itTask != m_hashTasks.cend(); ++itTask)
    {
        if (itTask->nEngineId != nEngineId || itTask->bCancelRequested) { continue; }
        const QString strPromptId = objEvent.value(QStringLiteral("prompt_id")).toString();
        if (strPromptId.isEmpty() || m_hashPromptTasks.contains(strPromptId)) { return; }
        const QString strKind = objEvent.value(QStringLiteral("prompt_kind")).toString();
        const QString strTaskId = itTask.key();
        m_hashPromptTasks.insert(strPromptId, strTaskId);
        m_hashPromptKinds.insert(strPromptId, strKind);
        QVariantMap hashDetails = objEvent.toVariantMap();
        const QJsonObject objCurrent = QJsonDocument::fromJson(m_pLibrary->GetTaskSnapshot(nEngineId)).object();
        hashDetails.insert(QStringLiteral("files"), objCurrent.value(QStringLiteral("files")).toArray().toVariantList());
        hashDetails.insert(QStringLiteral("peer_address"), objCurrent.value(QStringLiteral("peer_address")).toString());
        emit promptRequested(strTaskId, strPromptId, strKind, hashDetails);
        return;
    }
}

bool CrocDllSession::AnswerPrompt(const QString& _strTaskId, const QString& _strPromptId, int _nDecision)
{
    const auto itTask = m_hashTasks.constFind(_strTaskId);
    if (itTask == m_hashTasks.cend() || itTask->bCancelRequested
        || m_hashPromptTasks.value(_strPromptId) != _strTaskId) { return false; }
    const QString strKind = m_hashPromptKinds.value(_strPromptId);
    const bool bAllowed = _nDecision == 0 || ((strKind == QStringLiteral("receive_accept") || strKind == QStringLiteral("send_approval")) && _nDecision == 1)
        || (strKind == QStringLiteral("overwrite") && (_nDecision == 2 || _nDecision == 3 || _nDecision == 4))
        || (strKind == QStringLiteral("folder_overwrite") && _nDecision == 2);
    if (!bAllowed) { return false; }
    const bool bAnswered = m_pLibrary->RespondToPrompt(itTask->nEngineId, _strPromptId.toUtf8(), _nDecision);
    if (bAnswered) { m_hashPromptTasks.remove(_strPromptId); m_hashPromptKinds.remove(_strPromptId); }
    return bAnswered;
}
