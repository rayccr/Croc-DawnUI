/**
****************************************************************
* @file    AppControllerQueue.cpp
* @brief   DLL逻辑任务调度、重连与原子任务记录；不保存口令或中继密码
* @author  Ray
* @date    2026-11-04
* @note    引擎尝试可以结束，逻辑任务仍可等待重连；重启从中断记录手动恢复。
****************************************************************
*/
#include "AppController.h"
#include "CrocAdapter.h"
#include "TransferTaskModel.h"
#include "TransferSafety.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QRegularExpression>

namespace
{
bool IsIncompleteReceiveFile(const QVariantMap& _hashFile)
{
    const QString strStatus = _hashFile.value(QStringLiteral("status")).toString();
    return (strStatus == QStringLiteral("pending") || strStatus == QStringLiteral("transferring") || strStatus == QStringLiteral("failed"))
        && _hashFile.value(QStringLiteral("bytes_done")).toLongLong() < _hashFile.value(QStringLiteral("size")).toLongLong();
}
}

void AppController::InitializeTaskQueue()
{
    if (!m_bDllBackend) { return; } // 旧process保持既有路径，不宣称拥有DLL调度能力。
    m_timerQueueClock.start();
    m_pJournalTimer = new QTimer(this);
    m_pJournalTimer->setSingleShot(true);
    connect(m_pJournalTimer, &QTimer::timeout, this, &AppController::PersistTaskJournal);
    m_strTaskJournalPath = qEnvironmentVariable("FLOWDROP_TASKS_PATH");
    if (m_strTaskJournalPath.isEmpty())
    {
        const QString strHistoryPath = qEnvironmentVariable("FLOWDROP_HISTORY_PATH");
        m_strTaskJournalPath = strHistoryPath.isEmpty()
                                   ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral(
                                         "/tasks.json")
                                   : QFileInfo(strHistoryPath).absolutePath() + QStringLiteral("/tasks.json");
    }
    QDir().mkpath(QFileInfo(m_strTaskJournalPath).absolutePath());
    m_pJournalLock = new QLockFile(m_strTaskJournalPath + QStringLiteral(".lock"));
    m_bJournalWritable = m_pJournalLock->tryLock(0);
    if (m_bJournalWritable) { LoadTaskJournal(); }
    else
    {
        QTimer::singleShot(0, this, [this] { ShowError(QStringLiteral("另一个实例或目录权限阻止访问任务记录；本实例不恢复或覆盖该记录，传输仍可使用。")); });
    }
    m_pQueueTimer = new QTimer(this);
    m_pQueueTimer->setInterval(250);
    connect(m_pQueueTimer, &QTimer::timeout, this, &AppController::ScheduleTasks);
    m_pQueueTimer->start();
}

void AppController::QueueRecordWrite()
{
    if (m_pJournalTimer && !m_pJournalTimer->isActive() && !m_bQueueClosing) { m_pJournalTimer->start(1000); }
}

void AppController::PersistTaskJournal()
{
    if (!m_bJournalWritable) { return; }
    QJsonArray arrRecords;
    for (const QString &strId: m_lstManagedOrder)
    {
        const STaskSnapshot stTask = m_pTaskModel->Snapshot(strId);
        if (stTask.bTerminal) { continue; }
        const SManagedTask stManaged = m_hashManagedTasks.value(strId);
        QJsonObject objTask;
        objTask.insert(QStringLiteral("id"), strId);
        objTask.insert(QStringLiteral("direction"), stTask.enDirection == ETaskDirection::DIRECTION_SEND
                                                        ? QStringLiteral("send")
                                                        : QStringLiteral("receive"));
        objTask.insert(QStringLiteral("name"), stTask.strDisplayName);
        objTask.insert(QStringLiteral("target"), stTask.strTargetDir);
        objTask.insert(QStringLiteral("sources"), QJsonArray::fromStringList(stTask.lstSourcePaths));
        objTask.insert(QStringLiteral("manifest"), QJsonArray::fromVariantList(stTask.lstFiles));
        objTask.insert(QStringLiteral("results"), QJsonArray::fromVariantList(stTask.m_lstResultFiles));
        if (!stManaged.m_hashPartialFiles.isEmpty())
        { objTask.insert(QStringLiteral("partial_files"), QJsonObject::fromVariantMap(stManaged.m_hashPartialFiles)); }
        QVariantMap hashConfig = stTask.hashConfig;
        hashConfig.remove(QStringLiteral("password"));
        hashConfig.remove(QStringLiteral("queueManaged"));
        objTask.insert(QStringLiteral("config"), QJsonObject::fromVariantMap(hashConfig));
        objTask.insert(
            QStringLiteral("needs_relay_password"),
            stManaged.m_bNeedsRelayPassword || !stManaged.m_stConfig.strRelayPass.isEmpty());
        arrRecords.append(objTask);
    }
    const QByteArray arrData = QJsonDocument(QJsonObject{
        {QStringLiteral("schema"), 1}, {QStringLiteral("tasks"), arrRecords}
    }).toJson();
    QSaveFile fileJournal(m_strTaskJournalPath);
    if (arrData.size() > 64 * 1024 * 1024 || !fileJournal.open(QIODevice::WriteOnly)
        || fileJournal.write(arrData) != arrData.size() || !fileJournal.commit())
    {
        fileJournal.cancelWriting();
        m_bJournalWritable = false; // 不持续刷屏、不覆盖先前的可靠记录。
        ShowError(QStringLiteral("任务记录未能原子保存，旧记录保留。请检查权限、空间或任务清单大小；本轮恢复记录可能不完整。"));
    }
}

