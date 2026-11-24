/**
****************************************************************
* @file    AppController.h
* @brief   应用控制器：向 QML 提供真实任务模型与引擎操作入口
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
*          V1.0.1       Ray       2026-11-01    2. 接入 CrocAdapter 与任务模型
****************************************************************
*/
#ifndef FLOWDROP_APP_CONTROLLER_H  // 防止头文件重复包含
#define FLOWDROP_APP_CONTROLLER_H  // 定义头文件包含标记

#include <QObject>  // Qt 对象基类
#include <QString>  // Qt 字符串
#include <QVariantMap>  // 任务详情
#include <QHash>  // 口令等待计时表
#include <QList>
#include <QElapsedTimer>
#include <QLockFile>
#include <QSet>
#include <QtQml/qqmlregistration.h>  // QML 类型注册

#include "ConfigStore.h"  // 配置存储（Q_PROPERTY 需要完整类型）
#include "CrocTypes.h"  // 引擎配置与快照
#include "HistoryModel.h"  // 历史记录模型（Q_PROPERTY 需要完整类型）
#include "TaskFilterModel.h"  // 方向过滤模型（Q_PROPERTY 需要完整类型）
#include "TransferTaskModel.h"  // 任务模型（Q_PROPERTY 需要完整类型）

class CrocAdapter;  // 引擎适配层

/**
****************************************************************
* @brief 应用控制器
* @param 无
* @return 无
* @others 只转发真实引擎结果；示例数据仍在 QML 的 DemoData 中，两者不混用
* @author Ray
****************************************************************
*/
class QTimer;  // 队列与任务记录计时器

class AppController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString engineStatus READ GetEngineStatus NOTIFY engineStatusChanged)
    Q_PROPERTY(TransferTaskModel* taskModel READ GetTaskModel CONSTANT)
    Q_PROPERTY(TaskFilterModel* sendTasks READ GetSendTasks CONSTANT)
    Q_PROPERTY(TaskFilterModel* receiveTasks READ GetReceiveTasks CONSTANT)
    Q_PROPERTY(HistoryModel* history READ GetHistory CONSTANT)
    Q_PROPERTY(int taskRevision READ GetTaskRevision NOTIFY taskRevisionChanged)
    Q_PROPERTY(QString shareCode READ GetShareCode NOTIFY shareCodeChanged)
    Q_PROPERTY(bool promptVisible READ GetPromptVisible NOTIFY promptChanged)
    Q_PROPERTY(QString promptTitle READ GetPromptTitle NOTIFY promptChanged)
    Q_PROPERTY(QString promptBody READ GetPromptBody NOTIFY promptChanged)
    Q_PROPERTY(QString promptKind READ GetPromptKind NOTIFY promptChanged)
    Q_PROPERTY(bool promptCanResume READ GetPromptCanResume NOTIFY promptChanged)
    Q_PROPERTY(QString language READ GetLanguage NOTIFY settingsChanged)
    Q_PROPERTY(QString lastEventText READ GetLastEventText NOTIFY lastEventTextChanged)
    Q_PROPERTY(QVariantMap settings READ GetSettings NOTIFY settingsChanged)
    Q_PROPERTY(QString defaultSaveDir READ GetDefaultSaveDir NOTIFY settingsChanged)
    Q_PROPERTY(QString appVersion READ GetAppVersion CONSTANT)
    Q_PROPERTY(QString engineVersion READ GetEngineVersion NOTIFY engineStatusChanged)
    Q_PROPERTY(QString promptId READ GetPromptId NOTIFY promptChanged)
    Q_PROPERTY(bool hasActiveTasks READ HasActiveTasks NOTIFY taskRevisionChanged)
    Q_PROPERTY(bool engineAvailable READ EngineAvailable CONSTANT)
    Q_PROPERTY(bool androidFileBusy READ IsAndroidFileBusy NOTIFY androidFileBusyChanged)

