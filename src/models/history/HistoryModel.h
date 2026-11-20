/**
****************************************************************
* @file    HistoryModel.h
* @brief   传输历史：本地持久化与列表模型
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
****************************************************************
*/
#ifndef FLOWDROP_HISTORY_MODEL_H  // 防止头文件重复包含
#define FLOWDROP_HISTORY_MODEL_H  // 定义头文件包含标记

#include <QAbstractListModel>  // 列表模型基类
#include <QList>  // 记录容器
#include <QString>  // Qt 字符串

/**
****************************************************************
* @brief 一条历史记录
* @param 无
* @return 无
* @others 不保存口令、密钥或中继密码；只记录结果与文件信息
* @author Ray
****************************************************************
*/
struct SHistoryRecord
{
    QString strTaskId;      // 任务标识
    QString strDirection;   // "send" / "receive"
    QString strName;        // 文件名或文件数描述
    qint64 llBytes = 0;     // 总字节数，0 表示未知
    int nFileCount = 0;     // 文件数量，0 表示未知
    QString strResult;      // 已完成 / 已取消 / 失败
    QString strPeer;        // 对端
    QString strFinishedAt;  // 结束时间
    QString strError;       // 失败原因
    QString strLocation;   // 本地保存目录或发送源目录
    QStringList lstFiles;  // 实际文件清单
    bool bSizeExact = false;  // 容量是否精确
};

/**
****************************************************************
* @brief 历史记录模型
* @param 无
* @return 无
* @others 记录写入本地 JSON Lines 文件，启动时读回；搜索与方向过滤在内存中完成
* @author Ray
****************************************************************
*/
class HistoryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int visibleCount READ count NOTIFY countChanged)

public:
    /**
    ****************************************************************
    * @brief 模型角色
    * @param 无
    * @return 无
    * @others 供 QML 直接绑定
    * @author Ray
    ****************************************************************
    */
    enum EHistoryRole
    {
        ROLE_TASK_ID = Qt::UserRole + 1,  // 任务标识
        ROLE_DIRECTION,                  // 方向
        ROLE_NAME,                       // 名称
        ROLE_SIZE_TEXT,                  // 大小文本
        ROLE_RESULT,                     // 结果
        ROLE_PEER,                       // 对端
        ROLE_TIME_TEXT,                  // 时间
        ROLE_ERROR,                      // 错误
        ROLE_LOCATION,
        ROLE_FILES
    };

    /**
    ****************************************************************
    * @brief 构造历史模型
    * @param [IN] _pParent 父对象
    * @return 无
    * @others 构造后需调用 Load 才会读取磁盘
    * @author Ray
    ****************************************************************
    */
    explicit HistoryModel(QObject* _pParent = nullptr);

    int rowCount(const QModelIndex& _stParent = QModelIndex()) const override;  // 可见行数
    QVariant data(const QModelIndex& _stIndex, int _nRole = Qt::DisplayRole) const override;  // 读取字段
    QHash<int, QByteArray> roleNames() const override;  // 角色名

    /**
    ****************************************************************
    * @brief 从磁盘读取历史
    * @param 无
    * @return 读取成功返回 true
    * @others 文件不存在时按空历史处理，不当作错误
    * @author Ray
    ****************************************************************
    */
    bool Load();

    /**
    ****************************************************************
    * @brief 追加一条记录并落盘
    * @param [IN] _stRecord 记录
    * @return 无
    * @others 新记录排在最前；写盘失败只影响持久化，不影响当前显示
    * @author Ray
    ****************************************************************
    */
    void Append(const SHistoryRecord& _stRecord);

    /**
    ****************************************************************
    * @brief 按名称搜索
    * @param [IN] _strText 关键字，空串表示不过滤
    * @return 无
    * @others 大小写不敏感
    * @author Ray
    ****************************************************************
    */
    Q_INVOKABLE void setSearch(const QString& _strText);

    /**
    ****************************************************************
    * @brief 按方向过滤
    * @param [IN] _strDirection "发送" / "接收" / 其他表示全部
    * @return 无
    * @others 无
    * @author Ray
    ****************************************************************
    */
    Q_INVOKABLE void setDirection(const QString& _strDirection);

    /**
    ****************************************************************
    * @brief 清空全部历史记录
    * @param 无
    * @return 无
    * @others 只删记录文件，不删除任何用户文件
    * @author Ray
    ****************************************************************
    */
    Q_INVOKABLE void clearAll();

    /**
    ****************************************************************
    * @brief 当前可见记录数
    * @param 无
    * @return 行数
    * @others 供界面显示空状态
    * @author Ray
    ****************************************************************
    */
    Q_INVOKABLE int count() const;

    /**
    ****************************************************************
    * @brief 历史文件路径
    * @param 无
    * @return 绝对路径
    * @others 可用环境变量 FLOWDROP_HISTORY_PATH 覆盖，便于自检隔离
    * @author Ray
    ****************************************************************
    */
    QString GetFilePath() const;

signals:
    void countChanged();
    void storageError(const QString& _strText);

private:
    void RebuildFilter();  // 重建可见行
    bool Save() const;  // 写回磁盘
    static QString FormatSize(qint64 _llBytes);  // 字节数转可读文本

    QList<SHistoryRecord> m_lstRecords;  // 全量记录，newest first
    QList<int> m_lstVisible;  // 当前可见记录下标
    QString m_strSearch;  // 搜索关键字
    QString m_strDirection;  // 方向过滤
};

#endif  // 结束头文件包含标记