void AppController::LoadTaskJournal()
{
    QFile fileJournal(m_strTaskJournalPath);
    if (!fileJournal.exists()) { return; }
    QJsonParseError stParse;
    if (!fileJournal.open(QIODevice::ReadOnly) || fileJournal.size() > 64 * 1024 * 1024)
    {
        m_bJournalWritable = false;
        return;
    }
    const QJsonObject objJournal = QJsonDocument::fromJson(fileJournal.readAll(), &stParse).object();
    const QJsonArray arrRecords = objJournal.value(QStringLiteral("tasks")).toArray();
    if (stParse.error != QJsonParseError::NoError || objJournal.value(QStringLiteral("schema")).toInt() != 1 ||
        arrRecords.size() > 128)
    {
        m_bJournalWritable = false;
        QTimer::singleShot(0, this, [this] { ShowError(QStringLiteral("任务记录格式异常，原文件保留且未自动连接。请先备份并检查tasks.json。")); });
        return;
    }
    for (const QJsonValue &varRecord: arrRecords)
    {
        const QJsonObject objTask = varRecord.toObject();
        const QString strId = objTask.value(QStringLiteral("id")).toString();
        if (!QRegularExpression(QStringLiteral("^(send|recv)-[0-9a-fA-F-]{36}$")).match(strId).hasMatch() ||
            m_hashManagedTasks.contains(strId)) { continue; }
        ConfigStore storeSnapshot;
        QVariantMap hashConfig = objTask.value(QStringLiteral("config")).toObject().toVariantMap();
        hashConfig.remove(QStringLiteral("password"));
        storeSnapshot.SetValues(hashConfig);
        if (!ConfigStore::Validate(storeSnapshot.GetValues()).isEmpty()) { continue; }
        STaskSnapshot stTask;
        stTask.strTaskId = strId;
        stTask.strDisplayName = objTask.value(QStringLiteral("name")).toString();
        stTask.enDirection = objTask.value(QStringLiteral("direction")).toString() == QStringLiteral("send")
                                 ? ETaskDirection::DIRECTION_SEND
                                 : ETaskDirection::DIRECTION_RECEIVE;
        stTask.strTargetDir = objTask.value(QStringLiteral("target")).toString();
        for (const QJsonValue &varPath: objTask.value(QStringLiteral("sources")).toArray())
        {
            stTask.lstSourcePaths.append(varPath.toString());
        }
        stTask.lstFiles = objTask.value(QStringLiteral("manifest")).toArray().toVariantList();
        if ((stTask.enDirection == ETaskDirection::DIRECTION_SEND && (
                 stTask.lstSourcePaths.isEmpty() || stTask.lstFiles.isEmpty()))
            || (stTask.enDirection == ETaskDirection::DIRECTION_RECEIVE && !QFileInfo(stTask.strTargetDir).
                isAbsolute())) { continue; }
        stTask.m_lstResultFiles = objTask.value(QStringLiteral("results")).toArray().toVariantList();
        stTask.nFileCount = stTask.lstFiles.size();
        for (const QVariant &varFile: stTask.lstFiles)
        {
            stTask.llTaskBytes += varFile.toMap().value(QStringLiteral("size")).toLongLong();
        }
        stTask.hashConfig = storeSnapshot.GetValues();
        stTask.hashConfig.remove(QStringLiteral("password"));
        stTask.enStage = ETaskStage::STAGE_INTERRUPTED;
        stTask.strError = QStringLiteral("上次任务已中断，当前未连接。恢复前检查本地文件，并确认双方在线；接收需重新输入对方有效口令。");
        SManagedTask stManaged;
        stManaged.m_bHoldForUser = true;
        if (stTask.enDirection == ETaskDirection::DIRECTION_RECEIVE)
        { stManaged.m_hashPartialFiles = objTask.value(QStringLiteral("partial_files")).toObject().toVariantMap(); }
        stManaged.m_bNeedsRelayPassword = objTask.value(QStringLiteral("needs_relay_password")).toBool();
        stManaged.m_stConfig = storeSnapshot.ToCrocConfig(m_strCrocPath, m_stConfig.strWorkHome);
        m_hashManagedTasks.insert(strId, stManaged);
        m_lstManagedOrder.append(strId);
        m_pTaskModel->ApplySnapshot(stTask);
    }
    if (!m_hashManagedTasks.isEmpty())
    {
        ++m_nTaskRevision;
        emit taskRevisionChanged();
    }
}

