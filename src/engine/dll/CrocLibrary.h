/**
****************************************************************
* @file    CrocLibrary.h
* @brief   Croc Go 引擎动态库封装：只负责 C ABI 调用与资源生命周期
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建库加载与初始化封装
****************************************************************
*/
#ifndef FLOWDROP_CROC_LIBRARY_H
#define FLOWDROP_CROC_LIBRARY_H

#include <QByteArray>  // JSON 缓冲
#include <QLibrary>  // 动态库加载
#include <QString>  // Qt 字符串

#include <cstdint>  // 定长整型

// 与 croc_engine_api.h 保持一致的 ABI 版本；不一致时拒绝调用
#define FLOWDROP_CROC_API_VERSION 1

/**
****************************************************************
* @brief 引擎库封装
* @param 无
* @return 无
* @others 单例使用；加载失败不抛异常，由 GetLastErrorText 说明原因
* @author Ray
****************************************************************
*/
class CrocLibrary
{
public:
    static CrocLibrary& Instance();  // 全局实例

    ~CrocLibrary();

    bool Load();  // 加载动态库
    void Unload();  // 卸载动态库（会先关闭引擎）
    bool IsLoaded() const;  // 是否已加载

    const QString& GetLastErrorText() const;  // 最近一次失败原因
    QString GetApiVersion() const;  // ABI 版本
    QString GetEngineVersion() const;  // Croc 核心版本
    bool IsApiCompatible() const;  // ABI 主版本是否兼容

    int32_t Initialize();  // 初始化引擎
    int32_t Shutdown();  // 取消全部任务并等待退出
    bool IsInitialized() const;
    int32_t GetTaskCount() const;  // 未释放任务数
    int32_t GetActiveTaskCount() const;  // 运行中任务数

    // 任务级接口（见 CrocLibraryTasks.cpp）
    bool StartSend(const QByteArray& _arrConfigJson, quint64& _nTaskId, QString& _strError);
    bool StartReceive(const QByteArray& _arrConfigJson, quint64& _nTaskId, QString& _strError);
    bool CancelTask(quint64 _nTaskId);
    bool IsTaskFinished(quint64 _nTaskId, bool& _bFinished);
    QByteArray GetTaskSnapshot(quint64 _nTaskId);
    QByteArray PollEvent();
    bool RespondToPrompt(quint64 _nTaskId, const QByteArray& _arrPromptId, int _nDecision);
    bool ReleaseTask(quint64 _nTaskId);

    // 函数指针类型定义（与 croc_engine_api.h 对应）
    using PFN_GetString = char* (*)();
    using PFN_NoArg = int32_t (*)();
    using PFN_IntOut = int32_t (*)(int32_t*);

private:
    CrocLibrary();
    CrocLibrary(const CrocLibrary&) = delete;
    CrocLibrary& operator=(const CrocLibrary&) = delete;

    void ResolveSymbols();  // 解析导出符号
    QString ResolveLibraryPath() const;  // 解析动态库路径

    QLibrary m_library;  // 动态库句柄
    QString m_strLastError;  // 最近错误
    bool m_bInitialized = false;  // 引擎是否已初始化

    using PFN_StartTask = int32_t (*)(char*, uint64_t*, char*, int32_t);
    using PFN_TaskIdOnly = int32_t (*)(uint64_t);
    using PFN_IsFinished = int32_t (*)(uint64_t, int32_t*);
    using PFN_Buffer = int32_t (*)(uint64_t, char*, int32_t, int32_t*);
    using PFN_PollEvent = int32_t (*)(char*, int32_t, int32_t*);
    using PFN_Respond = int32_t (*)(uint64_t, char*, int32_t);

    PFN_StartTask m_pfnStartSend = nullptr;
    PFN_StartTask m_pfnStartReceive = nullptr;
    PFN_TaskIdOnly m_pfnCancelTask = nullptr;
    PFN_TaskIdOnly m_pfnReleaseTask = nullptr;
    PFN_IsFinished m_pfnIsTaskFinished = nullptr;
    PFN_Buffer m_pfnGetTaskSnapshot = nullptr;
    PFN_PollEvent m_pfnPollEvent = nullptr;
    PFN_Respond m_pfnRespondToPrompt = nullptr;

    PFN_GetString m_pfnGetApiVersion = nullptr;
    PFN_GetString m_pfnGetEngineVersion = nullptr;
    PFN_NoArg m_pfnInitialize = nullptr;
    PFN_NoArg m_pfnShutdown = nullptr;
    PFN_NoArg m_pfnGetTaskCount = nullptr;
    PFN_NoArg m_pfnGetActiveTaskCount = nullptr;
};

#endif  // 结束头文件包含标记
