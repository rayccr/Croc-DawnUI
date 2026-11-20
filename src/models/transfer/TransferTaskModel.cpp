/**
****************************************************************
* @file    TransferTaskModel.cpp
* @brief   传输任务列表模型实现
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
****************************************************************
*/
#include "TransferTaskModel.h"  // 包含任务模型头文件

TransferTaskModel::TransferTaskModel(QObject* _pParent)  // 初始化父对象
    : QAbstractListModel(_pParent)
{
}

int TransferTaskModel::rowCount(const QModelIndex& _stParent) const  // 返回任务行数
{
    if (_stParent.isValid())
    {
        return 0;
    }
    return static_cast<int>(m_lstSnapshots.size());
}

QVariant TransferTaskModel::data(const QModelIndex& _stIndex, int _nRole) const  // 读取任务字段
{
    if (!_stIndex.isValid() || (_stIndex.row() < 0) || (_stIndex.row() >= m_lstSnapshots.size()))
    {
        return QVariant();
    }
    const STaskSnapshot& stSnapshot = m_lstSnapshots.at(_stIndex.row());
    switch (_nRole)
    {
    case ROLE_TASK_ID:
        return stSnapshot.strTaskId;
    case ROLE_DIRECTION:
        return ETaskDirection::DIRECTION_SEND == stSnapshot.enDirection ? QStringLiteral("send") : QStringLiteral("receive");
    case ROLE_NAME:
        return stSnapshot.strDisplayName;
    case ROLE_STAGE_TEXT:
        if (stSnapshot.enStage == ETaskStage::STAGE_WAITING_PEER && stSnapshot.enDirection == ETaskDirection::DIRECTION_RECEIVE)
        { return QStringLiteral("正在连接发送方"); }
        if (stSnapshot.m_bAggregateProgress && stSnapshot.enStage == ETaskStage::STAGE_WAITING_PEER && stSnapshot.enDirection == ETaskDirection::DIRECTION_SEND)
        { return stSnapshot.bShareReady ? QStringLiteral("已就绪，等待接收") : QStringLiteral("连接中，口令未就绪"); }
        return StageText(stSnapshot.enStage);
    case ROLE_PERCENT:
        return stSnapshot.m_bAggregateProgress && stSnapshot.enStage == ETaskStage::STAGE_VERIFYING ? stSnapshot.m_dVerifyPercent : stSnapshot.dPercent;
    case ROLE_BYTES:
        return (stSnapshot.llTaskBytes > 0) ? stSnapshot.llTaskBytes : stSnapshot.llTotalBytes;
    case ROLE_SPEED:
        return stSnapshot.strSpeed;
    case ROLE_PEER:
        return stSnapshot.strPeer;
    case ROLE_CODE:
        return stSnapshot.strCode;
    case ROLE_FILE_INDEX:
        return stSnapshot.nFileIndex;
    case ROLE_FILE_COUNT:
        return stSnapshot.nFileCount;
    case ROLE_ERROR:
        return stSnapshot.strError;
    case ROLE_CANCELLED:
        return stSnapshot.bTerminal && stSnapshot.enStage == ETaskStage::STAGE_CANCELLED;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> TransferTaskModel::roleNames() const  // 暴露给 QML 的角色名
{
    QHash<int, QByteArray> hashRoles;
    hashRoles.insert(ROLE_TASK_ID, QByteArrayLiteral("taskId"));
    hashRoles.insert(ROLE_DIRECTION, QByteArrayLiteral("direction"));
    hashRoles.insert(ROLE_NAME, QByteArrayLiteral("name"));
    hashRoles.insert(ROLE_STAGE_TEXT, QByteArrayLiteral("stageText"));
    hashRoles.insert(ROLE_PERCENT, QByteArrayLiteral("percent"));
    hashRoles.insert(ROLE_BYTES, QByteArrayLiteral("totalBytes"));
    hashRoles.insert(ROLE_SPEED, QByteArrayLiteral("speed"));
    hashRoles.insert(ROLE_PEER, QByteArrayLiteral("peer"));
    hashRoles.insert(ROLE_CODE, QByteArrayLiteral("code"));
    hashRoles.insert(ROLE_FILE_INDEX, QByteArrayLiteral("fileIndex"));
    hashRoles.insert(ROLE_FILE_COUNT, QByteArrayLiteral("fileCount"));
    hashRoles.insert(ROLE_ERROR, QByteArrayLiteral("error"));
    hashRoles.insert(ROLE_CANCELLED, QByteArrayLiteral("cancelled"));
    return hashRoles;
}

void TransferTaskModel::ApplySnapshot(const STaskSnapshot& _stSnapshot)  // 应用任务快照
{
    const int nRow = FindRow(_stSnapshot.strTaskId);
    if (nRow < 0)
    {
        beginInsertRows(QModelIndex(), static_cast<int>(m_lstSnapshots.size()), static_cast<int>(m_lstSnapshots.size()));
        m_lstSnapshots.append(_stSnapshot);
        endInsertRows();
        return;
    }
    m_lstSnapshots[nRow] = _stSnapshot;
    const QModelIndex stIndex = index(nRow, 0);
    emit dataChanged(stIndex, stIndex);
}

void TransferTaskModel::Clear()  // 清空全部任务
{
    if (m_lstSnapshots.isEmpty())
    {
        return;
    }
    beginResetModel();
    m_lstSnapshots.clear();
    endResetModel();
}

int TransferTaskModel::FindRow(const QString& _strTaskId) const  // 查询任务行号
{
    for (int nIndex = 0; nIndex < m_lstSnapshots.size(); ++nIndex)
    {
        if (m_lstSnapshots.at(nIndex).strTaskId == _strTaskId)
        {
            return nIndex;
        }
    }
    return -1;
}

QString TransferTaskModel::StageText(ETaskStage _enStage)  // 阶段转中文描述
{
    switch (_enStage)
    {
    case ETaskStage::STAGE_IDLE:
        return QStringLiteral("未开始");
    case ETaskStage::STAGE_STARTING:
        return QStringLiteral("启动中");
    case ETaskStage::STAGE_WAITING_PEER:
        return QStringLiteral("等待对端");
    case ETaskStage::STAGE_CONNECTED:
        return QStringLiteral("已连接");
    case ETaskStage::STAGE_TRANSFERRING:
        return QStringLiteral("传输中");
    case ETaskStage::STAGE_COMPLETED:
        return QStringLiteral("接收结束（引擎确认）");
    case ETaskStage::STAGE_SENT:
        return QStringLiteral("本端发送结束");
    case ETaskStage::STAGE_UNCONFIRMED:
        return QStringLiteral("接收结果待检查");
    case ETaskStage::STAGE_VERIFYING:
        return QStringLiteral("校验中");
    case ETaskStage::STAGE_AWAITING_CONFIRM:
        return QStringLiteral("等待确认");
    case ETaskStage::STAGE_QUEUED:
        return QStringLiteral("排队中");
    case ETaskStage::STAGE_RECONNECTING:
        return QStringLiteral("重新连接");
    case ETaskStage::STAGE_INTERRUPTED:
        return QStringLiteral("中断，等待恢复");
    case ETaskStage::STAGE_SAVING:
        return QStringLiteral("正在完成保存");
    case ETaskStage::STAGE_PARTIAL:
        return QStringLiteral("部分完成 / 有跳过");
    case ETaskStage::STAGE_SKIPPED:
        return QStringLiteral("未传输 / 已跳过");
    case ETaskStage::STAGE_CANCELLING:
        return QStringLiteral("取消中");
    case ETaskStage::STAGE_FAILED:
        return QStringLiteral("失败");
    case ETaskStage::STAGE_CANCELLED:
        return QStringLiteral("已取消");
    }
    return QStringLiteral("未知");
}

STaskSnapshot TransferTaskModel::Snapshot(const QString& _strTaskId) const
{
    const int nRow = FindRow(_strTaskId);
    return nRow >= 0 ? m_lstSnapshots.at(nRow) : STaskSnapshot();
}