QString AppController::EnqueueTask(STaskSnapshot _stTask, const QString &_strCode)
{
    int nPending = 0;
    for (const QString &strId: m_lstManagedOrder)
    {
        const STaskSnapshot stExisting = m_pTaskModel->Snapshot(strId);
        if (stExisting.bTerminal) { continue; }
        ++nPending;
        const SManagedTask stExistingManaged = m_hashManagedTasks.value(strId);
        if (stExisting.enDirection == _stTask.enDirection && (
                !_strCode.isEmpty() && stExistingManaged.m_strCode == _strCode))
        {
            ShowError(QStringLiteral("同方向已有相同口令的任务，请查看原任务，避免连接混淆。"));
            return QString();
        }
        if (_stTask.enDirection == ETaskDirection::DIRECTION_SEND && stExisting.enDirection == _stTask.enDirection &&
            stExisting.lstSourcePaths == _stTask.lstSourcePaths)
        {
            ShowError(QStringLiteral("同一文件清单已有待办任务，请恢复或取消原任务。"));
            return QString();
        }
    }
    if (nPending >= 128)
    {
        ShowError(QStringLiteral("待办已达128项，请先完成或取消部分任务。"));
        return QString();
    }
    _stTask.hashConfig = GetSettings();
    _stTask.hashConfig.remove(QStringLiteral("password"));
    _stTask.hashConfig.insert(QStringLiteral("relay"), m_stConfig.strRelay);
    _stTask.hashConfig.insert(
        QStringLiteral("curve"), m_stConfig.strCurve.isEmpty() ? QStringLiteral("p256") : m_stConfig.strCurve);
    _stTask.hashConfig.insert(
        QStringLiteral("policy"), m_stConfig.bLocalOnly ? QStringLiteral("仅局域网") : QStringLiteral("自动连接"));
    _stTask.hashConfig.insert(QStringLiteral("lanAddress"), m_stConfig.m_strLanAddress);
    _stTask.hashConfig.insert(QStringLiteral("rate"), QString::number(m_stConfig.dUploadLimitMbps));
    _stTask.enStage = ETaskStage::STAGE_QUEUED;
    SManagedTask stManaged;
    stManaged.m_stConfig = m_stConfig;
    stManaged.m_strCode = _strCode;
    stManaged.m_llLastActivity = m_timerQueueClock.elapsed();
    m_hashManagedTasks.insert(_stTask.strTaskId, stManaged);
    m_lstManagedOrder.append(_stTask.strTaskId);
    m_pTaskModel->ApplySnapshot(_stTask);
    ++m_nTaskRevision;
    emit taskRevisionChanged();
    PersistTaskJournal();
    ScheduleTasks();
    return _stTask.strTaskId;
}

bool AppController::CanGrantTransfer(const QString &_strTaskId) const
{
    const STaskSnapshot stTask = m_pTaskModel->Snapshot(_strTaskId);
    const QString strKey = stTask.enDirection == ETaskDirection::DIRECTION_SEND
                               ? QStringLiteral("sendLimit")
                               : QStringLiteral("receiveLimit");
    int nLimit = stTask.hashConfig.value(strKey, 2).toInt(), nActive = 0;
    for (const QString &strId: m_lstManagedOrder)
    {
        if (strId == _strTaskId || !m_hashManagedTasks.value(strId).m_bTransferSlot) { continue; }
        const STaskSnapshot stOther = m_pTaskModel->Snapshot(strId);
        if (stOther.enDirection == stTask.enDirection)
        {
            ++nActive;
            nLimit = qMin(nLimit, stOther.hashConfig.value(strKey, 2).toInt());
        }
    }
    return nActive < nLimit;
}

