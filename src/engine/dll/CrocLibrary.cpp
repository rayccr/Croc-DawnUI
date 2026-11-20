/**
****************************************************************
* @file    CrocLibrary.cpp
* @brief   Croc Go 引擎动态库封装实现
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建库加载与初始化封装
****************************************************************
*/
#include "CrocLibrary.h"

#include <QCoreApplication>  // 应用目录
#include <QDir>  // 路径处理
#include <QFileInfo>  // 路径检查
#include <QDebug>  // 调试输出

namespace
{
// Windows: croc_engine.dll；Linux/Android: libcroc_engine.so
#ifdef Q_OS_WIN
const char* g_szLibraryName = "croc_engine";
#else
const char* g_szLibraryName = "croc_engine";
#endif
}

CrocLibrary::CrocLibrary() = default;

CrocLibrary::~CrocLibrary()  // 析构时先关闭引擎再卸载库
{
    Unload();
}

CrocLibrary& CrocLibrary::Instance()  // 单例
{
    static CrocLibrary s_instanceLibrary;
    return s_instanceLibrary;
}

QString CrocLibrary::ResolveLibraryPath() const  // 解析动态库路径
{
    // 优先环境变量（联调/测试隔离），其次可执行文件同目录，最后开发目录的 dist
    const QString strFromEnv = qEnvironmentVariable("FLOWDROP_CROC_ENGINE_LIB");
    if (!strFromEnv.isEmpty() && QFileInfo::exists(strFromEnv))
    {
        return strFromEnv;
    }
#ifdef Q_OS_WIN
    const QString strFileName = QStringLiteral("croc_engine.dll");
#else
    const QString strFileName = QStringLiteral("libcroc_engine.so");
#endif
    const QString strAppDir = QCoreApplication::applicationDirPath();
    const QString strDeployed = QDir(strAppDir).filePath(strFileName);
    if (QFileInfo::exists(strDeployed))
    {
        return strDeployed;
    }
#ifdef Q_OS_WIN
    const QString strDevPath = QDir::cleanPath(strAppDir + QStringLiteral("/../../Croc-lib/dist/windows-x64/") + strFileName);
#else
    const QString strDevPath = QDir::cleanPath(strAppDir + QStringLiteral("/../../Croc-lib/dist/linux-x64/") + strFileName);
#endif
    return strDevPath;
}

bool CrocLibrary::Load()  // 加载并校验 ABI
{
    if (m_library.isLoaded())
    {
        return true;
    }
    const QString strPath = ResolveLibraryPath();
    if (!QFileInfo::exists(strPath))
    {
        m_strLastError = QStringLiteral("找不到引擎库：") + strPath;
        return false;
    }
    m_library.setFileName(strPath);
#ifdef Q_OS_WIN
    // Go c-shared 的运行时线程可活到进程退出，不能提前解除 DLL 映射。
    m_library.setLoadHints(m_library.loadHints() | QLibrary::PreventUnloadHint);
#endif
    if (!m_library.load())
    {
        m_strLastError = QStringLiteral("加载引擎库失败：") + m_library.errorString();
        return false;
    }
    ResolveSymbols();
    if ((nullptr == m_pfnGetApiVersion) || (nullptr == m_pfnInitialize) || (nullptr == m_pfnShutdown))
    {
        m_strLastError = QStringLiteral("引擎库缺少必需的 ABI 符号");
        m_library.unload();
        return false;
    }
    return true;
}

void CrocLibrary::ResolveSymbols()  // 解析导出符号
{
    m_pfnGetApiVersion = reinterpret_cast<PFN_GetString>(m_library.resolve("Croc_GetApiVersion"));
    m_pfnGetEngineVersion = reinterpret_cast<PFN_GetString>(m_library.resolve("Croc_GetEngineVersion"));
    m_pfnInitialize = reinterpret_cast<PFN_NoArg>(m_library.resolve("Croc_Initialize"));
    m_pfnShutdown = reinterpret_cast<PFN_NoArg>(m_library.resolve("Croc_Shutdown"));
    m_pfnStartSend = reinterpret_cast<PFN_StartTask>(m_library.resolve("Croc_StartSend"));
    m_pfnStartReceive = reinterpret_cast<PFN_StartTask>(m_library.resolve("Croc_StartReceive"));
    m_pfnCancelTask = reinterpret_cast<PFN_TaskIdOnly>(m_library.resolve("Croc_CancelTask"));
    m_pfnReleaseTask = reinterpret_cast<PFN_TaskIdOnly>(m_library.resolve("Croc_ReleaseTask"));
    m_pfnIsTaskFinished = reinterpret_cast<PFN_IsFinished>(m_library.resolve("Croc_IsTaskFinished"));
    m_pfnGetTaskSnapshot = reinterpret_cast<PFN_Buffer>(m_library.resolve("Croc_GetTaskSnapshot"));
    m_pfnPollEvent = reinterpret_cast<PFN_PollEvent>(m_library.resolve("Croc_PollEvent"));
    m_pfnRespondToPrompt = reinterpret_cast<PFN_Respond>(m_library.resolve("Croc_RespondToPrompt"));
    m_pfnGetTaskCount = reinterpret_cast<PFN_NoArg>(m_library.resolve("Croc_GetTaskCount"));
    m_pfnGetActiveTaskCount = reinterpret_cast<PFN_NoArg>(m_library.resolve("Croc_GetActiveTaskCount"));
}

