/**
****************************************************************
* @file    CrocEventPump.cpp
* @brief   引擎事件读取线程实现
* @author  Ray
* @date    2026-11-02
* @note    V1.0.0       Ray       2026-11-02    1. 迁移第 2 轮：事件线程
****************************************************************
*/
#include "CrocEventPump.h"

#include "CrocLibrary.h"  // 引擎库

#include <QElapsedTimer>  // 空闲退避

namespace
{
const int g_nIdleSleepMs = 30;  // 无事件时的轮询间隔
}

CrocEventPump::CrocEventPump(CrocLibrary* _pLibrary, QObject* _pParent)  // 构造
    : QThread(_pParent)
    , m_pLibrary(_pLibrary)
{
}

void CrocEventPump::Stop()  // 停止并等待退出
{
    m_bStopRequested.store(true);
    if (isRunning())
    {
        wait(3000);
    }
}

void CrocEventPump::run()  // 轮询循环：只读引擎、只发信号
{
    while (!m_bStopRequested.load())
    {
        if (nullptr == m_pLibrary)
        {
            msleep(g_nIdleSleepMs);
            continue;
        }
        bool bAny = false;
        for (int nIndex = 0; nIndex < 32; ++nIndex)  // 每轮最多取 32 条，避免饿死其它任务
        {
            const QByteArray arrEvent = m_pLibrary->PollEvent();
            if (arrEvent.isEmpty())
            {
                break;
            }
            bAny = true;
            emit eventReceived(arrEvent);  // 跨线程自动走队列连接
        }
        if (!bAny)
        {
            msleep(g_nIdleSleepMs);
        }
    }
}
