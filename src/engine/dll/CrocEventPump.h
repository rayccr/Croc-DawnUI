/**
****************************************************************
* @file    CrocEventPump.h
* @brief   引擎事件读取线程：在独立线程轮询 C ABI，事件以队列信号送回主线程
* @author  Ray
* @date    2026-11-02
* @note    V1.0.0       Ray       2026-11-02    1. 迁移第 2 轮：事件线程
****************************************************************
*/
#ifndef FLOWDROP_CROC_EVENT_PUMP_H
#define FLOWDROP_CROC_EVENT_PUMP_H

#include <QByteArray>  // 事件 JSON
#include <QThread>  // 工作线程

#include <atomic>  // 停止标志

class CrocLibrary;

/**
****************************************************************
* @brief 事件读取线程
* @param 无
* @return 无
* @others 只负责轮询与转发；不触碰任何界面对象，接收方在主线程处理
* @author Ray
****************************************************************
*/
class CrocEventPump : public QThread
{
    Q_OBJECT

public:
    /**
    ****************************************************************
    * @brief 构造事件线程
    * @param [IN] _pLibrary 引擎库（生命周期须长于本线程）
    * @param [IN] _pParent 父对象
    * @return 无
    * @others 构造后需 Start
    * @author Ray
    ****************************************************************
    */
    explicit CrocEventPump(CrocLibrary* _pLibrary, QObject* _pParent = nullptr);

    /**
    ****************************************************************
    * @brief 请求停止并等待线程退出
    * @param 无
    * @return 无
    * @others 幂等；析构时自动调用
    * @author Ray
    ****************************************************************
    */
    void Stop();

signals:
    /**
    ****************************************************************
    * @brief 收到一条引擎事件
    * @param [IN] _arrEvent 事件 JSON（UTF-8）
    * @return 无
    * @others 队列连接，槽函数在主线程执行
    * @author Ray
    ****************************************************************
    */
    void eventReceived(const QByteArray& _arrEvent);

protected:
    void run() override;  // 轮询循环

private:
    CrocLibrary* m_pLibrary = nullptr;  // 引擎库
    std::atomic<bool> m_bStopRequested{false};  // 停止请求
};

#endif  // 结束头文件包含标记