bool AppController::AnswerManagedPrompt(const QString &_strTaskId, const QString &_strPromptId, int _nDecision)
{
    auto itTask = m_hashManagedTasks.find(_strTaskId);
    if (itTask == m_hashManagedTasks.end() || !itTask->m_bEngineRunning || itTask->m_bUserCancelled) { return false; }
    const bool bInitialReceive = !m_lstPrompts.isEmpty() && m_lstPrompts.first().strTaskId == _strTaskId
                                 && m_lstPrompts.first().m_strKind == QStringLiteral("receive_accept");
    if (bInitialReceive && _nDecision == 1 && !CanGrantTransfer(_strTaskId))
    {
        itTask->m_strGatePromptId = _strPromptId;
        itTask->m_nGateDecision = 1;
        STaskSnapshot stTask = m_pTaskModel->Snapshot(_strTaskId);
        stTask.enStage = ETaskStage::STAGE_QUEUED;
        stTask.strError = QStringLiteral("已同意接收，等待接收传输名额；尚未开始写入文件。");
        m_pTaskModel->ApplySnapshot(stTask);
        ++m_nTaskRevision;
        emit taskRevisionChanged();
        QueueRecordWrite();
        return true;
    }
    if (!m_pAdapter->AnswerDllPrompt(_strTaskId, _strPromptId, _nDecision)) { return false; }
    if (bInitialReceive && _nDecision == 1) { itTask->m_bTransferSlot = true; }
    if (_nDecision == 0 || _nDecision == 3) { itTask->m_bRejected = true; }
    return true;
}

void AppController::TryGrantTransfers()
{
    const QStringList lstOrder = m_lstManagedOrder;
    for (const QString &strId: lstOrder)
    {
        auto itTask = m_hashManagedTasks.find(strId);
        if (itTask == m_hashManagedTasks.end() || itTask->m_strGatePromptId.isEmpty() || !itTask->m_bEngineRunning ||
            itTask->m_bUserCancelled) { continue; }
        if (!CanGrantTransfer(strId))
        {
            STaskSnapshot stTask = m_pTaskModel->Snapshot(strId);
            stTask.enStage = ETaskStage::STAGE_QUEUED;
            stTask.strError = QStringLiteral("对端已连接，等待本方向的传输名额。");
            m_pTaskModel->ApplySnapshot(stTask);
            continue;
        }
        const QString strPrompt = itTask->m_strGatePromptId;
        if (m_pAdapter->AnswerDllPrompt(strId, strPrompt, itTask->m_nGateDecision))
        {
            itTask->m_bTransferSlot = true;
            itTask->m_strGatePromptId.clear();
            itTask->m_llLastActivity = m_timerQueueClock.elapsed();
        }
        else
        {
            itTask->m_strGatePromptId.clear();
            m_pAdapter->Cancel(strId);
        }
    }
}

bool AppController::IsRecoverableFailure(const STaskSnapshot &_stTask) const
{
    if (_stTask.m_strErrorCode == QStringLiteral("network_error")) { return true; }
    if (!_stTask.m_strErrorCode.isEmpty() && _stTask.m_strErrorCode != QStringLiteral("engine_error")) { return false; }
    const QString strError = _stTask.strError.toLower();
    return strError.contains(QStringLiteral("could not connect")) || strError.contains(
               QStringLiteral("connection reset"))
           || strError.contains(QStringLiteral("connection refused")) || strError.contains(
               QStringLiteral("connection closed"))
           || strError.contains(QStringLiteral("i/o timeout")) || strError.contains(QStringLiteral("unexpected eof"))
           || strError == QStringLiteral("eof");
}

