/**
****************************************************************
* @file    CrocJsonMap.h
* @brief   引擎后端选择与 JSON 快照映射（迁移第 1 轮）
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 快照 JSON 到任务快照的结构化映射
****************************************************************
*/
#ifndef FLOWDROP_CROC_JSON_MAP_H
#define FLOWDROP_CROC_JSON_MAP_H

#include <QByteArray>  // JSON 原文
#include <QString>  // Qt 字符串

#include "CrocTypes.h"  // 任务快照

/**
****************************************************************
* @brief 传输后端选择
* @param 无
* @return 无
* @others 默认保持旧的 QProcess 后端；DLL 后端需显式开启并逐轮验收
* @author Ray
****************************************************************
*/
enum class ECrocBackend
{
    BACKEND_PROCESS = 0,  // 旧后端：QProcess 调用 croc.exe
    BACKEND_DLL           // 新后端：C ABI 动态库
};

/**
****************************************************************
* @brief 把引擎快照 JSON 映射进任务快照
* @param [IN] _arrJson 引擎返回的快照 JSON（UTF-8）
* @param [OUT] _stSnapshot 任务快照；只写入 JSON 中真实存在的字段
* @param [OUT] _strError 映射失败原因
* @return 成功返回 true
* @others 字段缺失时保持原值，不猜测、不填默认百分比
* @author Ray
****************************************************************
*/
bool MapCrocSnapshotJson(const QByteArray& _arrJson, STaskSnapshot& _stSnapshot, QString& _strError);

/**
****************************************************************
* @brief 解析后端名称（dll / process）
* @param [IN] _strName 名称，空串表示使用默认值
* @return 后端枚举
* @others 无法识别时回退 process，并视作默认后端
* @author Ray
****************************************************************
*/
ECrocBackend ResolveCrocBackend(const QString& _strName);

#endif  // 结束头文件包含标记