public:
    QString GetPromptKind() const { return m_lstPrompts.isEmpty() ? QString() : m_lstPrompts.first().m_strKind; }
    bool GetPromptCanResume() const { return !m_lstPrompts.isEmpty() && m_lstPrompts.first().m_bCanResume; }
    Q_INVOKABLE void answerPromptDecisionFor(const QString& _strPromptId, int _nDecision);
    /**
    ****************************************************************
    * @brief 构造控制器
    * @param [IN] _pParent 父对象
    * @return 无
    * @others 构造时定位引擎并准备好模型；不自动启动任何传输
    * @author Ray
    ****************************************************************
    */
    explicit AppController(QObject* _pParent = nullptr);

    /**
    ****************************************************************
    * @brief 析构控制器
    * @param 无
    * @return 无
    * @others 适配层会结束仍在运行的引擎进程
    * @author Ray
    ****************************************************************
    */
    ~AppController() override;

    QString GetEngineStatus() const;  // 引擎状态说明
    TransferTaskModel* GetTaskModel() const;  // 全部任务模型
    TaskFilterModel* GetSendTasks() const;  // 发送任务模型
    TaskFilterModel* GetReceiveTasks() const;  // 接收任务模型
    HistoryModel* GetHistory() const;  // 历史记录模型
    int GetTaskRevision() const;  // 任务变更计数
    QString GetShareCode() const;  // 最近生成的口令
    bool GetPromptVisible() const;  // 确认弹窗是否显示
    QString GetPromptTitle() const;  // 确认弹窗标题
    QString GetPromptBody() const;  // 确认弹窗内容
    QString GetLastEventText() const;  // 最近一条事件文本
    QVariantMap GetSettings() const;  // 当前设置值
    QString GetDefaultSaveDir() const;  // 默认接收目录
    QString GetLanguage() const;  // 界面语言 en / zh
    QString GetAppVersion() const;  // 应用版本
    QString GetEngineVersion() const;  // 引擎版本
    QString GetPromptId() const;
    bool HasActiveTasks() const;
    bool EngineAvailable() const;

    /**
    ****************************************************************
    * @brief 启动发送任务
    * @param [IN] _lstPaths 要发送的文件或文件夹绝对路径
    * @return 任务标识；启动失败返回空字符串
    * @others 口令由引擎生成，通过 CodeReady 通知
    * @author Ray
    ****************************************************************
    */
    Q_INVOKABLE QString startSend(const QStringList& _lstPaths, const QString& _strCode = QString());

    /**
    ****************************************************************
    * @brief 启动接收任务
    * @param [IN] _strCode 对方口令
    * @param [IN] _strOutDir 保存目录
    * @return 任务标识；启动失败返回空字符串
    * @others 收到文件清单后会通过 ReceivePrompt 请求确认
    * @author Ray
    ****************************************************************
    */
    Q_INVOKABLE QString startReceive(const QString& _strCode, const QString& _strOutDir);

    /**
    ****************************************************************
    * @brief 取消任务
    * @param [IN] _strTaskId 任务标识
    * @return 无
    * @others 引擎没有取消接口，实际行为是结束进程
    * @author Ray
    ****************************************************************
    */
    Q_INVOKABLE void resumeTask(const QString& _strTaskId, const QString& _strCode = QString());
    Q_INVOKABLE void cancelTask(const QString& _strTaskId);
    Q_INVOKABLE void discardInterruptedTask(const QString& _strTaskId);

    /**
    ****************************************************************
    * @brief 回答引擎的交互提问
    * @param [IN] _bAccept 是否同意
    * @return 无
    * @others 任务由最近一次提示自动记住，界面只需传是否同意
    * @author Ray
    ****************************************************************
    */
    Q_INVOKABLE void answerPrompt(bool _bAccept);
    Q_INVOKABLE void answerPromptFor(const QString& _strPromptId, bool _bAccept);
    Q_INVOKABLE QVariantList describeSelection(const QStringList& _lstUrls) const;
    Q_INVOKABLE QString localPath(const QString& _strUrl) const;
    Q_INVOKABLE void openLocation(const QString& _strPath);
    bool IsAndroidFileBusy() const { return m_bAndroidFileBusy; }
    Q_INVOKABLE void pickAndroidFiles();
    Q_INVOKABLE void importAndroidDocuments(const QStringList& _lstUris);
    Q_INVOKABLE void exportAndroidReceive(const QString& _strPath, const QString& _strTreeUri);

    /**
    ****************************************************************
    * @brief 关闭确认弹窗（等同于拒绝）
    * @param 无
    * @return 无
    * @others 无
    * @author Ray
    ****************************************************************
    */
    Q_INVOKABLE void dismissPrompt();

    /**
    ****************************************************************
    * @brief 复制文本到剪贴板
    * @param [IN] _strText 文本
    * @return 无
    * @others 只复制口令本身，不写入历史
    * @author Ray
    ****************************************************************
    */
    Q_INVOKABLE void copyToClipboard(const QString& _strText);

    Q_INVOKABLE bool saveSettings(const QVariantMap& _hashValues);

    /**
    ****************************************************************
    * @brief 恢复出厂设置
    * @param 无
    * @return 无
    * @others 把设置重置为默认值并立即保存，只影响之后新建的任务
    * @author Ray
    ****************************************************************
    */
    Q_INVOKABLE void resetSettings();

    /**
    ****************************************************************
    * @brief 读取任务详情
    * @param [IN] _strTaskId 任务标识
    * @return 字段表；任务不存在时返回空表
    * @others 供界面详情卡使用，不包含任何演示数据
    * @author Ray
    ****************************************************************
    */
    Q_INVOKABLE QVariantMap getTask(const QString& _strTaskId) const;

signals:
    void androidFileBusyChanged();
    void androidDocumentsImported(const QStringList& _lstPaths);
    void engineStatusChanged();  // 引擎状态变化
    void taskRevisionChanged();  // 任务变更
    void codeReady(const QString& _strTaskId, const QString& _strCode);  // 口令就绪
    void receivePrompt(const QString& _strTaskId, const QString& _strName,
                       const QString& _strSizeText, const QString& _strPeerId);  // 接收确认
    void resumePrompt(const QString& _strTaskId, const QString& _strName, double _dPercent);  // 续传确认
    void taskFinished(const QString& _strTaskId, int _nExitCode, bool _bCancelled, const QString& _strError);  // 任务结束
    void notice(const QString& _strText);  // 界面提示
    void shareCodeChanged();  // 口令变化
    void promptChanged();  // 确认弹窗变化
    void lastEventTextChanged();  // 事件文本变化
    void settingsChanged();  // 设置变化