void AppController::ApplyManagedSnapshot(const STaskSnapshot &_stSnapshot)
{
    auto itTask = m_hashManagedTasks.find(_stSnapshot.strTaskId);
    if (itTask == m_hashManagedTasks.end()) { return; }
    STaskSnapshot stTask = _stSnapshot;
    if (stTask.enDirection == ETaskDirection::DIRECTION_RECEIVE)
    {
        for (const QVariant& varFile : stTask.m_lstResultFiles)
        {
            const QVariantMap hashFile = varFile.toMap();
            const QString strPath = hashFile.value(QStringLiteral("path")).toString();
            if (!IsIncompleteReceiveFile(hashFile))
            {
                itTask->m_hashPartialFiles.remove(strPath);
                continue;
            }
            QString strIdentity = hashFile.value(QStringLiteral("created_file_identity")).toString();
            if (strIdentity.isEmpty() && hashFile.value(QStringLiteral("session_bytes")).toLongLong() > 0)
            { strIdentity = itTask->m_hashPartialFiles.value(strPath).toString().section(QLatin1Char('/'), 0, 0); }
            if (strIdentity.isEmpty()) { continue; }
            const QString strToken = TransferSafety::PartialFileToken(stTask.strTargetDir, strPath);
            if (strToken.startsWith(strIdentity + QLatin1Char('/'))) { itTask->m_hashPartialFiles.insert(strPath, strToken); }
        }
    }
    if (stTask.bTerminal) { itTask->m_bRetryEligible = stTask.nExitCode != 0 && IsRecoverableFailure(stTask); }
    const STaskSnapshot stPrevious = m_pTaskModel->Snapshot(stTask.strTaskId);
    if (!stTask.bTerminal && stTask.bShareReady) { itTask->m_llRetryDeadline = 0; }
    if (stTask.enStage == ETaskStage::STAGE_TRANSFERRING && itTask->m_bTransferSlot
        && stTask.llDoneBytes > itTask->m_llLastBytes && !stTask.strSpeed.isEmpty())
    {
        itTask->m_llRetryDeadline = 0; // 已恢复实际传输，重连上限不能误杀后续健康的长传输。
        itTask->m_bEverTransferred = true;
    }
    if (stTask.llDoneBytes != itTask->m_llLastBytes || stTask.enStage != stPrevious.enStage)
    {
        itTask->m_llLastActivity = m_timerQueueClock.elapsed();
        itTask->m_llLastBytes = stTask.llDoneBytes;
    }
    if (stTask.bTerminal && !itTask->m_bUserCancelled && !itTask->m_bRejected && !itTask->m_bExpired
        && (itTask->m_bInternalReconnect || (stTask.nExitCode != 0 && IsRecoverableFailure(stTask)))
        && stTask.hashConfig.value(QStringLiteral("reconnectWindow"), 600).toInt() > 0)
    {
        stTask.bTerminal = false;
        stTask.enStage = ETaskStage::STAGE_RECONNECTING;
        stTask.strError = QStringLiteral("连接中断，保留记录和部分文件，准备重新连接；请确认双方在线，必要时重新分享口令。");
    }
    if (!itTask->m_strGatePromptId.isEmpty() && !stTask.bTerminal) { stTask.enStage = ETaskStage::STAGE_QUEUED; }
    else if (!itTask->m_bTransferSlot && stTask.enStage == ETaskStage::STAGE_TRANSFERRING && !stTask.bTerminal)
    {
        stTask.enStage = ETaskStage::STAGE_AWAITING_CONFIRM;
    }
    m_pTaskModel->ApplySnapshot(stTask);
    ++m_nTaskRevision;
    emit taskRevisionChanged();
    QueueRecordWrite();
}

bool AppController::HandleManagedFinish(const QString &_strTaskId, int _nExitCode, bool _bCancelled,
                                        const QString &_strError)
{
    auto itTask = m_hashManagedTasks.find(_strTaskId);
    if (itTask == m_hashManagedTasks.end()) { return false; }
    itTask->m_bEngineRunning = false;
    itTask->m_bTransferSlot = false;
    itTask->m_strGatePromptId.clear();
    RemovePrompts(_strTaskId);
    STaskSnapshot stTask = m_pTaskModel->Snapshot(_strTaskId);
    const int nWindow = stTask.hashConfig.value(QStringLiteral("reconnectWindow"), 600).toInt();
    const qint64 llNow = m_timerQueueClock.elapsed();
    if (_nExitCode == 0 && !_bCancelled && !itTask->m_bInternalReconnect)
    {
        itTask->m_llRetryDeadline = 0;
        itTask->m_bExpired = false;
    }
    if (itTask->m_bExpired || (itTask->m_llRetryDeadline > 0 && llNow >= itTask->m_llRetryDeadline))
    {
        itTask->m_bHoldForUser = true;
        itTask->m_bInternalReconnect = false;
        itTask->m_strCode.clear();
        stTask.bTerminal = false;
        stTask.bShareReady = false;
        stTask.strCode.clear();
        stTask.enStage = ETaskStage::STAGE_INTERRUPTED;
        stTask.strError = QStringLiteral("自动重连等待上限已到，当前引擎会话已结束。任务记录和部分文件保留；可手动恢复，必要时重新分享口令。");
        m_pTaskModel->ApplySnapshot(stTask);
        ++m_nTaskRevision;
        emit taskRevisionChanged();
        PersistTaskJournal();
        return true;
    }
    if (!itTask->m_bUserCancelled && !itTask->m_bRejected && nWindow > 0
        && (itTask->m_bInternalReconnect || (_nExitCode != 0 && !_bCancelled && (
                                                 itTask->m_bRetryEligible || IsRecoverableFailure(stTask)))))
    {
        if (itTask->m_llRetryDeadline == 0) { itTask->m_llRetryDeadline = llNow + nWindow * 1000LL; }
        itTask->m_bInternalReconnect = false;
        itTask->m_llNextAttempt = llNow + qMin(15000, 1000 * (1 << qMin(itTask->m_nAttempt, 4)));
        stTask.bTerminal = false;
        stTask.bShareReady = false;
        stTask.strCode.clear();
        stTask.enStage = ETaskStage::STAGE_RECONNECTING;
        stTask.strError = QStringLiteral("重新连接中；请检查网络和中继。部分文件保留，必要时重新分享口令。");
        if (!_strError.isEmpty()) { stTask.strError += QLatin1Char('\n') + TransferSafety::Redact(_strError, {itTask->m_strCode, itTask->m_stConfig.strRelayPass}); }
        m_pTaskModel->ApplySnapshot(stTask);
        ++m_nTaskRevision;
        emit taskRevisionChanged();
        PersistTaskJournal();
        return true;
    }
    stTask.bTerminal = true;
    if (itTask->m_bRejected && _nExitCode != 0 && !_bCancelled) { stTask.enStage = ETaskStage::STAGE_SKIPPED; }
    m_pTaskModel->ApplySnapshot(stTask);
    itTask->m_strCode.clear();
    PersistTaskJournal();
    QTimer::singleShot(0, this, &AppController::ScheduleTasks);
    return false;
}

