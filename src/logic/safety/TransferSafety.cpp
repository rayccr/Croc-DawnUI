/**
****************************************************************
* @file    TransferSafety.cpp
* @brief   文件清单检查与敏感诊断处理
* @author  Ray
* @date    2026-11-01
****************************************************************
*/
#include "TransferSafety.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QRegularExpression>
#include <QSet>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

namespace
{
QString FileIdentity(const QString& _strPath)
{
#ifdef Q_OS_WIN
    HANDLE hFile = CreateFileW(reinterpret_cast<LPCWSTR>(_strPath.utf16()), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    BY_HANDLE_FILE_INFORMATION stInfo = {};
    const bool bOk = hFile != INVALID_HANDLE_VALUE && GetFileInformationByHandle(hFile, &stInfo);
    if (hFile != INVALID_HANDLE_VALUE) { CloseHandle(hFile); }
    if (bOk) { return QStringLiteral("%1:%2:%3").arg(stInfo.dwVolumeSerialNumber).arg(stInfo.nFileIndexHigh).arg(stInfo.nFileIndexLow); }
#endif
    return QFileInfo(_strPath).canonicalFilePath();
}

#ifdef Q_OS_WIN
// 删除与身份复核在同一文件句柄上完成，避免路径复核后被替换。
QString CheckPartialFile(const QString& _strRoot, const QString& _strPath,
                         const QString& _strExpectedToken = {})
{
    const QString strRoot = QDir::cleanPath(_strRoot);
    const QString strPath = QDir::cleanPath(_strPath);
    const QString strPrefix = strRoot.endsWith(QLatin1Char('/')) ? strRoot : strRoot + QLatin1Char('/');
    if (!QDir::isAbsolutePath(strRoot) || !strPath.startsWith(strPrefix, Qt::CaseInsensitive)) { return {}; }
    // 跳过备用数据流及 UNC。不能安全识别身份时保留文件。
    if (strPath.size() < 4 || strPath.at(1) != QLatin1Char(':') || strPath.indexOf(QLatin1Char(':'), 2) >= 0) { return {}; }
    const bool bRemove = !_strExpectedToken.isEmpty();
    const QString strNative = QDir::toNativeSeparators(strPath);
    const HANDLE hFile = CreateFileW(reinterpret_cast<LPCWSTR>(strNative.utf16()),
        FILE_READ_ATTRIBUTES | (bRemove ? DELETE : 0),
        bRemove ? FILE_SHARE_READ : FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) { return {}; }
    QString strToken;
    BY_HANDLE_FILE_INFORMATION stInfo = {};
    wchar_t szFinalPath[32768];
    const DWORD dwLength = GetFinalPathNameByHandleW(hFile, szFinalPath, 32768, FILE_NAME_NORMALIZED);
    if (dwLength > 4 && dwLength < 32768 && GetFileInformationByHandle(hFile, &stInfo)
        && !(stInfo.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
        && stInfo.nNumberOfLinks == 1)
    {
        const QString strFinal = QDir::fromNativeSeparators(QString::fromWCharArray(szFinalPath, dwLength));
        // 规范路径与保存路径相同才允许处理，拒绝经联接点重定向的文件。
        if (strFinal == strPath)
        {
            strToken = QStringLiteral("%1:%2:%3:%4:%5/%6:%7:%8:%9")
                .arg(stInfo.dwVolumeSerialNumber).arg(stInfo.nFileIndexHigh).arg(stInfo.nFileIndexLow)
                .arg(stInfo.ftCreationTime.dwHighDateTime).arg(stInfo.ftCreationTime.dwLowDateTime)
                .arg(stInfo.ftLastWriteTime.dwHighDateTime).arg(stInfo.ftLastWriteTime.dwLowDateTime)
                .arg(stInfo.nFileSizeHigh).arg(stInfo.nFileSizeLow);
        }
    }
    if (bRemove)
    {
        FILE_DISPOSITION_INFO stDisposition = {TRUE};
        if (strToken != _strExpectedToken || !SetFileInformationByHandle(hFile, FileDispositionInfo, &stDisposition, sizeof(stDisposition)))
        { strToken.clear(); }
    }
    CloseHandle(hFile);
    return strToken;
}
#endif

bool IsLink(const QFileInfo& _stInfo)
{
    bool bLink = _stInfo.isSymLink();
#ifdef Q_OS_WIN
    const DWORD dwAttributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(_stInfo.absoluteFilePath().utf16()));
    bLink = bLink || (dwAttributes != INVALID_FILE_ATTRIBUTES && (dwAttributes & FILE_ATTRIBUTE_REPARSE_POINT));
#endif
    return bLink;
}
}

QVariantMap TransferSafety::InspectFiles(const QStringList& _lstPaths)
{
    QVariantMap hashResult;
    QVariantList lstFiles;
    QStringList lstCleanPaths;
    QSet<QString> setSeen;
    qint64 llTotal = 0;
    QString strError;
    auto fnAdd = [&](const QFileInfo& _stInfo)
    {
        if (!strError.isEmpty()) { return; }
        if (IsLink(_stInfo)) { strError = QStringLiteral("暂不发送符号链接或联接点：") + _stInfo.fileName(); return; }
        if (_stInfo.isDir()) { return; }
        const QString strPath = _stInfo.canonicalFilePath();
        if (!_stInfo.isFile() || strPath.isEmpty()) { strError = QStringLiteral("文件不存在或类型不支持：") + _stInfo.fileName(); return; }
        if (setSeen.contains(strPath.toCaseFolded())) { return; }
        QFile fileRead(strPath);
        if (!fileRead.open(QIODevice::ReadOnly)) { strError = QStringLiteral("无法读取文件，请检查权限：") + _stInfo.fileName(); return; }
        setSeen.insert(strPath.toCaseFolded());
        QVariantMap hashFile;
        hashFile.insert(QStringLiteral("path"), strPath);
        hashFile.insert(QStringLiteral("name"), _stInfo.fileName());
        hashFile.insert(QStringLiteral("size"), _stInfo.size());
        hashFile.insert(QStringLiteral("modified"), _stInfo.lastModified().toMSecsSinceEpoch());
        hashFile.insert(QStringLiteral("identity"), FileIdentity(strPath));
        lstFiles.append(hashFile);
        llTotal += _stInfo.size();
        if (lstFiles.size() > 100000) { strError = QStringLiteral("单次文件数超过 100000，请分批发送。"); }
    };
    for (const QString& strPath : _lstPaths)
    {
        const QFileInfo stInfo(strPath);
        if (!stInfo.isAbsolute() || !stInfo.exists() || IsLink(stInfo)) { strError = QStringLiteral("请选择存在的本地文件或普通目录，暂不支持链接：") + stInfo.fileName(); break; }
        const QString strCanonical = stInfo.canonicalFilePath();
        for (const QString& strOther : _lstPaths)
        {
            const QFileInfo stOther(strOther);
            if (stOther.isDir() && strCanonical.startsWith(stOther.canonicalFilePath() + QLatin1Char('/'), Qt::CaseInsensitive))
            { strError = QStringLiteral("清单包含重复的父目录与子项，请移除重复项目后发送。"); break; }
        }
        if (!strError.isEmpty()) { break; }
        if (lstCleanPaths.contains(strCanonical, Qt::CaseInsensitive)) { continue; }
        lstCleanPaths.append(strCanonical);
        if (stInfo.isDir())
        {
            QDirIterator itFiles(strCanonical, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (itFiles.hasNext() && strError.isEmpty()) { itFiles.next(); fnAdd(itFiles.fileInfo()); }
        }
        else { fnAdd(stInfo); }
    }
    if (strError.isEmpty() && lstFiles.isEmpty()) { strError = QStringLiteral("没有可发送的普通文件，请检查空目录或选择内容。"); }
    hashResult.insert(QStringLiteral("paths"), lstCleanPaths);
    hashResult.insert(QStringLiteral("files"), lstFiles);
    hashResult.insert(QStringLiteral("bytes"), llTotal);
    hashResult.insert(QStringLiteral("error"), strError);
    return hashResult;
}

bool TransferSafety::FilesUnchanged(const QVariantList& _lstFiles)
{
    for (const QVariant& varFile : _lstFiles)
    {
        const QVariantMap hashFile = varFile.toMap();
        const QString strPath = hashFile.value(QStringLiteral("path")).toString();
        const QFileInfo stInfo(strPath);
        if (!stInfo.isFile() || IsLink(stInfo) || stInfo.size() != hashFile.value(QStringLiteral("size")).toLongLong()
            || stInfo.lastModified().toMSecsSinceEpoch() != hashFile.value(QStringLiteral("modified")).toLongLong()
            || FileIdentity(strPath) != hashFile.value(QStringLiteral("identity")).toString()) { return false; }
    }
    return true;
}

QString TransferSafety::PartialFileToken(const QString& _strRoot, const QString& _strPath)
{
#ifdef Q_OS_WIN
    return CheckPartialFile(_strRoot, _strPath);
#else
    Q_UNUSED(_strRoot); Q_UNUSED(_strPath);
    return {};
#endif
}

bool TransferSafety::RemovePartialFile(const QString& _strRoot, const QString& _strPath, const QString& _strToken)
{
#ifdef Q_OS_WIN
    return !_strToken.isEmpty() && !CheckPartialFile(_strRoot, _strPath, _strToken).isEmpty();
#else
    Q_UNUSED(_strRoot); Q_UNUSED(_strPath); Q_UNUSED(_strToken);
    return false;
#endif
}

QString TransferSafety::Redact(const QString& _strText, const QStringList& _lstSecrets)
{
    QString strSafe = _strText;
    for (const QString& strSecret : _lstSecrets) { if (!strSecret.isEmpty()) { strSafe.replace(strSecret, QStringLiteral("[已脱敏]")); } }
    strSafe.replace(QRegularExpression(QStringLiteral("(?i)(?:--(?:pass|code)|[?&]code=)\\s*[^\\s]+")), QStringLiteral("[敏感参数已脱敏]"));
    strSafe.replace(QRegularExpression(QStringLiteral("(?i)(?:socks5?|https?)://[^\\s/@]+:[^\\s/@]+@")), QStringLiteral("proxy://[凭据已脱敏]@"));
    return strSafe.left(4096);
}

QString TransferSafety::ExplainError(const QString& _strText)
{
    const QString strLower = _strText.toLower();
    if (strLower.contains(QStringLiteral("access is denied")) || strLower.contains(QStringLiteral("permission denied"))) { return QStringLiteral("无法读写文件。请检查权限或更换保存目录；未完成文件仍需检查。"); }
    if (strLower.contains(QStringLiteral("space")) || strLower.contains(QStringLiteral("disk full"))) { return QStringLiteral("空间可能不足，请释放磁盘空间或更换保存目录后重新接收。"); }
    if (strLower.contains(QStringLiteral("could not connect")) || strLower.contains(QStringLiteral("timeout")) || strLower.contains(QStringLiteral("discovery failed"))) { return QStringLiteral("连接未成功。请确认双方在线、口令与中继配置匹配，并检查网络后重试；不能据此断定口令错误。"); }
    if (strLower.contains(QStringLiteral("file specified")) || strLower.contains(QStringLiteral("no such file"))) { return QStringLiteral("本地文件可能已移动或删除，请重新选择文件。"); }
    return QStringLiteral("任务未成功。请检查双方在线状态、本地文件和连接配置后重试；需要时重新分享口令。");
}
