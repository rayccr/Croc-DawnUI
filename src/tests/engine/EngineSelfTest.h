/**
****************************************************************
* @file    EngineSelfTest.h
* @brief   引擎联调自检：用真实进程跑一次发送与接收
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
****************************************************************
*/
#ifndef FLOWDROP_ENGINE_SELF_TEST_H  // 防止头文件重复包含
#define FLOWDROP_ENGINE_SELF_TEST_H  // 定义头文件包含标记

#include <QString>  // Qt 字符串

/**
****************************************************************
* @brief 运行引擎联调自检
* @param [IN] _strCrocPath croc.exe 绝对路径
* @param [IN] _strWorkDir 自检工作目录，测试文件与接收结果都放在这里
* @param [IN] _bLongSingleFile 为 true 时检查长文件名单文件接收，默认保留三文件用例
* @return 收发双方都成功返回 0，否则返回 1
* @others 自检会启动一个本机中继，跑完自动结束；不写入用户目录，也不参与正式界面
* @author Ray
****************************************************************
*/
int RunEngineApiCheck();

int RunEngineLibOnlyCheck();

int RunEngineDllLocalCheck();  // 无 croc.exe 的本地中继收发  // 库模式检查（无 croc.exe）  // 库加载与初始化自检

int RunEngineApiTaskCheck(const QString& _strCrocPath);

int RunEngineDllTransferCheck(const QString& _strCrocPath);  // DLL 到 DLL 真实传输与结构化进度  // 任务级 ABI 自检  // 动态库加载与初始化自检

int RunEngineSelfTest(const QString& _strCrocPath, const QString& _strWorkDir, bool _bLongSingleFile = false, bool _bSendOnlyTimeout = false);

#endif  // 结束头文件包含标记