void AppController::ScheduleTasks()
{
    if (!m_pAdapter || m_bQueueClosing || m_bScheduling) { return; }
    m_bScheduling = true;
    const qint64 llNow = m_timerQueueClock.elapsed();
    const QStringList lstOrder = m_lstManagedOrder;
    for (const QString &strId: lstOrder)
    {
        auto itTask = m_hashManagedTasks.find(strId);
        if (itTask == m_hashManagedTasks.end() || itTask->m_bHoldForUser || itTask->m_bUserCancelled) { continue; }
        STaskSnapshot stTask = m_pTaskModel->Snapshot(strId);
        if (stTask.bTerminal) { continue; }
        if (itTask->m_llRetryDeadline > 0 && llNow >= itTask->m_llRetryDeadline)
        {
            itTask->m_bExpired = true;
            if (itTask->m_bEngineRunning) { m_pAdapter->Cancel(strId); }
            else { HandleManagedFinish(strId, 1, false, QString()); }
            continue;
        }
        if (itTask->m_bEngineRunning)
        {
            if (itTask->m_bTransferSlot && itTask->m_bEverTransferred && stTask.enStage != ETaskStage::STAGE_VERIFYING
                && stTask.enStage != ETaskStage::STAGE_SAVING && stTask.enStage != ETaskStage::STAGE_AWAITING_CONFIRM
                && llNow - itTask->m_llLastActivity > 15000
                && stTask.hashConfig.value(QStringLiteral("reconnectWindow"), 600).toInt() > 0)
            {
                // 避免底层长读超时把界面留在假传输状态；取消只针对本次引擎尝试。
                if (itTask->m_llRetryDeadline == 0)
                {
                    itTask->m_llRetryDeadline = llNow + stTask.hashConfig.value(QStringLiteral("reconnectWindow"), 600).
                                                toInt() * 1000LL;
                }
                itTask->m_bInternalReconnect = true;
                m_pAdapter->Cancel(strId);
            }
            continue;
        }
        if (llNow < itTask->m_llNextAttempt) { continue; }
        int nSessions = 0;
        bool bDirectoryBusy = false;
        for (const QString &strOtherId: lstOrder)
        {
            if (!m_hashManagedTasks.value(strOtherId).m_bEngineRunning) { continue; }
            const STaskSnapshot stOther = m_pTaskModel->Snapshot(strOtherId);
            if (stOther.enDirection == stTask.enDirection) { ++nSessions; }
            if (stTask.enDirection == ETaskDirection::DIRECTION_RECEIVE && stOther.enDirection == stTask.enDirection
                && (stOther.strTargetDir.compare(stTask.strTargetDir, Qt::CaseInsensitive) == 0
                    || stOther.strTargetDir.startsWith(stTask.strTargetDir + QLatin1Char('/'), Qt::CaseInsensitive)
                    || stTask.strTargetDir.startsWith(stOther.strTargetDir + QLatin1Char('/'), Qt::CaseInsensitive)))
            {
                bDirectoryBusy = true;
            }
        }
        const int nLimit = stTask.hashConfig.value(stTask.enDirection == ETaskDirection::DIRECTION_SEND
                                                       ? QStringLiteral("sendLimit")
                                                       : QStringLiteral("receiveLimit"), 2).toInt();
        if (bDirectoryBusy || nSessions >= qMin(8, nLimit + 4)) { continue; }
        QString strStartError;
        if (stTask.enDirection == ETaskDirection::DIRECTION_SEND && !TransferSafety::FilesUnchanged(stTask.lstFiles))
        {
            strStartError = QStringLiteral("开始或续传前发现源文件缺失、替换或修改，请重新选择文件创建新任务。");
        }
        if (stTask.enDirection == ETaskDirection::DIRECTION_RECEIVE)
        {
            QTemporaryFile fileProbe(QDir(stTask.strTargetDir).filePath(QStringLiteral(".crocd-resume-XXXXXX")));
            if (!QFileInfo(stTask.strTargetDir).isDir() || QDir(stTask.strTargetDir).canonicalPath().compare(
                    stTask.strTargetDir, Qt::CaseInsensitive) != 0
                || !fileProbe.open() || fileProbe.write("probe") != 5 || !fileProbe.flush())
            {
                strStartError = QStringLiteral("恢复前目标目录不可写、不存在或实际位置改变，请新建接收任务并重新选择目录；部分文件保留。");
            }
        }
        if (strStartError.isEmpty())
        {
            const SCrocConfig stConfig = itTask->m_stConfig;
            const QString strCode = itTask->m_strCode;
            itTask->m_bEngineRunning = true;
            itTask->m_bRetryEligible = false;
            ++itTask->m_nAttempt;
            itTask->m_llLastActivity = llNow;
            itTask->m_llLastBytes = 0;
            QVariantMap hashConfig = stTask.hashConfig;
            hashConfig.insert(QStringLiteral("queueManaged"), true);
            m_pAdapter->SetConfig(stConfig);
            m_pAdapter->SetNextConfigSnapshot(hashConfig);
            const bool bStarted = stTask.enDirection == ETaskDirection::DIRECTION_SEND
                                      ? m_pAdapter->StartSend(strId, stTask.lstSourcePaths, strCode)
                                      : m_pAdapter->StartReceive(strId, strCode, stTask.strTargetDir);
            m_pAdapter->SetConfig(m_stConfig);
            if (!bStarted)
            {
                strStartError = m_pAdapter->GetLastStartError();
                if (strStartError.isEmpty()) { strStartError = QStringLiteral("引擎未能启动，请检查配置。晚些时候可重新创建任务。"); }
            }
        }
        if (!strStartError.isEmpty())
        {
            stTask.enStage = ETaskStage::STAGE_FAILED;
            stTask.bTerminal = true;
            stTask.nExitCode = 1;
            stTask.strError = strStartError;
            ApplyManagedSnapshot(stTask);
            if (!HandleManagedFinish(strId, 1, false, strStartError)) { CompleteTask(strId, 1, false, strStartError); }
        }
    }
    TryGrantTransfers();
    m_bScheduling = false;
    if (llNow - m_llQueueUiTick >= 1000)
    {
        m_llQueueUiTick = llNow;
        for (const SManagedTask &stManaged: m_hashManagedTasks)
        {
            if (stManaged.m_llRetryDeadline > 0 && !stManaged.m_bHoldForUser)
            {
                ++m_nTaskRevision;
                emit taskRevisionChanged();
                break;
            }
        }
    }
}

