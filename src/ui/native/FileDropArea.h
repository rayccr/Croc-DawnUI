/**
****************************************************************
* @file    FileDropArea.h
* @brief   原生 Qt Quick 本地文件拖放接收区域
* @author  Ray
* @date    2026-11-01
****************************************************************
*/
#ifndef CROCD_FILE_DROP_AREA_H
#define CROCD_FILE_DROP_AREA_H
#include <QQuickItem>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class QMimeData;

class FileDropArea : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool containsDrag READ ContainsDrag NOTIFY containsDragChanged)
public:
    explicit FileDropArea(QQuickItem* _pParent = nullptr);
    bool ContainsDrag() const;
signals:
    void containsDragChanged();
    void filesDropped(const QStringList& _lstPaths);
protected:
    void dragEnterEvent(QDragEnterEvent* _pEvent) override;
    void dragMoveEvent(QDragMoveEvent* _pEvent) override;
    void dragLeaveEvent(QDragLeaveEvent* _pEvent) override;
    void dropEvent(QDropEvent* _pEvent) override;
private:
    static QStringList LocalPaths(const QMimeData* _pMimeData);
    void SetContainsDrag(bool _bContains);
    bool m_bContainsDrag = false;
};
#endif
