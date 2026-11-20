/**
****************************************************************
* @file    CrocJsonMap.cpp
* @brief   引擎后端选择与 JSON 快照映射实现
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 快照 JSON 到任务快照的结构化映射
****************************************************************
*/
#include "CrocJsonMap.h"

#include <QJsonDocument>  // JSON 解析
#include <QJsonObject>  // JSON 对象
#include <QJsonArray>  // 文件清单
#include <QJsonValue>  // 字段读取
#include <QCoreApplication>  // 应用目录
#include <QDir>  // 路径
#include <QFileInfo>  // 库文件检查

namespace
{
/**
****************************************************************
* @brief 阶段字符串转枚举
* @param [IN] _strPhase 引擎 phase 字段
* @return 阶段枚举；未知阶段返回 STAGE_IDLE
* @others 只映射文档中列出的阶段名
* @author Ray
****************************************************************
*/
ETaskStage PhaseToStage(const QString& _strPhase)
{
    if (_strPhase == QStringLiteral("preparing")) { return ETaskStage::STAGE_STARTING; }
    if (_strPhase == QStringLiteral("connecting")) { return ETaskStage::STAGE_WAITING_PEER; }
    if (_strPhase == QStringLiteral("waiting_peer")) { return ETaskStage::STAGE_WAITING_PEER; }
    if (_strPhase == QStringLiteral("negotiating")) { return ETaskStage::STAGE_CONNECTED; }
    if (_strPhase == QStringLiteral("awaiting_confirmation")) { return ETaskStage::STAGE_AWAITING_CONFIRM; }
    if (_strPhase == QStringLiteral("transferring")) { return ETaskStage::STAGE_TRANSFERRING; }
    if (_strPhase == QStringLiteral("verifying")) { return ETaskStage::STAGE_VERIFYING; }
    if (_strPhase == QStringLiteral("completed")) { return ETaskStage::STAGE_COMPLETED; }
    if (_strPhase == QStringLiteral("cancelled")) { return ETaskStage::STAGE_CANCELLED; }
    if (_strPhase == QStringLiteral("failed")) { return ETaskStage::STAGE_FAILED; }
    return ETaskStage::STAGE_IDLE;
}
}

ECrocBackend ResolveCrocBackend(const QString& _strName)  // 解析后端名称
{
    const QString strName = _strName.trimmed();
    if (strName.compare(QStringLiteral("dll"), Qt::CaseInsensitive) == 0)
    {
        return ECrocBackend::BACKEND_DLL;
    }
    if (strName.compare(QStringLiteral("process"), Qt::CaseInsensitive) == 0)
    {
        return ECrocBackend::BACKEND_PROCESS;
    }
    // 自动模式：本地能找到引擎库就用库，不要求 croc.exe 存在
#ifdef Q_OS_WIN
    const QString strLibName = QStringLiteral("croc_engine.dll");
#else
    const QString strLibName = QStringLiteral("libcroc_engine.so");
#endif
    const QString strEnvLib = qEnvironmentVariable("FLOWDROP_CROC_ENGINE_LIB");
    if (!strEnvLib.isEmpty() && QFileInfo::exists(strEnvLib))
    {
        return ECrocBackend::BACKEND_DLL;
    }
    const QString strAppDir = QCoreApplication::applicationDirPath();
    if (QFileInfo::exists(QDir(strAppDir).filePath(strLibName)))
    {
        return ECrocBackend::BACKEND_DLL;
    }
#ifdef Q_OS_WIN
    const QString strDevLib = QDir::cleanPath(strAppDir + QStringLiteral("/../../Croc-lib/dist/windows-x64/") + strLibName);
#else
    const QString strDevLib = QDir::cleanPath(strAppDir + QStringLiteral("/../../Croc-lib/dist/linux-x64/") + strLibName);
#endif
    return QFileInfo::exists(strDevLib) ? ECrocBackend::BACKEND_DLL : ECrocBackend::BACKEND_PROCESS;
}

