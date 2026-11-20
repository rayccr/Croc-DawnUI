/**
****************************************************************
* @file    SendPageCheck.cpp
* @brief   发送页事件链：原生拖放、任务口令刷新、默认显示、完成后再次发送
* @author  Ray
* @date    2026-11-01
****************************************************************
*/
#include "SendPageCheck.h"
#include "FileDropArea.h"
#include <QCoreApplication>
#include <QQuickWindow>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QVariant>
#include <memory>

void StartSendPageCheck(QQuickWindow* _pWindow, const QString& _strFile, const QString& _strReport)
{
    _pWindow->setProperty("currentPage", QStringLiteral("send"));
    QTimer::singleShot(150, _pWindow, [_pWindow, _strFile, _strReport]()
    {
        QObject* pPage = _pWindow->findChild<QObject*>(QStringLiteral("sendPage"));
        FileDropArea* pDrop = _pWindow->findChild<FileDropArea*>(QStringLiteral("sendDropArea"));
        QObject* pButton = _pWindow->findChild<QObject*>(QStringLiteral("startSendButton"));
        QObject* pCodeField = _pWindow->findChild<QObject*>(QStringLiteral("shareCodeField"));
        if (!pPage || !pDrop || !pButton || !pCodeField) { QCoreApplication::exit(2); return; }
        QMimeData mimeFile;
        mimeFile.setUrls({QUrl::fromLocalFile(_strFile)});
        const QPoint pointDrop = pDrop->mapToScene(QPointF(pDrop->width()/2, pDrop->height()/2)).toPoint();
        QDragEnterEvent eventEnter(pointDrop, Qt::CopyAction | Qt::MoveAction, &mimeFile, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(_pWindow, &eventEnter);
        QDropEvent eventDrop(pointDrop, Qt::CopyAction | Qt::MoveAction, &mimeFile, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(_pWindow, &eventDrop);
        const bool bDropped = eventEnter.isAccepted() && eventDrop.isAccepted() && eventDrop.dropAction() == Qt::CopyAction
            && pPage->property("selectionCount").toInt() == 1;
        QMetaObject::invokeMethod(pPage, "startSelectedTask");

        QTimer* pPoll = new QTimer(_pWindow);
        pPoll->setInterval(20);
        const auto pSawCode = std::make_shared<bool>(false);
        const auto pTicks = std::make_shared<int>(0);
        QObject::connect(pPoll, &QTimer::timeout, _pWindow, [=]()
        {
            ++*pTicks;
            if (pPage->property("shareCode").toString() == QStringLiteral("secret-test-code")
                && pCodeField->property("text").toString() == QStringLiteral("secret-test-code")
                && pCodeField->property("echoMode").toInt() == 0) { *pSawCode = true; }
            const bool bFinished = pButton->property("text").toString() == QStringLiteral("再次发送") && pButton->property("enabled").toBool();
            if (!bFinished && *pTicks < 250) { return; }
            pPoll->stop();
            QJsonObject objReport;
            objReport.insert(QStringLiteral("nativeDropAcceptedAsCopy"), bDropped);
            objReport.insert(QStringLiteral("taskCodeUpdatedAndVisible"), *pSawCode);
            objReport.insert(QStringLiteral("finishedSendCanRestart"), bFinished);
            objReport.insert(QStringLiteral("sourceStillExists"), QFile::exists(_strFile));
            QFile fileReport(_strReport);
            const bool bReportOk = fileReport.open(QIODevice::WriteOnly) && fileReport.write(QJsonDocument(objReport).toJson()) > 0;
            QCoreApplication::exit(bReportOk && bDropped && *pSawCode && bFinished ? 0 : 2);
        });
        pPoll->start();
    });
}