private:
    struct SManagedTask
    {
        SCrocConfig m_stConfig;
        QString m_strCode;                // 仅内存，绝不写任务记录
        QString m_strGatePromptId;
        QVariantMap m_hashPartialFiles;   // 接收任务的新建未完成文件与身份凭据
        int m_nGateDecision = 0;
        int m_nAttempt = 0;
        bool m_bEngineRunning = false;
        bool m_bTransferSlot = false;
        bool m_bUserCancelled = false;
        bool m_bRejected = false;
        bool m_bHoldForUser = false;
        bool m_bInternalReconnect = false;
        bool m_bExpired = false;
        bool m_bNeedsRelayPassword = false;
        bool m_bRetryEligible = false;
        bool m_bEverTransferred = false;
        qint64 m_llRetryDeadline = 0;      // 当前进程单调时钟，重启不自动沿用
        qint64 m_llNextAttempt = 0;
        qint64 m_llLastActivity = 0;
        qint64 m_llLastBytes = 0;
    };
    void InitializeTaskQueue();
    void PersistTaskJournal();
    void LoadTaskJournal();
    void ScheduleTasks();
    void TryGrantTransfers();
    bool CanGrantTransfer(const QString& _strTaskId) const;
    bool IsRecoverableFailure(const STaskSnapshot& _stTask) const;
    bool HandleManagedFinish(const QString& _strTaskId, int _nExitCode, bool _bCancelled, const QString& _strError);
    bool AnswerManagedPrompt(const QString& _strTaskId, const QString& _strPromptId, int _nDecision);
    void ApplyManagedSnapshot(const STaskSnapshot& _stSnapshot);
    void CompleteTask(const QString& _strTaskId, int _nExitCode, bool _bCancelled, const QString& _strError);
    QString EnqueueTask(STaskSnapshot _stTask, const QString& _strCode);
    void QueueRecordWrite();
    bool m_bDllBackend = false;
    bool m_bAndroidFileBusy = false;
    bool m_bQueueClosing = false;
    bool m_bScheduling = false;
    bool m_bJournalWritable = false;
    QHash<QString, SManagedTask> m_hashManagedTasks;
    QStringList m_lstManagedOrder;
    QTimer* m_pQueueTimer = nullptr;
    QTimer* m_pJournalTimer = nullptr;
    QLockFile* m_pJournalLock = nullptr;
    QString m_strTaskJournalPath;
    QElapsedTimer m_timerQueueClock;
    qint64 m_llQueueUiTick = 0;
    void PrepareEngine();  // 定位引擎并建立适配层
    void ApplyEnvOverrides();  // 联调用的环境变量覆盖
    QString ResolveCrocPath() const;  // 解析 croc.exe 路径
    void QueuePrompt(const QString& _strTaskId, const QString& _strTitle, const QString& _strBody);
    void ShowNextPrompt();
    void RemovePrompts(const QString& _strTaskId);
    void ShowError(const QString& _strText);
    QString ValidateStart() const;

    QString m_strEngineStatus;  // 引擎状态文本
    QString m_strEngineVersion;  // 引擎版本
    QString m_strCrocPath;  // 引擎路径
    SCrocConfig m_stConfig;  // 引擎配置
    CrocAdapter* m_pAdapter = nullptr;  // 适配层
    TransferTaskModel* m_pTaskModel = nullptr;  // 任务模型
    TaskFilterModel* m_pSendTasks = nullptr;  // 发送任务
    TaskFilterModel* m_pReceiveTasks = nullptr;  // 接收任务
    HistoryModel* m_pHistoryModel = nullptr;  // 历史记录
    ConfigStore* m_pConfigStore = nullptr;  // 配置存储
    int m_nTaskRevision = 0;  // 任务变更计数
    int m_nTaskSeed = 1;  // 任务编号种子
    QString m_strShareCode;  // 最近口令
    bool m_bPromptVisible = false;  // 确认弹窗状态
    QString m_strPromptTitle;  // 弹窗标题
    QString m_strPromptBody;  // 弹窗内容
    QString m_strPromptTaskId;  // 弹窗对应任务
    QString m_strLastEventText;  // 最近事件文本
    struct SPendingPrompt
    {
        QString m_strEnginePromptId;
        QString m_strKind;
        bool m_bCanResume = false;
        QString strId;
        QString strTaskId;
        QString strTitle;
        QString strBody;
    };
    QList<SPendingPrompt> m_lstPrompts;
    QString m_strPromptId;
    int m_nClipboardVersion = 0;
    QSet<QString> m_setFinishedTasks;
    bool m_bConfigLoaded = true;
};

#endif  // 结束头文件包含标记