void AppController::discardInterruptedTask(const QString& _strTaskId)
{
    auto itTask = m_hashManagedTasks.find(_strTaskId);
    if (itTask == m_hashManagedTasks.end() || !itTask->m_bHoldForUser || itTask->m_bEngineRunning) { return; }
    const STaskSnapshot stTask = m_pTaskModel->Snapshot(_strTaskId);
    if (stTask.bTerminal) { return; }
    if (stTask.enDirection == ETaskDirection::DIRECTION_SEND) { cancelTask(_strTaskId); return; }
    if (!m_bJournalWritable) { ShowError(QStringLiteral("无法保存任务记录，请检查目录权限后重试。")); return; }

    const QString strRoot = QDir::cleanPath(stTask.strTargetDir);
    for (auto itOther = m_hashManagedTasks.cbegin(); itOther != m_hashManagedTasks.cend(); ++itOther)
    {
        if (!itOther->m_bEngineRunning) { continue; }
        const STaskSnapshot stOther = m_pTaskModel->Snapshot(itOther.key());
        if (stOther.enDirection != ETaskDirection::DIRECTION_RECEIVE) { continue; }
        const QString strOther = QDir::cleanPath(stOther.strTargetDir);
        if (strRoot.compare(strOther, Qt::CaseInsensitive) == 0
            || strOther.startsWith((strRoot.endsWith(QLatin1Char('/')) ? strRoot : strRoot + QLatin1Char('/')), Qt::CaseInsensitive)
            || strRoot.startsWith((strOther.endsWith(QLatin1Char('/')) ? strOther : strOther + QLatin1Char('/')), Qt::CaseInsensitive))
        { ShowError(QStringLiteral("该保存位置还有接收任务在运行，请等接收结束后再清理。")); return; }
    }

    int nRemoved = 0;
    bool bUnverified = false;
    bool bFailed = false;
    const QVariantMap hashPartial = itTask->m_hashPartialFiles;
    for (const QVariant& varFile : stTask.m_lstResultFiles)
    {
        const QVariantMap hashFile = varFile.toMap();
        const QString strPath = hashFile.value(QStringLiteral("path")).toString();
        if (IsIncompleteReceiveFile(hashFile) && !hashPartial.contains(strPath)
            && hashFile.value(QStringLiteral("bytes_done")).toLongLong() > 0) { bUnverified = true; }
    }
    for (auto itFile = hashPartial.cbegin(); itFile != hashPartial.cend(); ++itFile)
    {
        const QString strPath = itFile.key();
        const QString strToken = itFile.value().toString();
        const QString strCurrent = TransferSafety::PartialFileToken(strRoot, strPath);
        if (strCurrent.isEmpty() || strCurrent != strToken)
        {
            bUnverified |= QFileInfo::exists(strPath);
            continue;
        }
        if (!TransferSafety::RemovePartialFile(strRoot, strPath, strToken)) { bFailed = true; continue; }
        itTask->m_hashPartialFiles.remove(strPath);
        ++nRemoved;
    }
    if (bFailed)
    {
        PersistTaskJournal();
        ShowError(QStringLiteral("部分未完成文件无法删除，已保留中断记录。请关闭占用文件的程序后重试。"));
        return;
    }
    cancelTask(_strTaskId);
    if (!m_bJournalWritable) { return; }
    QString strMessage = QStringLiteral("已移除中断记录，清理 %1 个未完成文件。").arg(nRemoved);
    if (bUnverified) { strMessage += QStringLiteral("无法确认归属或已变化的文件仍在保存目录，请手动检查。"); }
    ShowError(strMessage);
}

