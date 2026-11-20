/**
****************************************************************
* @file    CrocAdapter.h
* @brief   Croc 引擎适配层：用独立进程调用 croc，并把输出转换成任务状态
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
****************************************************************
*/
#ifndef FLOWDROP_CROC_ADAPTER_H  // 防止头文件重复包含
#define FLOWDROP_CROC_ADAPTER_H  // 定义头文件包含标记

#include <QHash>  // 任务上下文表
#include <QObject>  // Qt 对象基类
#include <QString>  // Qt 字符串
#include <QStringList>  // 参数列表
#include <QStringDecoder>
#include <QSet>

#include "CrocJsonMap.h"  // 后端选择
#include "CrocTypes.h"  // 任务阶段与数据结构

class QProcess;  // 前向声明，避免头文件引入 Qt 进程模块
class CrocDllSession;  // DLL 后端会话

/**
****************************************************************
* @brief Croc 引擎适配层
* @param 无
* @return 无
* @others 每个任务对应一个 croc 子进程；同时读取 stdout 与 stderr，
*         stderr 用于状态与进度，stdout 用于错误收集。
* @author Ray
****************************************************************
*/
class CrocAdapter : public QObject
{
    Q_OBJECT

public:
    /**
    ****************************************************************
    * @brief 构造适配层
    * @param [IN] _stConfig 引擎调用配置
    * @param [IN] _pParent 父对象
    * @return 无
    * @others 构造后不会自动启动任何进程
    * @author Ray
    ****************************************************************
    */
    explicit CrocAdapter(const SCrocConfig& _stConfig, QObject* _pParent = nullptr);

    /**
    ****************************************************************
    * @brief 析构适配层并结束所有子进程
    * @param 无
    * @return 无
    * @others 无
    * @author Ray
    ****************************************************************
    */
    ~CrocAdapter() override;

    /**
    ****************************************************************
    * @brief 更新引擎配置
    * @param [IN] _stConfig 新配置
    * @return 无
    * @others 只影响之后启动的任务，已在运行的进程不受影响
    * @author Ray
    ****************************************************************
    */
    void SetConfig(const SCrocConfig& _stConfig);

    /**
    ****************************************************************
    * @brief 选择传输后端
    * @param [IN] _enBackend process=旧 QProcess；dll=C ABI 动态库
    * @return 无
    * @others 默认 process；切换只影响之后新建的任务
    * @author Ray
    ****************************************************************
    */
    void SetBackend(ECrocBackend _enBackend);
    QString GetLastStartError() const;
    void Shutdown();
    QList<STaskSnapshot> StopAndSnapshotReceives();  // 先断开信号再收尾，防止析构重入
    void FlushTail(const QString& _strTaskId);  // 引擎退出时处理无换行尾部
    void SetNextConfigSnapshot(const QVariantMap& _hashConfig);

    /**
    ****************************************************************
    * @brief 启动发送任务
    * @param [IN] _strTaskId 任务标识
    * @param [IN] _lstPaths 要发送的文件或文件夹绝对路径
    * @param [IN] _strCode 自定义口令；为空表示由引擎生成
    * @return 进程启动成功返回 true
    * @others 不会拼 shell 命令，参数以列表方式传入
    * @author Ray
    ****************************************************************
    */
    bool StartSend(const QString& _strTaskId, const QStringList& _lstPaths, const QString& _strCode = QString());

    /**
    ****************************************************************
    * @brief 启动接收任务
    * @param [IN] _strTaskId 任务标识
    * @param [IN] _strCode 对方给出的口令
    * @param [IN] _strOutDir 保存目录，必须是已存在的绝对路径
    * @return 进程启动成功返回 true
    * @others 固定带 --ask，接收前确认由上层决定，不让 stdin 走默认值
    * @author Ray
    ****************************************************************
    */
    bool StartReceive(const QString& _strTaskId, const QString& _strCode, const QString& _strOutDir);

    /**
    ****************************************************************
    * @brief 回答引擎的交互提问
    * @param [IN] _strTaskId 任务标识
    * @param [IN] _bAccept 是否同意（写入 y 或 n）
    * @return 无
    * @others 只对 strError 为空且进程仍运行的任务有效
    * @author Ray
    ****************************************************************
    */
    void AnswerPrompt(const QString& _strTaskId, bool _bAccept);
    bool AnswerDllPrompt(const QString& _strTaskId, const QString& _strPromptId, int _nDecision);

    /**
    ****************************************************************
    * @brief 取消任务
    * @param [IN] _strTaskId 任务标识
    * @return 无
    * @others 引擎没有取消命令，只能结束进程；结果以引擎退出状态为准
    * @author Ray
    ****************************************************************
    */
    void Cancel(const QString& _strTaskId);

    /**
    ****************************************************************
    * @brief 查询任务进程是否仍在运行
    * @param [IN] _strTaskId 任务标识
    * @return 运行中返回 true
    * @others 无
    * @author Ray
    ****************************************************************
    */
    bool IsRunning(const QString& _strTaskId) const;

signals:
    void DllPromptRequested(const QString& _strTaskId, const QString& _strPromptId, const QString& _strKind, const QVariantMap& _hashDetails);
    /**
    ****************************************************************
    * @brief 任务状态更新
    * @param [IN] _stSnapshot 任务快照
    * @return 无
    * @others 只在真实观测到变化时发出
    * @author Ray
    ****************************************************************
    */
    void TaskUpdated(const STaskSnapshot& _stSnapshot);

