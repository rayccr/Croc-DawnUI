/**
****************************************************************
* @file    ConfigStore.h
* @brief   配置存储：读写用户设置，并转换成引擎调用参数
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
****************************************************************
*/
#ifndef FLOWDROP_CONFIG_STORE_H  // 防止头文件重复包含
#define FLOWDROP_CONFIG_STORE_H  // 定义头文件包含标记

#include <QObject>  // Qt 对象基类
#include <QString>  // Qt 字符串
#include <QVariantMap>  // 设置值表

#include "CrocTypes.h"  // 引擎配置

/**
****************************************************************
* @brief 配置存储
* @param 无
* @return 无
* @others 设置写在用户数据目录的 config.json；可用 FLOWDROP_CONFIG_PATH 覆盖以便自检
* @author Ray
****************************************************************
*/
class ConfigStore : public QObject
{
    Q_OBJECT

public:
    /**
    ****************************************************************
    * @brief 构造配置存储
    * @param [IN] _pParent 父对象
    * @return 无
    * @others 构造后用 Load 读取磁盘
    * @author Ray
    ****************************************************************
    */
    explicit ConfigStore(QObject* _pParent = nullptr);

    /**
    ****************************************************************
    * @brief 读取全部设置值
    * @param 无
    * @return 键值表（键与设置结构一致）
    * @others 返回副本，调用方修改后需通过 SetValues 保存
    * @author Ray
    ****************************************************************
    */
    QVariantMap GetValues() const;
    static QString Validate(const QVariantMap& _hashValues);  // 保存和启动前校验，不接受静默降级

    /**
    ****************************************************************
    * @brief 覆盖设置值
    * @param [IN] _hashValues 新键值表，未知键忽略
    * @return 无
    * @others 不自动落盘，需再调用 Save
    * @author Ray
    ****************************************************************
    */
    void SetValues(const QVariantMap& _hashValues);

    /**
    ****************************************************************
    * @brief 从磁盘读取
    * @param 无
    * @return 读取成功返回 true
    * @others 文件不存在时使用默认值
    * @author Ray
    ****************************************************************
    */
    bool Load();

    /**
    ****************************************************************
    * @brief 写回磁盘
    * @param 无
    * @return 写入成功返回 true
    * @others 无
    * @author Ray
    ****************************************************************
    */
    bool Save() const;

    QString GetFilePath() const;  // 配置文件路径

    /**
    ****************************************************************
    * @brief 转换成引擎调用配置
    * @param [IN] _strCrocPath 引擎路径
    * @param [IN] _strWorkHome 子进程 HOME
    * @return 引擎配置
    * @others 只映射真正能生效的项；托盘、通知等仍只保存在本地
    * @author Ray
    ****************************************************************
    */
    SCrocConfig ToCrocConfig(const QString& _strCrocPath, const QString& _strWorkHome) const;

    /**
    ****************************************************************
    * @brief 默认接收目录
    * @param 无
    * @return 绝对路径
    * @others 设置为空或非绝对路径时回退到"下载/FlowDrop"
    * @author Ray
    ****************************************************************
    */
    QString GetDefaultSaveDir() const;

    /**
    ****************************************************************
    * @brief 同名文件处理策略
    * @param 无
    * @return "询问" / "保留两份" / "添加时间戳"
    * @others 供适配层决定是否使用 --rename
    * @author Ray
    ****************************************************************
    */
    QString GetConflictMode() const;

    /**
    ****************************************************************
    * @brief 界面语言
    * @param 无
    * @return "en" 或 "zh"
    * @others 由设置项 lang 推导，供界面与翻译服务读取
    * @author Ray
    ****************************************************************
    */
    QString GetLanguage() const;

    /**
    ****************************************************************
    * @brief 口令等待上限（秒）
    * @param 无
    * @return 30–900 之间的秒数；无效值按默认 120 处理
    * @others 超时由控制器结束等待中的发送任务
    * @author Ray
    ****************************************************************
    */
    int GetCodeTimeoutSeconds() const;

private:
    static QVariantMap DefaultValues();  // 默认设置值

    QVariantMap m_hashValues;  // 当前设置值
};

#endif  // 结束头文件包含标记