void AppController::resumeTask(const QString &_strTaskId, const QString &_strCode)
{
    auto itTask = m_hashManagedTasks.find(_strTaskId);
    if (itTask == m_hashManagedTasks.end() || itTask->m_bEngineRunning || !itTask->m_bHoldForUser) { return; }
    STaskSnapshot stTask = m_pTaskModel->Snapshot(_strTaskId);
    if (stTask.enDirection == ETaskDirection::DIRECTION_RECEIVE
        && !QRegularExpression(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9-]{5,127}$")).match(_strCode).hasMatch())
    {
        ShowError(QStringLiteral("恢复接收前请输入对方当前有效口令；不能承诺旧口令仍能恢复。"));
        return;
    }
    if (stTask.enDirection == ETaskDirection::DIRECTION_SEND && !TransferSafety::FilesUnchanged(stTask.lstFiles))
    {
        ShowError(QStringLiteral("源文件已变化或丢失，不能沿用旧记录；请选择文件创建新发送任务。"));
        return;
    }
    if (itTask->m_bNeedsRelayPassword)
    {
        if (m_stConfig.strRelayPass.isEmpty() || m_stConfig.strRelay != itTask->m_stConfig.strRelay)
        {
            ShowError(QStringLiteral("原任务需要中继密码，请在设置中填写相同中继的会话密码后恢复。"));
            return;
        }
        itTask->m_stConfig.strRelayPass = m_stConfig.strRelayPass;
    }
    itTask->m_strCode = stTask.enDirection == ETaskDirection::DIRECTION_RECEIVE ? _strCode : QString();
    itTask->m_bHoldForUser = false;
    itTask->m_bUserCancelled = false;
    itTask->m_bRejected = false;
    itTask->m_bExpired = false;
    itTask->m_bInternalReconnect = false;
    itTask->m_llRetryDeadline = 0;
    itTask->m_llNextAttempt = 0;
    itTask->m_llLastActivity = m_timerQueueClock.elapsed();
    stTask.enStage = ETaskStage::STAGE_QUEUED;
    stTask.bTerminal = false;
    stTask.bShareReady = false;
    stTask.strCode.clear();
    stTask.strError = QStringLiteral("恢复记录已检查；等待双方在线后重新连接，由引擎检查可复用内容，无法匹配时重新接收。发送需重新分享就绪口令。");
    m_pTaskModel->ApplySnapshot(stTask);
    ++m_nTaskRevision;
    emit taskRevisionChanged();
    PersistTaskJournal();
    ScheduleTasks();
}