    /**
    ****************************************************************
    * @brief 口令就绪
    * @param [IN] _strTaskId 任务标识
    * @param [IN] _strCode 口令
    * @return 无
    * @others 无论自定义还是引擎生成，都以解析到的文本为准
    * @author Ray
    ****************************************************************
    */
    void CodeReady(const QString& _strTaskId, const QString& _strCode);

    /**
    ****************************************************************
    * @brief 接收端请求确认
    * @param [IN] _strTaskId 任务标识
    * @param [IN] _strName 文件名
    * @param [IN] _strSizeText 大小文本
    * @param [IN] _strPeerId 发送端标识
    * @return 无
    * @others 必须调用 AnswerPrompt，否则 stdin 到 EOF 会按默认值接受
    * @author Ray
    ****************************************************************
    */
    void PromptAccept(const QString& _strTaskId, const QString& _strName, const QString& _strSizeText, const QString& _strPeerId);

    /**
    ****************************************************************
    * @brief 续传 / 覆盖确认
    * @param [IN] _strTaskId 任务标识
    * @param [IN] _strName 文件名
    * @param [IN] _dPercent 引擎报告的可续传百分比
    * @return 无
    * @others 默认答案是拒绝，必须由上层明确决定
    * @author Ray
    ****************************************************************
    */
    void PromptResume(const QString& _strTaskId, const QString& _strName, double _dPercent);

    /**
    ****************************************************************
    * @brief 任务结束
    * @param [IN] _strTaskId 任务标识
    * @param [IN] _nExitCode 引擎退出码
    * @param [IN] _bCancelled 是否由本程序取消
    * @param [IN] _strError 错误说明
    * @return 无
    * @others 只以退出码与取消标记判定结果，不采信进度条 100%
    * @author Ray
    ****************************************************************
    */
    void TaskFinished(const QString& _strTaskId, int _nExitCode, bool _bCancelled, const QString& _strError);

private:
    /**
    ****************************************************************
    * @brief 单个任务的进程上下文
    * @param 无
    * @return 无
    * @others 每个任务独立缓冲，迟到的输出不会污染其他任务
    * @author Ray
    ****************************************************************
    */
    struct SProcessContext
    {
        QProcess* pProcess = nullptr;  // 子进程
        STaskSnapshot stSnapshot;      // 最近一次快照
        QString strErrBuffer;          // stderr 未成行缓冲
        QString strOutBuffer;          // stdout 未成行缓冲
        bool bCancelRequested = false; // 是否由本程序发起取消
        bool bAwaitCodeLine = false;   // 下一行是否为 croc 命令示例
        QString strAcceptSignature;    // 已上报的接收确认特征，避免重复弹窗
        QString strResumeSignature;    // 已上报的续传确认特征
        QStringDecoder decoderErr{QStringDecoder::Utf8};
        QStringDecoder decoderOut{QStringDecoder::Utf8};
        SCrocConfig stConfig;          // 固定本次配置
        bool bRejected = false;
        bool bNoFiles = false;
        bool bSawProgress = false;
        bool bInternalAbort = false;
        bool bCrashed = false;
        QSet<QString> setReceivedNames;
        QString strExpectedSingleFile;  // 接收确认中的完整单文件名，不能由进度显示名替代
        qint64 llStartedMs = 0;         // 任务启动时间，用于“本次任务写入的文件”兜底核对
    };

    bool StartProcess(SProcessContext* _pContext, const QStringList& _lstArguments);  // 启动子进程
    void DrainChannels(SProcessContext* _pContext);  // 读取并分发当前可用输出
    void HandleStdErrLine(SProcessContext* _pContext, const QString& _strLine);  // 处理 stderr 一行
    void HandleStdOutLine(SProcessContext* _pContext, const QString& _strLine);  // 处理 stdout 一行
    void HandleProgress(SProcessContext* _pContext, const QString& _strLine);  // 解析进度行
    void SetStage(SProcessContext* _pContext, ETaskStage _enStage);  // 更新阶段并上报
    void AppendError(SProcessContext* _pContext, const QString& _strError);  // 追加错误说明
    void FinishContext(const QString& _strTaskId, int _nExitCode, bool _bCancelled);  // 收尾并上报
    QStringList BuildCommonArguments(bool _bInteractive) const;  // 组装公共参数

    SCrocConfig m_stConfig;  // 引擎配置快照
    ECrocBackend m_enBackend = ECrocBackend::BACKEND_PROCESS;  // 当前后端
    CrocDllSession* m_pDllSession = nullptr;  // DLL 后端会话
    QHash<QString, SProcessContext*> m_hashContexts;  // 任务上下文表
    QVariantMap m_hashNextConfig;
    QSet<QString> m_setDllCodeReady;
};

#endif  // 结束头文件包含标记
