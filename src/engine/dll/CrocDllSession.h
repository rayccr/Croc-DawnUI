/**
****************************************************************
* @file    CrocDllSession.h
* @brief   DLL 后端的任务会话：创建任务、轮询快照、取消与收尾
* @author  Ray
* @date    2026-11-02
* @note    V1.0.0       Ray       2026-11-02    1. 迁移第 3 轮：适配层 DLL 后端
****************************************************************
*/
#ifndef FLOWDROP_CROC_DLL_SESSION_H
#define FLOWDROP_CROC_DLL_SESSION_H

#include <QFuture>
#include <QHash>  // 任务表
#include <QObject>  // Qt 对象
#include <QStringList>  // 发送路径

#include "CrocTypes.h"  // 任务快照

class CrocLibrary;
class QTimer;
class CrocEventPump;
class QFileSystemWatcher;

/**
****************************************************************
* @brief DLL 后端会话
* @param 无
* @return 无
* @others 只在主线程使用；任务状态来自引擎快照，不做终端文本解析
* @author Ray
****************************************************************
*/
class CrocDllSession : public QObject
{
    Q_OBJECT

public:
    explicit CrocDllSession(CrocLibrary* _pLibrary, QObject* _pParent = nullptr);
    ~CrocDllSession() override;

    bool IsAvailable() const;  // 库是否可用
    QString GetLastError() const;  // 最近错误

    bool StartSend(const QString& _strTaskId, const QStringList& _lstPaths, const QString& _strCode,
                   const SCrocConfig& _stConfig, const QVariantMap& _hashConfig = {});
    bool StartReceive(const QString& _strTaskId, const QString& _strCode, const QString& _strOutDir,
                      const SCrocConfig& _stConfig, const QVariantMap& _hashConfig = {});
    void Cancel(const QString& _strTaskId);
    QList<STaskSnapshot> StopAndSnapshotReceives();
    bool AnswerPrompt(const QString& _strTaskId, const QString& _strPromptId, int _nDecision);
    bool IsRunning(const QString& _strTaskId) const;

signals:
    void promptRequested(const QString& _strTaskId, const QString& _strPromptId, const QString& _strKind, const QVariantMap& _hashDetails);
    void taskUpdated(const STaskSnapshot& _stSnapshot);
    void taskFinished(const QString& _strTaskId, int _nExitCode, bool _bCancelled, const QString& _strError);

private:
    struct SDllTask
    {
        quint64 nEngineId = 0;  // 引擎任务句柄
        STaskSnapshot stSnapshot;  // 最近快照
        QFuture<bool> m_futureSourceCheck;  // 工作线程只读不可变源清单
        qint64 m_llLastSourceCheck = 0;
        QString m_strSafetyError;
        bool bCancelRequested = false;  // 是否已请求取消
    };

    void HandleEvent(const QByteArray& _arrEvent);
    QFileSystemWatcher* m_pSourceWatcher = nullptr;
    CrocEventPump* m_pEventPump = nullptr;
    QHash<QString, QString> m_hashPromptTasks;
    QHash<QString, QString> m_hashPromptKinds;
    void PollAll();  // 轮询所有任务快照
    void FinishTask(const QString& _strTaskId, bool _bCancelled, const QString& _strError);  // 收尾并释放
    static QString MakeCode(const QString& _strSeed);  // 生成口令（库不生成）

    CrocLibrary* m_pLibrary = nullptr;  // 引擎库
    QTimer* m_pTimerPoll = nullptr;  // 快照轮询
    QHash<QString, SDllTask> m_hashTasks;  // 任务表
    QString m_strLastError;  // 最近错误
};

#endif  // 结束头文件包含标记
