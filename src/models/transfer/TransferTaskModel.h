/**
****************************************************************
* @file    TransferTaskModel.h
* @brief   传输任务列表模型：把适配层上报的真实快照提供给界面
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
****************************************************************
*/
#ifndef FLOWDROP_TRANSFER_TASK_MODEL_H  // 防止头文件重复包含
#define FLOWDROP_TRANSFER_TASK_MODEL_H  // 定义头文件包含标记

#include <QAbstractListModel>  // 列表模型基类
#include <QList>  // 任务行容器

#include "CrocTypes.h"  // 任务快照

/**
****************************************************************
* @brief 传输任务列表模型
* @param 无
* @return 无
* @others 只接收适配层上报的快照，模型内部不产生任何推测的进度
* @author Ray
****************************************************************
*/
class TransferTaskModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /**
    ****************************************************************
    * @brief 模型角色
    * @param 无
    * @return 无
    * @others 供 QML 直接绑定的字段名
    * @author Ray
    ****************************************************************
    */
    enum ETaskRole
    {
        ROLE_TASK_ID = Qt::UserRole + 1,  // 任务标识
        ROLE_DIRECTION,                  // "send" / "receive"
        ROLE_NAME,                       // 文件名
        ROLE_STAGE_TEXT,                 // 阶段中文描述
        ROLE_PERCENT,                    // 百分比，负值表示无法计量
        ROLE_BYTES,                      // 总字节数
        ROLE_SPEED,                      // 速度文本
        ROLE_PEER,                       // 对端
        ROLE_CODE,                       // 口令
        ROLE_FILE_INDEX,                 // 多文件当前序号
        ROLE_FILE_COUNT,                 // 多文件总数
        ROLE_ERROR,                      // 错误说明
        ROLE_CANCELLED                   // 引擎已确认取消的终态
    };

    /**
    ****************************************************************
    * @brief 构造任务模型
    * @param [IN] _pParent 父对象
    * @return 无
    * @others 无
    * @author Ray
    ****************************************************************
    */
    explicit TransferTaskModel(QObject* _pParent = nullptr);

    int rowCount(const QModelIndex& _stParent = QModelIndex()) const override;  // 任务行数
    QVariant data(const QModelIndex& _stIndex, int _nRole = Qt::DisplayRole) const override;  // 读取任务字段
    QHash<int, QByteArray> roleNames() const override;  // 暴露给 QML 的角色名

    /**
    ****************************************************************
    * @brief 应用一次任务快照
    * @param [IN] _stSnapshot 适配层上报的快照
    * @return 无
    * @others 新任务追加到末尾，已有任务就地更新
    * @author Ray
    ****************************************************************
    */
    void ApplySnapshot(const STaskSnapshot& _stSnapshot);

    /**
    ****************************************************************
    * @brief 清空全部任务
    * @param 无
    * @return 无
    * @others 只清模型，不删除磁盘文件
    * @author Ray
    ****************************************************************
    */
    void Clear();

    /**
    ****************************************************************
    * @brief 查询任务在当前模型中的行号
    * @param [IN] _strTaskId 任务标识
    * @return 行号，未找到返回 -1
    * @others 无
    * @author Ray
    ****************************************************************
    */
    int FindRow(const QString& _strTaskId) const;
    STaskSnapshot Snapshot(const QString& _strTaskId) const;  // 返回指定任务完整副本

private:
    static QString StageText(ETaskStage _enStage);  // 阶段转中文描述

    QList<STaskSnapshot> m_lstSnapshots;  // 任务快照列表
};

#endif  // 结束头文件包含标记
