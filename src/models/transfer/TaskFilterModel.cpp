/**
****************************************************************
* @file    TaskFilterModel.cpp
* @brief   按收发方向过滤任务列表的代理模型实现
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
****************************************************************
*/
#include "TaskFilterModel.h"  // 包含过滤模型头文件

TaskFilterModel::TaskFilterModel(QObject* _pParent)  // 初始化父对象
    : QSortFilterProxyModel(_pParent)
{
}

QString TaskFilterModel::GetDirection() const  // 返回过滤方向
{
    return m_strDirection;
}

void TaskFilterModel::SetDirection(const QString& _strDirection)  // 设置过滤方向
{
    if (m_strDirection == _strDirection)
    {
        return;
    }
    m_strDirection = _strDirection;
    invalidateFilter();
    emit DirectionChanged();
}

bool TaskFilterModel::filterAcceptsRow(int _nSourceRow, const QModelIndex& _stSourceParent) const  // 隐藏取消终态并判断方向
{
    const QModelIndex stIndex = sourceModel()->index(_nSourceRow, 0, _stSourceParent);
    if (sourceModel()->data(stIndex, TransferTaskModel::ROLE_CANCELLED).toBool()) { return false; }
    if (m_strDirection.isEmpty())
    {
        return true;
    }
    const QString strDirection = sourceModel()->data(stIndex, TransferTaskModel::ROLE_DIRECTION).toString();
    return strDirection == m_strDirection;
}
