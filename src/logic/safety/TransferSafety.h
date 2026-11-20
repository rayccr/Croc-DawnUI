/**
****************************************************************
* @file    TransferSafety.h
* @brief   本地文件检查、诊断脱敏与错误行动提示
* @author  Ray
* @date    2026-11-01
****************************************************************
*/
#ifndef CROCD_TRANSFER_SAFETY_H
#define CROCD_TRANSFER_SAFETY_H
#include <QStringList>
#include <QVariantMap>

namespace TransferSafety
{
    QVariantMap InspectFiles(const QStringList& _lstPaths);  // 返回规范路径、精确清单与错误
    bool FilesUnchanged(const QVariantList& _lstFiles);  // 身份与元信息检查
    QString PartialFileToken(const QString& _strRoot, const QString& _strPath);
    bool RemovePartialFile(const QString& _strRoot, const QString& _strPath, const QString& _strToken);
    QString Redact(const QString& _strText, const QStringList& _lstSecrets = {});
    QString ExplainError(const QString& _strText);
}
#endif
