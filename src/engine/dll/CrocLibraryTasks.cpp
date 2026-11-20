/**
****************************************************************
* @file    CrocLibraryTasks.cpp
* @brief   CrocLibrary 的任务级 C ABI 封装：发送/接收/快照/事件/取消/应答/释放
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 迁移第二步：任务接口
****************************************************************
*/
#include "CrocLibrary.h"

#include <QByteArray>  // UTF-8 缓冲

namespace
{
// 统一的“缓冲区 + 两次调用”辅助：先用空缓冲取所需长度，再分配读取
template <typename TCall>
int32_t CallBufferApi(TCall _fnCall, QByteArray& _arrOut)
{
    int32_t nRequired = 0;
    _fnCall(nullptr, 0, &nRequired);
    if (nRequired <= 0)
    {
        _arrOut.clear();
        return 0;
    }
    _arrOut.resize(nRequired);
    const int32_t nResult = _fnCall(_arrOut.data(), nRequired, &nRequired);
    if (0 != nResult)
    {
        _arrOut.clear();
        return nResult;
    }
    _arrOut.truncate(nRequired);
    const qsizetype nTerminator = _arrOut.indexOf(char(0));
    if (nTerminator >= 0) { _arrOut.truncate(nTerminator); }
    return 0;
}
}

bool CrocLibrary::StartSend(const QByteArray& _arrConfigJson, quint64& _nTaskId, QString& _strError)  // 启动发送任务
{
    _strError.clear();
    if ((nullptr == m_pfnStartSend) || !m_library.isLoaded())
    {
        _strError = QStringLiteral("引擎库未加载");
        return false;
    }
    QByteArray arrJson = _arrConfigJson;
    char arrError[512] = {0};
    uint64_t nTaskId = 0;
    const int32_t nResult = m_pfnStartSend(arrJson.data(), &nTaskId, arrError, static_cast<int32_t>(sizeof(arrError)));
    if (0 != nResult)
    {
        _strError = QString::fromUtf8(arrError);
        if (_strError.isEmpty())
        {
            _strError = QStringLiteral("启动发送失败，返回码 %1").arg(nResult);
        }
        return false;
    }
    _nTaskId = static_cast<quint64>(nTaskId);
    return true;
}

bool CrocLibrary::StartReceive(const QByteArray& _arrConfigJson, quint64& _nTaskId, QString& _strError)  // 启动接收任务
{
    _strError.clear();
    if ((nullptr == m_pfnStartReceive) || !m_library.isLoaded())
    {
        _strError = QStringLiteral("引擎库未加载");
        return false;
    }
    QByteArray arrJson = _arrConfigJson;
    char arrError[512] = {0};
    uint64_t nTaskId = 0;
    const int32_t nResult = m_pfnStartReceive(arrJson.data(), &nTaskId, arrError, static_cast<int32_t>(sizeof(arrError)));
    if (0 != nResult)
    {
        _strError = QString::fromUtf8(arrError);
        if (_strError.isEmpty())
        {
            _strError = QStringLiteral("启动接收失败，返回码 %1").arg(nResult);
        }
        return false;
    }
    _nTaskId = static_cast<quint64>(nTaskId);
    return true;
}

bool CrocLibrary::CancelTask(quint64 _nTaskId)  // 取消任务
{
    return (nullptr != m_pfnCancelTask) && (0 == m_pfnCancelTask(static_cast<uint64_t>(_nTaskId)));
}

bool CrocLibrary::IsTaskFinished(quint64 _nTaskId, bool& _bFinished)  // 查询是否结束
{
    if (nullptr == m_pfnIsTaskFinished)
    {
        return false;
    }
    int32_t nFinished = 0;
    if (0 != m_pfnIsTaskFinished(static_cast<uint64_t>(_nTaskId), &nFinished))
    {
        return false;
    }
    _bFinished = (1 == nFinished);
    return true;
}

QByteArray CrocLibrary::GetTaskSnapshot(quint64 _nTaskId)  // 任务快照 JSON
{
    QByteArray arrOut;
    if (nullptr == m_pfnGetTaskSnapshot)
    {
        return arrOut;
    }
    const int32_t nResult = CallBufferApi([this, _nTaskId](char* _pszBuffer, int32_t _nCapacity, int32_t* _pnRequired)
    {
        return m_pfnGetTaskSnapshot(static_cast<uint64_t>(_nTaskId), _pszBuffer, _nCapacity, _pnRequired);
    }, arrOut);
    if (0 != nResult)
    {
        arrOut.clear();
    }
    return arrOut;
}

QByteArray CrocLibrary::PollEvent()  // 取一条事件 JSON
{
    QByteArray arrOut;
    if (nullptr == m_pfnPollEvent)
    {
        return arrOut;
    }
    // 先用空缓冲探测：0=无事件；-5=缓冲区不足（_nRequired 为所需长度）；1=事件可用
    int32_t nRequired = 0;
    const int32_t nProbe = m_pfnPollEvent(nullptr, 0, &nRequired);
    if (0 == nProbe)
    {
        return arrOut;  // 当前无事件
    }
    if ((nProbe < 0) && (nRequired <= 0))
    {
        return arrOut;  // 其他错误或无长度信息
    }
    if (nRequired <= 0)
    {
        return arrOut;
    }
    arrOut.resize(nRequired);
    if (1 != m_pfnPollEvent(arrOut.data(), nRequired, &nRequired))
    {
        arrOut.clear();
        return arrOut;
    }
    arrOut.truncate(nRequired);
    const qsizetype nTerminator = arrOut.indexOf(char(0));
    if (nTerminator >= 0) { arrOut.truncate(nTerminator); }
    return arrOut;
}

bool CrocLibrary::RespondToPrompt(quint64 _nTaskId, const QByteArray& _arrPromptId, int _nDecision)  // 回答提示
{
    if (nullptr == m_pfnRespondToPrompt)
    {
        return false;
    }
    QByteArray arrPrompt = _arrPromptId;
    return 0 == m_pfnRespondToPrompt(static_cast<uint64_t>(_nTaskId), arrPrompt.data(), _nDecision);
}

bool CrocLibrary::ReleaseTask(quint64 _nTaskId)  // 释放任务
{
    return (nullptr != m_pfnReleaseTask) && (0 == m_pfnReleaseTask(static_cast<uint64_t>(_nTaskId)));
}
