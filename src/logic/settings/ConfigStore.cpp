/**
****************************************************************
* @file    ConfigStore.cpp
* @brief   配置存储实现
* @author  Ray
* @date    2026-11-01
* @note    V1.0.0       Ray       2026-11-01    1. 创建此模块
****************************************************************
*/
#include "ConfigStore.h"  // 包含配置存储头文件

#include <QDir>  // 目录创建
#include <QFile>  // 配置文件
#include <QFileInfo>  // 路径检查
#include <QJsonDocument>  // JSON 解析
#include <QJsonObject>  // JSON 对象
#include <QSaveFile>
#include <QUrl>
#include <cmath>
#include <QStandardPaths>  // 用户数据目录

ConfigStore::ConfigStore(QObject* _pParent)  // 初始化默认值
    : QObject(_pParent)
    , m_hashValues(DefaultValues())
{
}

QVariantMap ConfigStore::DefaultValues()  // 默认设置
{
    QVariantMap hashValues;
    hashValues.insert(QStringLiteral("save"), QString());
    hashValues.insert(QStringLiteral("lang"), QStringLiteral("中文"));
    hashValues.insert(QStringLiteral("codeTimeout"), QStringLiteral("120"));
    hashValues.insert(QStringLiteral("reconnectWindow"), QStringLiteral("600"));
    hashValues.insert(QStringLiteral("notify"), QStringLiteral("开启"));
    hashValues.insert(QStringLiteral("close"), QStringLiteral("最小化到托盘"));
    hashValues.insert(QStringLiteral("conflict"), QStringLiteral("询问"));
    hashValues.insert(QStringLiteral("sendLimit"), QStringLiteral("2"));
    hashValues.insert(QStringLiteral("receiveLimit"), QStringLiteral("2"));
    hashValues.insert(QStringLiteral("rate"), QStringLiteral("0"));
    hashValues.insert(QStringLiteral("zip"), QStringLiteral("默认不打包"));
    hashValues.insert(QStringLiteral("policy"), QStringLiteral("自动连接"));
    hashValues.insert(QStringLiteral("relay"), QString());
    hashValues.insert(QStringLiteral("lanAddress"), QString());
    hashValues.insert(QStringLiteral("password"), QString());
    hashValues.insert(QStringLiteral("proxy"), QStringLiteral("关闭"));
    hashValues.insert(QStringLiteral("proxyAddress"), QStringLiteral("127.0.0.1:1080"));
    hashValues.insert(QStringLiteral("phrase"), QStringLiteral("随机生成"));
    hashValues.insert(QStringLiteral("mask"), QStringLiteral("默认显示"));
    hashValues.insert(QStringLiteral("codeVisibilityVersion"), 1);
    hashValues.insert(QStringLiteral("clipboard"), QStringLiteral("不自动清理"));
    hashValues.insert(QStringLiteral("curve"), QStringLiteral("p256"));
    hashValues.insert(QStringLiteral("retention"), QStringLiteral("手动清理"));
    hashValues.insert(QStringLiteral("export"), QStringLiteral("默认脱敏"));
    return hashValues;
}

QVariantMap ConfigStore::GetValues() const  // 读取设置
{
    return m_hashValues;
}

void ConfigStore::SetValues(const QVariantMap& _hashValues)  // 覆盖设置
{
    for (auto itValue = _hashValues.constBegin(); itValue != _hashValues.constEnd(); ++itValue)
    {
        if (m_hashValues.contains(itValue.key()))  // 只接受已知键
        {
            m_hashValues.insert(itValue.key(), itValue.value());
        }
    }
}

