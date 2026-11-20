/**
****************************************************************
* @file    CrocTypes.h
* @brief   Croc 传输任务的阶段、方向与数据结构
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
****************************************************************
*/
#ifndef FLOWDROP_CROC_TYPES_H  // 防止头文件重复包含
#define FLOWDROP_CROC_TYPES_H  // 定义头文件包含标记

#include <QString>  // Qt 字符串
#include <QVariantList>
#include <QVariantMap>

/**
****************************************************************
* @brief 任务阶段
* @param 无
* @return 无
* @others 阶段来自实测到的引擎输出，不用百分比反推；没有真实依据时停留在上一阶段
* @author Ray
****************************************************************
*/
enum class ETaskStage
{
    STAGE_IDLE = 0,          // 尚未开始
    STAGE_STARTING,          // 进程已启动，尚未与对端建立联系
    STAGE_WAITING_PEER,      // 等待对端（connecting / looking for sender）
    STAGE_CONNECTED,         // 已认证口令，正在打开通道
    STAGE_TRANSFERRING,      // 正在传输（解析到真实进度）
    STAGE_COMPLETED,         // 引擎退出码为 0
    STAGE_FAILED,            // 引擎报错或退出码非 0
    STAGE_CANCELLED,         // 由本程序主动结束
    STAGE_CANCELLING,        // 等待引擎结束，不提前宣布已取消
    STAGE_SENT,              // 本端发送结束，未证明对方已保存
    STAGE_SKIPPED,           // 用户拒绝或没有传输文件
    STAGE_UNCONFIRMED,       // 引擎退出但缺乏足够文件结果证据
    STAGE_VERIFYING,         // 引擎正在校验已传输的内容（Hashing 行）
    STAGE_QUEUED,            // 尚未获得引擎会话或传输名额
    STAGE_RECONNECTING,      // 应用保留任务，正在重新建立会话
    STAGE_INTERRUPTED,       // 重启恢复的记录，等待用户确认
    STAGE_SAVING,            // 校验后正在核对并完成保存
    STAGE_PARTIAL,           // 部分文件跳过或未获完整结果
    STAGE_AWAITING_CONFIRM   // 发送端已发完，等待对方确认
};

/**
****************************************************************
* @brief 任务方向
* @param 无
* @return 无
* @others 无
* @author Ray
****************************************************************
*/
enum class ETaskDirection
{
    DIRECTION_SEND = 0,      // 发送
    DIRECTION_RECEIVE        // 接收
};

/**
****************************************************************
* @brief Croc 引擎调用配置
* @param 无
* @return 无
* @others 与任务无关的公共参数；每个任务在创建时取一份快照
* @author Ray
****************************************************************
*/
struct SCrocConfig
{
    QString strCrocPath;     // croc.exe 的绝对路径
    QString strRelay;        // 中继地址；为空表示使用 croc 默认公网中继
    QString strRelayPass;    // 自建中继的访问密码，不是文件加密口令
    QString strCurve;        // 加密曲线；为空表示使用引擎默认 p256
    QString strWorkHome;     // 子进程的 HOME，避免 croc 去写用户主目录的 .config
    QString m_strLanAddress;   // 仅局域网接收的可选发送端IP:端口
    QString m_strLocalRelayPort; // 由实际库绑定结果填充（配置不使用）
    QString strSocks5;       // SOCKS5 代理地址
    QString strHttpProxy;    // HTTP 代理地址
    double dUploadLimitMbps = 0.0;   // 上传限速，0 表示不限速
    bool bNoCompress = true;         // 是否禁用压缩
    bool bLocalOnly = false;         // 仅局域网（引擎 --local）
    bool bZipFolders = false;        // 目录先打包再发送（引擎 --zip）
    bool bRenameOnConflict = false;  // 同名文件自动改名（引擎 --rename）
};

/**
****************************************************************
* @brief 任务快照
* @param 无
* @return 无
* @others 只承载已经真实观测到的数据；未测量到的字段保持初值，不用示例值填充
* @author Ray
****************************************************************
*/
struct STaskSnapshot
{
    QString strTaskId;                                    // 任务标识
    ETaskDirection enDirection = ETaskDirection::DIRECTION_SEND;  // 任务方向
    ETaskStage enStage = ETaskStage::STAGE_IDLE;          // 当前阶段
    QString strDisplayName;                               // 文件名或清单摘要
    QString strCode;                                      // 分享口令
    qint64 llTotalBytes = 0;                              // 当前文件总字节数，0 表示未知
    qint64 llTaskBytes = 0;                               // 整个任务的总字节数（来自接收确认），0 表示未知
    qint64 llDoneBytes = 0;                               // 已传输字节数
    double dPercent = -1.0;                               // 百分比，负值表示无法计量
    bool m_bAggregateProgress = false;                    // DLL是整批语义，旧进程解析仍保持原语义
    double m_dFilePercent = -1.0;
    double m_dVerifyPercent = -1.0;
    QString m_strVerifyStatus;
    QString strSpeed;                                     // 速度文本，例如 "4.4 MB/s"
    QString strEta;                                       // 剩余时间文本，例如 "2s:2s"
    QString m_strLocalRelayPort;                          // 本任务实际本地中继端口
    QString strPeer;                                      // 对端地址或标识
    int nFileIndex = 0;                                   // 多文件时当前第几个（1 起），0 表示未知
    int nFileCount = 0;                                   // 多文件总数，0 表示未知
    QString m_strErrorCode;                                // DLL结构化错误分类
    QString strError;                                     // 错误说明
    int nExitCode = 0;                                    // 引擎退出码
    bool bTerminal = false;                              // 本次引擎尝试已收尾
    bool bSizeExact = false;                             // 总量是否来自精确清单
    bool bShareReady = false;                            // 当前任务有可分享的口令且进程仍在
    QString strTargetDir;                                // 本任务固定接收目录
    QStringList lstSourcePaths;                          // 发送源路径，不保存到诊断日志
    QVariantList lstFiles;                               // 开始前源文件清单与身份信息
    QVariantList m_lstResultFiles;                        // 引擎清单及实际路径；不能覆盖源身份清单
    int m_nSkippedFiles = 0;                               // 引擎确认跳过的文件数
    int m_nCompletedFiles = 0;                             // 引擎确认完成的文件数
    int m_nFailedFiles = 0;                                // 引擎确认失败的文件数
    QVariantMap hashConfig;                              // 创建时普通配置快照，不含密码/口令
};

#endif  // 结束头文件包含标记