bool MapCrocSnapshotJson(const QByteArray& _arrJson, STaskSnapshot& _stSnapshot, QString& _strError)  // 快照映射
{
    _strError.clear();
    QJsonParseError stParseError;
    const QJsonDocument docSnapshot = QJsonDocument::fromJson(_arrJson, &stParseError);
    if ((QJsonParseError::NoError != stParseError.error) || !docSnapshot.isObject())
    {
        _strError = QStringLiteral("快照 JSON 解析失败：") + stParseError.errorString();
        return false;
    }
    const QJsonObject objSnapshot = docSnapshot.object();
    _stSnapshot.m_bAggregateProgress = true;
    _stSnapshot.m_strErrorCode = objSnapshot.value(QStringLiteral("error_code")).toString();
    if (objSnapshot.contains(QStringLiteral("local_relay_port"))) { _stSnapshot.m_strLocalRelayPort = objSnapshot.value(QStringLiteral("local_relay_port")).toString(); }
    if (objSnapshot.contains(QStringLiteral("peer_address"))) { _stSnapshot.strPeer = objSnapshot.value(QStringLiteral("peer_address")).toString(); }
    const QJsonObject objFile = objSnapshot.value(QStringLiteral("file")).toObject();
    if (!objFile.isEmpty()) {
        _stSnapshot.strDisplayName = objFile.value(QStringLiteral("name")).toString();
        _stSnapshot.nFileIndex = objFile.value(QStringLiteral("index")).toInt() + 1;
        _stSnapshot.llTotalBytes = objFile.value(QStringLiteral("size")).toInteger();
        _stSnapshot.m_dFilePercent = _stSnapshot.llTotalBytes > 0 ? qBound(0.0, 100.0 * objFile.value(QStringLiteral("bytes_done")).toDouble() / _stSnapshot.llTotalBytes, 100.0) : -1.0;
    } else { _stSnapshot.m_dFilePercent = -1.0; _stSnapshot.nFileIndex = 0; }
    const QJsonObject objVerify = objSnapshot.value(QStringLiteral("verification")).toObject();
    _stSnapshot.m_strVerifyStatus = objVerify.value(QStringLiteral("status")).toString();
    const QJsonValue varVerifyTotal = objVerify.value(QStringLiteral("bytes_total"));
    const QJsonValue varVerifyDone = objVerify.value(QStringLiteral("bytes_done"));
    _stSnapshot.m_dVerifyPercent = varVerifyTotal.isDouble() && varVerifyDone.isDouble() && varVerifyTotal.toDouble() > 0
        ? qBound(0.0, 100.0 * varVerifyDone.toDouble() / varVerifyTotal.toDouble(), 100.0) : -1.0;


    // 只映射真实存在的字段，缺失的保持原值
    if (objSnapshot.contains(QStringLiteral("task_id")))
    {
        _stSnapshot.strTaskId = QString::number(static_cast<qulonglong>(objSnapshot.value(QStringLiteral("task_id")).toDouble()));
    }
    const QString strDirection = objSnapshot.value(QStringLiteral("direction")).toString();
    if (!strDirection.isEmpty())
    {
        _stSnapshot.enDirection = (strDirection == QStringLiteral("send"))
                                  ? ETaskDirection::DIRECTION_SEND : ETaskDirection::DIRECTION_RECEIVE;
    }
    const QString strPhase = objSnapshot.value(QStringLiteral("phase")).toString();
    if (!strPhase.isEmpty())
    {
        _stSnapshot.enStage = PhaseToStage(strPhase);
    }
    const QJsonObject objTotals = objSnapshot.value(QStringLiteral("totals")).toObject();
    if (objTotals.contains(QStringLiteral("bytes_total")))
    {
        _stSnapshot.llTaskBytes = static_cast<qint64>(objTotals.value(QStringLiteral("bytes_total")).toDouble());
        _stSnapshot.bSizeExact = true;
    }
    if (objTotals.contains(QStringLiteral("bytes_done")))
    {
        _stSnapshot.llDoneBytes = static_cast<qint64>(objTotals.value(QStringLiteral("bytes_done")).toDouble());
    }
    if (objTotals.contains(QStringLiteral("files")))
    {
        _stSnapshot.nFileCount = objTotals.value(QStringLiteral("files")).toInt();
    }
    if (objSnapshot.contains(QStringLiteral("files"))) { _stSnapshot.m_lstResultFiles = objSnapshot.value(QStringLiteral("files")).toArray().toVariantList(); }
    _stSnapshot.m_nSkippedFiles = objTotals.value(QStringLiteral("files_skipped")).toInt();
    _stSnapshot.m_nCompletedFiles = objTotals.value(QStringLiteral("files_completed")).toInt();
    _stSnapshot.m_nFailedFiles = objTotals.value(QStringLiteral("files_failed")).toInt();
    const QJsonObject objTransfer = objSnapshot.value(QStringLiteral("transfer")).toObject();
    if (objTransfer.contains(QStringLiteral("percent")))
    {
        _stSnapshot.dPercent = objTransfer.value(QStringLiteral("percent")).isDouble() ? qBound(0.0, objTransfer.value(QStringLiteral("percent")).toDouble(), 100.0) : -1.0;
    }
    if (objTransfer.contains(QStringLiteral("speed_bps")))
    {
        const double dBytesPerSecond = objTransfer.value(QStringLiteral("speed_bps")).toDouble();
        _stSnapshot.strSpeed = (dBytesPerSecond > 0.0)
                               ? QString::number(dBytesPerSecond / 1e6, 'f', 1) + QStringLiteral(" MB/s") : QString();
    }
    if (objTransfer.contains(QStringLiteral("eta_seconds")))
    {
        const double dSeconds = objTransfer.value(QStringLiteral("eta_seconds")).toDouble();
        _stSnapshot.strEta = (dSeconds > 0.0) ? QString::number(static_cast<int>(dSeconds)) + QStringLiteral("s") : QString();
    }
    const QString strState = objSnapshot.value(QStringLiteral("state")).toString();
    _stSnapshot.bTerminal = strState == QStringLiteral("completed") || strState == QStringLiteral("failed") || strState == QStringLiteral("cancelled");
    return true;
}