void CrocLibrary::Unload()  // 关闭引擎并卸载
{
    if (!m_library.isLoaded())
    {
        return;
    }
    if (m_bInitialized && (nullptr != m_pfnShutdown))
    {
        m_pfnShutdown();
        m_bInitialized = false;
    }
    m_library.unload();
    m_pfnGetApiVersion = nullptr;
    m_pfnGetEngineVersion = nullptr;
    m_pfnInitialize = nullptr;
    m_pfnShutdown = nullptr;
    m_pfnStartSend = nullptr;
    m_pfnStartReceive = nullptr;
    m_pfnCancelTask = nullptr;
    m_pfnReleaseTask = nullptr;
    m_pfnIsTaskFinished = nullptr;
    m_pfnGetTaskSnapshot = nullptr;
    m_pfnPollEvent = nullptr;
    m_pfnRespondToPrompt = nullptr;
    m_pfnGetTaskCount = nullptr;
    m_pfnGetActiveTaskCount = nullptr;
}

bool CrocLibrary::IsLoaded() const  // 是否已加载
{
    return m_library.isLoaded();
}

const QString& CrocLibrary::GetLastErrorText() const  // 最近错误
{
    return m_strLastError;
}

QString CrocLibrary::GetApiVersion() const  // ABI 版本
{
    if ((nullptr == m_pfnGetApiVersion) || !m_library.isLoaded())
    {
        return QString();
    }
    const char* pszVersion = m_pfnGetApiVersion();
    return (nullptr == pszVersion) ? QString() : QString::fromUtf8(pszVersion);
}

QString CrocLibrary::GetEngineVersion() const  // Croc 核心版本
{
    if ((nullptr == m_pfnGetEngineVersion) || !m_library.isLoaded())
    {
        return QString();
    }
    const char* pszVersion = m_pfnGetEngineVersion();
    return (nullptr == pszVersion) ? QString() : QString::fromUtf8(pszVersion);
}

bool CrocLibrary::IsApiCompatible() const  // ABI 兼容判断
{
    // 版本字符串形如 "1.0.0"：只比较主版本，次版本必须保持向后兼容
    const QString strVersion = GetApiVersion();
    const QString strMajor = strVersion.section(QLatin1Char('.'), 0, 0);
    bool bOk = false;
    const int nMajor = strMajor.toInt(&bOk);
    return bOk && (FLOWDROP_CROC_API_VERSION == nMajor);
}

int32_t CrocLibrary::Initialize()  // 初始化引擎
{
    if (m_bInitialized)
    {
        return 0;  // 幂等：适配层与控制器都可能请求初始化
    }
    if ((nullptr == m_pfnInitialize) || !m_library.isLoaded())
    {
        m_strLastError = QStringLiteral("引擎库未加载");
        return -2;  // CROC_ERR_NOT_INITIALIZED
    }
    const int32_t nResult = m_pfnInitialize();
    if (0 == nResult)
    {
        m_bInitialized = true;
    }
    else
    {
        m_strLastError = QStringLiteral("引擎初始化失败，返回码 ") + QString::number(nResult);
    }
    return nResult;
}

int32_t CrocLibrary::Shutdown()  // 关闭引擎
{
    if ((nullptr == m_pfnShutdown) || !m_library.isLoaded())
    {
        return 0;
    }
    const int32_t nResult = m_pfnShutdown();
    m_bInitialized = false;
    return nResult;
}

bool CrocLibrary::IsInitialized() const  // 是否已初始化
{
    return m_bInitialized;
}

int32_t CrocLibrary::GetTaskCount() const  // 未释放任务数
{
    return (nullptr != m_pfnGetTaskCount) ? m_pfnGetTaskCount() : -1;
}

int32_t CrocLibrary::GetActiveTaskCount() const  // 运行中任务数
{
    return (nullptr != m_pfnGetActiveTaskCount) ? m_pfnGetActiveTaskCount() : -1;
}