QString ConfigStore::GetFilePath() const  // 配置文件路径
{
    const QString strFromEnv = qEnvironmentVariable("FLOWDROP_CONFIG_PATH");
    if (!strFromEnv.isEmpty())
    {
        return strFromEnv;
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/config.json");
}

bool ConfigStore::Load()  // 读取磁盘
{
    const QString strPath = GetFilePath();
    QFile fileConfig(strPath);
    if (!fileConfig.exists())
    {
        return true;  // 没有配置文件就用默认值
    }
    if (!fileConfig.open(QIODevice::ReadOnly)) { return false; }
    const QJsonDocument docConfig = QJsonDocument::fromJson(fileConfig.readAll());
    fileConfig.close();
    if (!docConfig.isObject())
    {
        return false;
    }
    const QJsonObject objConfig = docConfig.object();
    for (auto itValue = objConfig.constBegin(); itValue != objConfig.constEnd(); ++itValue)
    {
        if (m_hashValues.contains(itValue.key()) && itValue.key() != QStringLiteral("password"))
        {
            m_hashValues.insert(itValue.key(), itValue.value().toVariant());
        }
    }
    // 用户新约定：默认显示口令；旧版本只有强制遮挡，没有真正可选策略。
    const bool bMigrateVisibility = !objConfig.contains(QStringLiteral("codeVisibilityVersion"));
    if (bMigrateVisibility) { m_hashValues.insert(QStringLiteral("mask"), QStringLiteral("默认显示")); }
    // 迁移旧配置：移除既有明文密码，不把它恢复为会话密码。
    return (!objConfig.contains(QStringLiteral("password")) && !bMigrateVisibility) || Save();
}

bool ConfigStore::Save() const  // 写回磁盘
{
    const QString strPath = GetFilePath();
    const QFileInfo stInfo(strPath);
    if (!stInfo.dir().exists())
    {
        QDir().mkpath(stInfo.dir().absolutePath());
    }
    QSaveFile fileConfig(strPath);
    if (!fileConfig.open(QIODevice::WriteOnly))
    {
        return false;
    }
    QJsonObject objConfig;
    for (auto itValue = m_hashValues.constBegin(); itValue != m_hashValues.constEnd(); ++itValue)
    {
        if (itValue.key() != QStringLiteral("password"))  // 中继密码只保留当前会话，不写普通配置
        { objConfig.insert(itValue.key(), QJsonValue::fromVariant(itValue.value())); }
    }
    const QByteArray arrJson = QJsonDocument(objConfig).toJson(QJsonDocument::Indented);
    if (fileConfig.write(arrJson) != arrJson.size()) { fileConfig.cancelWriting(); return false; }
    return fileConfig.commit();
}

SCrocConfig ConfigStore::ToCrocConfig(const QString& _strCrocPath, const QString& _strWorkHome) const  // 转引擎配置
{
    SCrocConfig stConfig;
    stConfig.strCrocPath = _strCrocPath;
    stConfig.strWorkHome = _strWorkHome;

    const QString strRelay = m_hashValues.value(QStringLiteral("relay")).toString();
    stConfig.strRelay = (strRelay == QStringLiteral("引擎默认")) ? QString() : strRelay;
    stConfig.strRelayPass = m_hashValues.value(QStringLiteral("password")).toString();

    const QString strCurve = m_hashValues.value(QStringLiteral("curve")).toString();
    stConfig.strCurve = (strCurve == QStringLiteral("引擎默认")) ? QString() : strCurve;

    // 仅局域网：对应引擎的 --local
    stConfig.bLocalOnly = (m_hashValues.value(QStringLiteral("policy")).toString() == QStringLiteral("仅局域网"));

    stConfig.m_strLanAddress = m_hashValues.value(QStringLiteral("lanAddress")).toString().trimmed();

    // 代理：SOCKS5 用 --socks5，HTTP 用 --connect
    const QString strProxy = m_hashValues.value(QStringLiteral("proxy")).toString();
    const QString strProxyAddress = m_hashValues.value(QStringLiteral("proxyAddress")).toString();
    if (strProxy == QStringLiteral("SOCKS5"))
    {
        stConfig.strSocks5 = strProxyAddress;
    }
    else if (strProxy == QStringLiteral("HTTP"))
    {
        stConfig.strHttpProxy = strProxyAddress;
    }

    // 上传限速：0 或空表示不限速，单位 MB/s
    bool bOk = false;
    const double dRate = m_hashValues.value(QStringLiteral("rate")).toString().toDouble(&bOk);
    stConfig.dUploadLimitMbps = (bOk && (dRate > 0.0)) ? dRate : 0.0;

    // 目录打包：只有选择"主动选择时打包"才加 --zip
    stConfig.bZipFolders = (m_hashValues.value(QStringLiteral("zip")).toString() == QStringLiteral("主动选择时打包"));

    // 同名文件：保留两份时用 --rename，其余交给续传询问
    stConfig.bRenameOnConflict = (m_hashValues.value(QStringLiteral("conflict")).toString() == QStringLiteral("保留两份"));
    return stConfig;
}

QString ConfigStore::GetDefaultSaveDir() const  // 默认接收目录
{
#ifdef Q_OS_ANDROID
    // Croc 引擎需要可随机读写的本地路径，接收后由系统文件选择器授权导出。
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/received");
#else
    const QString strSave = m_hashValues.value(QStringLiteral("save")).toString();
    if (!strSave.isEmpty() && QFileInfo(strSave).isAbsolute())
    {
        return QDir::cleanPath(strSave);
    }
    QString strDownload = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (strDownload.isEmpty())
    {
        strDownload = QDir::homePath();
    }
    return QDir::cleanPath(strDownload + QStringLiteral("/Croc-DawnUI"));
#endif
}

QString ConfigStore::GetConflictMode() const  // 同名文件策略
{
    return m_hashValues.value(QStringLiteral("conflict")).toString();
}

int ConfigStore::GetCodeTimeoutSeconds() const  // 口令等待上限
{
    bool bOk = false;
    const int nSeconds = m_hashValues.value(QStringLiteral("codeTimeout")).toString().toInt(&bOk);
    if (!bOk)
    {
        return 120;
    }
    if (nSeconds < 30) { return 30; }
    if (nSeconds > 900) { return 900; }
    return nSeconds;
}

QString ConfigStore::GetLanguage() const  // 界面语言：en / zh
{
    return (m_hashValues.value(QStringLiteral("lang")).toString() == QStringLiteral("English"))
           ? QStringLiteral("en") : QStringLiteral("zh");
}

/**
****************************************************************
* @brief 校验设置；未支持的限制策略必须明确拒绝
* @param [IN] _hashValues 待保存设置
* @return 空串为通过，否则为可操作的错误说明
****************************************************************
*/
QString ConfigStore::Validate(const QVariantMap& _hashValues)
{
    bool bOk = false;
    for (const QString& strKey : {QStringLiteral("sendLimit"), QStringLiteral("receiveLimit")}) {
        const int nLimit = _hashValues.value(strKey, QStringLiteral("2")).toString().toInt(&bOk);
        if (!bOk || nLimit < 1 || nLimit > 8) { return QStringLiteral("发送和接收并发上限请输入1–8；等待对方不占实际传输名额。"); }
    }
    const int nWindow = _hashValues.value(QStringLiteral("reconnectWindow"), QStringLiteral("600")).toString().toInt(&bOk);
    if (!bOk || (nWindow != 0 && (nWindow < 30 || nWindow > 3600))) { return QStringLiteral("断线等待请输入30–3600秒，或0关闭自动重连。"); }
    const double dRate = _hashValues.value(QStringLiteral("rate"), QStringLiteral("0")).toString().toDouble(&bOk);
    if (!bOk || !std::isfinite(dRate) || dRate < 0 || dRate > 1000000) { return QStringLiteral("上传限速请输入 0 到 1000000 的有效数字，0 表示不限速。"); }
    const QString strSave = _hashValues.value(QStringLiteral("save")).toString().trimmed();
    if (!strSave.isEmpty() && !QFileInfo(strSave).isAbsolute()) { return QStringLiteral("默认保存目录必须是本机绝对路径，请重新选择。"); }
    const QString strPolicy = _hashValues.value(QStringLiteral("policy"), QStringLiteral("自动连接")).toString();
    if (strPolicy != QStringLiteral("自动连接") && strPolicy != QStringLiteral("仅局域网")) { return QStringLiteral("指定路径策略尚未支持，请选择自动连接或仅局域网。不会静默降级。"); }
    if (_hashValues.value(QStringLiteral("proxy"), QStringLiteral("关闭")).toString() != QStringLiteral("关闭")) { return QStringLiteral("强制代理的全部通路尚未验证，此版本暂不开放。请关闭代理；本次不会启动连接。"); }
    const QString strConflict = _hashValues.value(QStringLiteral("conflict"), QStringLiteral("询问")).toString();
    if (strConflict != QStringLiteral("询问") && strConflict != QStringLiteral("保留两份")) { return QStringLiteral("时间戳重命名尚未支持，请选择询问或保留两份。"); }
    const QString strCurve = _hashValues.value(QStringLiteral("curve"), QStringLiteral("p256")).toString();
    if (!QStringList{QStringLiteral("p256"), QStringLiteral("p384"), QStringLiteral("p521"), QStringLiteral("siec"), QStringLiteral("ed25519")}.contains(strCurve)) { return QStringLiteral("不支持此曲线，请选择列表中的选项并确认对端兼容。"); }
    const QString strRelay = _hashValues.value(QStringLiteral("relay")).toString().trimmed();
    if (!strRelay.isEmpty() && strRelay != QStringLiteral("引擎默认"))
    {
        const QUrl urlRelay(QStringLiteral("tcp://") + strRelay);
        if (urlRelay.host().isEmpty() || urlRelay.port() < 1 || urlRelay.port() > 65535 || !urlRelay.userInfo().isEmpty() || !urlRelay.path().isEmpty()) { return QStringLiteral("中继请输入主机:端口，例如 192.168.1.8:9009。"); }
    }
    return QString();
}
