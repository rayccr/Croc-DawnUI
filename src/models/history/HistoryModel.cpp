/**
****************************************************************
* @file    HistoryModel.cpp
* @brief   传输历史模型实现：JSON Lines 持久化 + 内存过滤
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
****************************************************************
*/
#include "HistoryModel.h"  // 包含历史模型头文件

#include <QDateTime>  // 结束时间
#include <QDir>  // 目录创建
#include <QSaveFile>
#include <QJsonArray>
#include "TransferSafety.h"
#include <QFile>  // 记录文件
#include <QFileInfo>  // 路径检查
#include <QJsonDocument>  // JSON 解析
#include <QJsonObject>  // JSON 对象
#include <QStandardPaths>  // 用户数据目录

HistoryModel::HistoryModel(QObject* _pParent)  // 初始化父对象
    : QAbstractListModel(_pParent)
{
    m_strDirection = QStringLiteral("全部");
}

int HistoryModel::rowCount(const QModelIndex& _stParent) const  // 可见行数
{
    if (_stParent.isValid())
    {
        return 0;
    }
    return static_cast<int>(m_lstVisible.size());
}

QVariant HistoryModel::data(const QModelIndex& _stIndex, int _nRole) const  // 读取字段
{
    if (!_stIndex.isValid() || (_stIndex.row() < 0) || (_stIndex.row() >= m_lstVisible.size()))
    {
        return QVariant();
    }
    const SHistoryRecord& stRecord = m_lstRecords.at(m_lstVisible.at(_stIndex.row()));
    switch (_nRole)
    {
    case ROLE_TASK_ID:
        return stRecord.strTaskId;
    case ROLE_DIRECTION:
        return stRecord.strDirection;
    case ROLE_NAME:
        return stRecord.strName;
    case ROLE_SIZE_TEXT:
        return (stRecord.bSizeExact ? QString() : QStringLiteral("约 ")) + FormatSize(stRecord.llBytes);
    case ROLE_RESULT:
        return stRecord.strResult;
    case ROLE_PEER:
        return stRecord.strPeer;
    case ROLE_TIME_TEXT:
        return stRecord.strFinishedAt;
    case ROLE_ERROR:
        return stRecord.strError;
    case ROLE_LOCATION: return stRecord.strLocation;
    case ROLE_FILES: return stRecord.lstFiles.join(QLatin1Char('\n'));
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> HistoryModel::roleNames() const  // 角色名
{
    QHash<int, QByteArray> hashRoles;
    hashRoles.insert(ROLE_TASK_ID, QByteArrayLiteral("taskId"));
    hashRoles.insert(ROLE_DIRECTION, QByteArrayLiteral("direction"));
    hashRoles.insert(ROLE_NAME, QByteArrayLiteral("name"));
    hashRoles.insert(ROLE_SIZE_TEXT, QByteArrayLiteral("sizeText"));
    hashRoles.insert(ROLE_RESULT, QByteArrayLiteral("result"));
    hashRoles.insert(ROLE_PEER, QByteArrayLiteral("peer"));
    hashRoles.insert(ROLE_TIME_TEXT, QByteArrayLiteral("timeText"));
    hashRoles.insert(ROLE_ERROR, QByteArrayLiteral("error"));
    hashRoles.insert(ROLE_LOCATION, QByteArrayLiteral("location"));
    hashRoles.insert(ROLE_FILES, QByteArrayLiteral("files"));
    return hashRoles;
}

QString HistoryModel::GetFilePath() const  // 历史文件路径
{
    const QString strFromEnv = qEnvironmentVariable("FLOWDROP_HISTORY_PATH");
    if (!strFromEnv.isEmpty())
    {
        return strFromEnv;
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/history.jsonl");
}

bool HistoryModel::Load()  // 读取历史
{
    m_lstRecords.clear();
    const QString strPath = GetFilePath();
    QFile fileHistory(strPath);
    if (!fileHistory.exists() || !fileHistory.open(QIODevice::ReadOnly))
    {
        RebuildFilter();  // 没有文件就是空历史
        return true;
    }
    while (!fileHistory.atEnd())
    {
        const QByteArray arrLine = fileHistory.readLine().trimmed();
        if (arrLine.isEmpty())
        {
            continue;
        }
        const QJsonDocument docLine = QJsonDocument::fromJson(arrLine);
        if (!docLine.isObject())
        {
            continue;  // 跳过损坏行，不让单行错误毁掉整个历史
        }
        const QJsonObject objLine = docLine.object();
        SHistoryRecord stRecord;
        stRecord.strTaskId = objLine.value(QStringLiteral("taskId")).toString();
        stRecord.strDirection = objLine.value(QStringLiteral("direction")).toString();
        stRecord.strName = objLine.value(QStringLiteral("name")).toString();
        stRecord.llBytes = static_cast<qint64>(objLine.value(QStringLiteral("bytes")).toDouble());
        stRecord.nFileCount = objLine.value(QStringLiteral("files")).toInt();
        stRecord.strResult = objLine.value(QStringLiteral("result")).toString();
        stRecord.strPeer = objLine.value(QStringLiteral("peer")).toString();
        stRecord.strFinishedAt = objLine.value(QStringLiteral("finishedAt")).toString();
        stRecord.strError = TransferSafety::Redact(objLine.value(QStringLiteral("error")).toString());
        stRecord.strLocation = objLine.value(QStringLiteral("location")).toString();
        stRecord.bSizeExact = objLine.value(QStringLiteral("sizeExact")).toBool();
        for (const QJsonValue& varPath : objLine.value(QStringLiteral("filesList")).toArray()) { stRecord.lstFiles.append(varPath.toString()); }
        m_lstRecords.prepend(stRecord);  // 文件里是旧到新，内存里新在最前
    }
    fileHistory.close();
    RebuildFilter();
    return true;
}

void HistoryModel::Append(const SHistoryRecord& _stRecord)  // 追加记录
{
    beginResetModel();
    m_lstRecords.prepend(_stRecord);
    m_lstVisible.clear();
    for (int nIndex = 0; nIndex < m_lstRecords.size(); ++nIndex)
    {
        const SHistoryRecord& stRecord = m_lstRecords.at(nIndex);
        if (m_strDirection != QStringLiteral("全部") && stRecord.strDirection != m_strDirection)
        {
            continue;
        }
        if (!m_strSearch.isEmpty() && !stRecord.strName.contains(m_strSearch, Qt::CaseInsensitive))
        {
            continue;
        }
        m_lstVisible.append(nIndex);
    }
    endResetModel();
    emit countChanged();
    if (!Save()) { emit storageError(QStringLiteral("历史未能写入磁盘。请检查应用数据目录权限与磁盘空间；当前显示仍保留在内存中。")); }
}

void HistoryModel::setSearch(const QString& _strText)  // 搜索
{
    if (m_strSearch == _strText)
    {
        return;
    }
    m_strSearch = _strText;
    RebuildFilter();
}

void HistoryModel::setDirection(const QString& _strDirection)  // 方向过滤
{
    if (m_strDirection == _strDirection)
    {
        return;
    }
    m_strDirection = _strDirection;
    RebuildFilter();
}

void HistoryModel::clearAll()  // 清空历史
{
    const QList<SHistoryRecord> lstPrevious = m_lstRecords;
    beginResetModel();
    m_lstRecords.clear();
    m_lstVisible.clear();
    endResetModel();
    emit countChanged();
    if (!Save()) { m_lstRecords = lstPrevious; RebuildFilter(); emit storageError(QStringLiteral("清空历史未能写入磁盘，原记录已保留，请检查权限和空间。")); }
}

int HistoryModel::count() const  // 可见记录数
{
    return static_cast<int>(m_lstVisible.size());
}

void HistoryModel::RebuildFilter()  // 重建可见行
{
    beginResetModel();
    m_lstVisible.clear();
    for (int nIndex = 0; nIndex < m_lstRecords.size(); ++nIndex)
    {
        const SHistoryRecord& stRecord = m_lstRecords.at(nIndex);
        if (m_strDirection != QStringLiteral("全部") && stRecord.strDirection != m_strDirection)
        {
            continue;
        }
        if (!m_strSearch.isEmpty() && !stRecord.strName.contains(m_strSearch, Qt::CaseInsensitive))
        {
            continue;
        }
        m_lstVisible.append(nIndex);
    }
    endResetModel();
    emit countChanged();
}

bool HistoryModel::Save() const  // 写回磁盘
{
    const QString strPath = GetFilePath();
    const QFileInfo stInfo(strPath);
    if (!stInfo.dir().exists())
    {
        QDir().mkpath(stInfo.dir().absolutePath());
    }

    QSaveFile fileHistory(strPath);
    if (!fileHistory.open(QIODevice::WriteOnly))
    {
        return false;
    }
    // 文件里按旧到新写，读回时再反转
    for (int nIndex = m_lstRecords.size() - 1; nIndex >= 0; --nIndex)
    {
        const SHistoryRecord& stRecord = m_lstRecords.at(nIndex);
        QJsonObject objLine;
        objLine.insert(QStringLiteral("taskId"), stRecord.strTaskId);
        objLine.insert(QStringLiteral("direction"), stRecord.strDirection);
        objLine.insert(QStringLiteral("name"), stRecord.strName);
        objLine.insert(QStringLiteral("bytes"), static_cast<double>(stRecord.llBytes));
        objLine.insert(QStringLiteral("files"), stRecord.nFileCount);
        objLine.insert(QStringLiteral("result"), stRecord.strResult);
        objLine.insert(QStringLiteral("peer"), stRecord.strPeer);
        objLine.insert(QStringLiteral("finishedAt"), stRecord.strFinishedAt);
        objLine.insert(QStringLiteral("error"), TransferSafety::Redact(stRecord.strError));
        objLine.insert(QStringLiteral("location"), stRecord.strLocation);
        objLine.insert(QStringLiteral("sizeExact"), stRecord.bSizeExact);
        objLine.insert(QStringLiteral("filesList"), QJsonArray::fromStringList(stRecord.lstFiles));
        const QByteArray arrLine = QJsonDocument(objLine).toJson(QJsonDocument::Compact) + "\n";
        if (fileHistory.write(arrLine) != arrLine.size()) { fileHistory.cancelWriting(); return false; }
    }
    return fileHistory.commit();
}

QString HistoryModel::FormatSize(qint64 _llBytes)  // 字节数转文本
{
    if (_llBytes <= 0)
    {
        return QStringLiteral("—");
    }
    if (_llBytes >= 1000000000LL)
    {
        return QString::number(static_cast<double>(_llBytes) / 1e9, 'f', 1) + QStringLiteral(" GB");
    }
    if (_llBytes >= 1000000LL)
    {
        return QString::number(static_cast<double>(_llBytes) / 1e6, 'f', 1) + QStringLiteral(" MB");
    }
    if (_llBytes >= 1000LL)
    {
        return QString::number(static_cast<double>(_llBytes) / 1e3, 'f', 1) + QStringLiteral(" KB");
    }
    return QString::number(_llBytes) + QStringLiteral(" B");
}
