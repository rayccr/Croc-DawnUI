/**
****************************************************************
* @file    FileDropArea.cpp
* @brief   本地文件拖放：明确接受复制操作，不移动发送源文件
* @author  Ray
* @date    2026-11-01
****************************************************************
*/
#include "FileDropArea.h"
#include <QDragEnterEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QUrl>

FileDropArea::FileDropArea(QQuickItem* _pParent) : QQuickItem(_pParent)
{
    setFlag(QQuickItem::ItemAcceptsDrops, true);
}

bool FileDropArea::ContainsDrag() const { return m_bContainsDrag; }

QStringList FileDropArea::LocalPaths(const QMimeData* _pMimeData)
{
    QStringList lstPaths;
    if (!_pMimeData || !_pMimeData->hasUrls()) { return lstPaths; }
    for (const QUrl& urlFile : _pMimeData->urls())
    {
        if (!urlFile.isLocalFile() || !QFileInfo::exists(urlFile.toLocalFile())) { return {}; }
        const QString strPath = urlFile.toLocalFile();
        if (!lstPaths.contains(strPath, Qt::CaseInsensitive)) { lstPaths.append(strPath); }
    }
    return lstPaths;
}

void FileDropArea::SetContainsDrag(bool _bContains)
{
    if (m_bContainsDrag != _bContains) { m_bContainsDrag = _bContains; emit containsDragChanged(); }
}

void FileDropArea::dragEnterEvent(QDragEnterEvent* _pEvent)
{
    const bool bAccept = (_pEvent->possibleActions() & Qt::CopyAction) && !LocalPaths(_pEvent->mimeData()).isEmpty();
    SetContainsDrag(bAccept);
    if (bAccept) { _pEvent->setDropAction(Qt::CopyAction); _pEvent->accept(); }
    else { _pEvent->ignore(); }
}

void FileDropArea::dragMoveEvent(QDragMoveEvent* _pEvent)
{
    const bool bAccept = (_pEvent->possibleActions() & Qt::CopyAction) && !LocalPaths(_pEvent->mimeData()).isEmpty();
    SetContainsDrag(bAccept);
    if (bAccept) { _pEvent->setDropAction(Qt::CopyAction); _pEvent->accept(); }
    else { _pEvent->ignore(); }
}

void FileDropArea::dragLeaveEvent(QDragLeaveEvent* _pEvent)
{
    SetContainsDrag(false);
    _pEvent->accept();
}

void FileDropArea::dropEvent(QDropEvent* _pEvent)
{
    const QStringList lstPaths = LocalPaths(_pEvent->mimeData());
    SetContainsDrag(false);
    if (lstPaths.isEmpty() || !(_pEvent->possibleActions() & Qt::CopyAction)) { _pEvent->ignore(); return; }
    _pEvent->setDropAction(Qt::CopyAction);
    _pEvent->accept();
    emit filesDropped(lstPaths);
}
