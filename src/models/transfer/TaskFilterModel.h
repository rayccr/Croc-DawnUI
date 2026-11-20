/**
****************************************************************
* @file    TaskFilterModel.h
* @brief   按收发方向过滤任务列表的代理模型
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
****************************************************************
*/
#ifndef FLOWDROP_TASK_FILTER_MODEL_H  // 防止头文件重复包含
#define FLOWDROP_TASK_FILTER_MODEL_H  // 定义头文件包含标记

#include <QSortFilterProxyModel>  // 代理模型基类

#include "TransferTaskModel.h"  // 任务模型角色

/**
****************************************************************
* @brief 按方向过滤的任务代理模型
* @param 无
* @return 无
* @others 首页收发两列各用一个实例，底层共用同一个任务模型
* @author Ray
****************************************************************
*/
class TaskFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString direction READ GetDirection WRITE SetDirection NOTIFY DirectionChanged)

public:
    /**
    ****************************************************************
    * @brief 构造过滤模型
    * @param [IN] _pParent 父对象
    * @return 无
    * @others 无
    * @author Ray
    ****************************************************************
    */
    explicit TaskFilterModel(QObject* _pParent = nullptr);

    /**
    ****************************************************************
    * @brief 获取过滤方向
    * @param 无
    * @return "send" 或 "receive"
    * @others 无
    * @author Ray
    ****************************************************************
    */
    QString GetDirection() const;

    /**
    ****************************************************************
    * @brief 设置过滤方向
    * @param [IN] _strDirection "send" 或 "receive"
    * @return 无
    * @others 方向变化会重新过滤
    * @author Ray
    ****************************************************************
    */
    void SetDirection(const QString& _strDirection);

signals:
    /**
    ****************************************************************
    * @brief 过滤方向变化
    * @param 无
    * @return 无
    * @others 无
    * @author Ray
    ****************************************************************
    */
    void DirectionChanged();

protected:
    /**
    ****************************************************************
    * @brief 判断某一行是否属于当前方向
    * @param [IN] _nSourceRow 源模型行号
    * @param [IN] _stSourceParent 源模型父索引
    * @return 属于当前方向返回 true
    * @others 无
    * @author Ray
    ****************************************************************
    */
    bool filterAcceptsRow(int _nSourceRow, const QModelIndex& _stSourceParent) const override;

private:
    QString m_strDirection;  // 当前过滤方向
};

#endif  // 结束头文件包含标记
