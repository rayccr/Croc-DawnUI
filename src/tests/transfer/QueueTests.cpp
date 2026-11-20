// 队列与恢复的真实DLL验证，全部使用调用方新建的独立目录。
#include "AppController.h"
#include "CrocLibrary.h"
#include "TransferTaskModel.h"
#include <QGuiApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTimer>
#include <functional>
#include <cstdio>

namespace {
int g_nFailures = 0;
void Check(bool _bOk, const char* _pszName) { std::printf("%s %s\n", _bOk ? "PASS" : "FAIL", _pszName); std::fflush(stdout); if (!_bOk) { ++g_nFailures; } }
bool WaitFor(const std::function<bool()>& _fnReady, int _nTimeout = 10000) {
    if (_fnReady()) { return true; } QEventLoop loopWait; QTimer timerPoll; timerPoll.setInterval(20);
    QObject::connect(&timerPoll, &QTimer::timeout, &loopWait, [&] { if (_fnReady()) { loopWait.quit(); } });
    QTimer::singleShot(_nTimeout, &loopWait, &QEventLoop::quit); timerPoll.start(); loopWait.exec(); return _fnReady();
}
void Write(const QString& _strPath, const QByteArray& _arrData) { QFile fileOut(_strPath); Check(fileOut.open(QIODevice::WriteOnly) && fileOut.write(_arrData) == _arrData.size(), "write isolated fixture"); }
QByteArray Read(const QString& _strPath) { QFile fileIn(_strPath); if (!fileIn.open(QIODevice::ReadOnly)) { return {}; } return fileIn.readAll(); }
void Setup(const QString& _strRoot, bool _bLocal, const QString& _strRelay = QString(), int _nWindow = 600) {
    QDir().mkpath(_strRoot);
    qputenv("FLOWDROP_CONFIG_PATH", (_strRoot + "/config.json").toUtf8());
    qputenv("FLOWDROP_HISTORY_PATH", (_strRoot + "/history.jsonl").toUtf8());
    qputenv("FLOWDROP_TASKS_PATH", (_strRoot + "/tasks.json").toUtf8());
    qputenv("FLOWDROP_ENGINE_BACKEND", "dll");
    Write(_strRoot + "/config.json", QJsonDocument(QJsonObject{
        {"policy", _bLocal ? "仅局域网" : "自动连接"}, {"relay", _strRelay}, {"curve", "p256"},
        {"sendLimit", "1"}, {"receiveLimit", "1"}, {"rate", "1"}, {"reconnectWindow", QString::number(_nWindow)}}).toJson());
}
void AcceptPrompts(AppController& _controller) {
    QObject::connect(&_controller, &AppController::promptChanged, &_controller, [&_controller] {
        if (!_controller.GetPromptVisible()) { return; }
        const QString strPrompt = _controller.GetPromptId();
        QTimer::singleShot(0, &_controller, [&_controller, strPrompt] {
            if (_controller.GetPromptId() != strPrompt) { return; }
            if (_controller.GetPromptKind() == "overwrite") { _controller.answerPromptDecisionFor(strPrompt, _controller.GetPromptCanResume() ? 4 : 2); }
            else { _controller.answerPromptFor(strPrompt, true); }
        });
    });
}
bool Terminal(AppController& _controller, const QString& _strId) { return _controller.getTask(_strId).value("terminal").toBool(); }
bool Same(const QString& _strA, const QString& _strB) { return QCryptographicHash::hash(Read(_strA), QCryptographicHash::Sha256) == QCryptographicHash::hash(Read(_strB), QCryptographicHash::Sha256); }
}

int main(int _nArgc, char* _ppArgv[])
{
    QGuiApplication appTest(_nArgc, _ppArgv);
    if (_nArgc < 3) { return 2; }
    QTemporaryDir dirTest(QString::fromLocal8Bit(_ppArgv[1]) + "/queue-XXXXXX"); dirTest.setAutoRemove(false);
    if (!dirTest.isValid()) { return 3; } const QString strRoot = dirTest.path();
    Check(CrocLibrary::Instance().Load(), "real DLL loaded");

    if (_nArgc == 4 && QByteArray(_ppArgv[3]) == "--wait-policy-only")
    {
        Setup(strRoot + "/wait-policy", true);
        AppController controller;
        QVariantMap hashSettings = controller.GetSettings();
        hashSettings.insert("codeTimeout", "30");  // 旧配置不能再取消等待中的发送。
        Check(controller.saveSettings(hashSettings), "legacy settings accepted");
        const QString strWaitingFile = strRoot + "/waiting.bin";
        const QString strConfirmFile = strRoot + "/confirm.bin";
        Write(strWaitingFile, "idle sender");
        Write(strConfirmFile, "receive after 60 seconds");
        const QString strWaiting = controller.startSend({strWaitingFile});
        const QString strCode = controller.getTask(strWaiting).value("code").toString();
        Check(!strCode.isEmpty() && !controller.getTask(strWaiting).value("shareReady").toBool(),
              "code available immediately while sharing is gated");
        const QString strSending = controller.startSend({strConfirmFile});
        Check(WaitFor([&] { return controller.getTask(strSending).value("shareReady").toBool(); }), "second sender ready");
        const QString strTarget = strRoot + "/wait-policy/received";
        const QString strReceiving = controller.startReceive(controller.getTask(strSending).value("code").toString(), strTarget);
        Check(WaitFor([&] { return controller.GetPromptVisible(); }), "receive prompt visible");
        const QString strPrompt = controller.GetPromptId();
        WaitFor([] { return false; }, 64000);
        Check(controller.GetPromptVisible() && controller.GetPromptId() == strPrompt, "receive prompt survives the former 60 second limit");
        Check(controller.getTask(strWaiting).value("shareReady").toBool() && controller.getTask(strWaiting).value("code").toString() == strCode,
              "idle sender keeps the same ready code beyond the former 30 second limit");
        controller.answerPromptFor(strPrompt, true);
        Check(WaitFor([&] { return Terminal(controller, strSending) && Terminal(controller, strReceiving); }) &&
              Same(strConfirmFile, strTarget + "/confirm.bin"), "late acceptance completes transfer with matching SHA256");
        controller.cancelTask(strWaiting);
        Check(WaitFor([&] { return Terminal(controller, strWaiting); }), "waiting sender cancels normally");
        return g_nFailures == 0 ? 0 : 1;
    }

    if (_nArgc == 4 && QByteArray(_ppArgv[3]) == "--discard-policy-only")
    {
        for (int nCase = 0; nCase < 3; ++nCase)
        {
            const QString strCase = strRoot + "/discard-" + QString::number(nCase);
            const QString strTarget = strCase + "/dst";
            const QString strSource = strCase + "/payload.bin";
            const QString strPartial = strTarget + "/payload.bin";
            Setup(strCase, true);
            QDir().mkpath(strTarget);
            Write(strSource, QByteArray(8 * 1024 * 1024, 'P'));
            Write(strTarget + "/unrelated.txt", "user file");
            if (nCase == 2) { Write(strPartial, "original file exists before receive"); }
            QString strSend;
            QString strReceive;
            {
                AppController controller;
                AcceptPrompts(controller);
                strSend = controller.startSend({strSource});
                Check(WaitFor([&] { return controller.getTask(strSend).value("shareReady").toBool(); }), "sender ready for cleanup fixture");
                strReceive = controller.startReceive(controller.getTask(strSend).value("code").toString(), strTarget);
                Check(WaitFor([&] {
                    const double dPercent = controller.getTask(strReceive).value("transferPercent").toDouble();
                    return dPercent > 10 && dPercent < 70;
                }), "real receive interrupted at partial progress");
            }
            Check(QFileInfo::exists(strPartial), "partial is on disk before explicit removal");
            const bool bHasProof = Read(strCase + "/tasks.json").contains("partial_files");
            Check(bHasProof == (nCase != 2), "only newly created files have cleanup proof");
            if (nCase == 1)
            {
                Check(QFile::rename(strPartial, strPartial + ".original"), "move synthetic partial out of the way");
                Write(strPartial, "replacement owned by user");
            }
            {
                AppController controller;
                Check(controller.getTask(strReceive).value("canRecover").toBool(), "interrupted record restored");
                controller.discardInterruptedTask(strReceive);
                Check(Terminal(controller, strReceive), "interrupted receive record removed");
                controller.discardInterruptedTask(strSend);
                Check(QFileInfo::exists(strSource), "send cleanup preserves original file");
            }
            Check(QFileInfo::exists(strPartial) == (nCase != 0), "clean owned partial and preserve replaced or pre-existing paths");
            Check(Read(strTarget + "/unrelated.txt") == "user file", "unrelated file unchanged");
            Check(QJsonDocument::fromJson(Read(strCase + "/tasks.json")).object().value("tasks").toArray().isEmpty(),
                  "record stays removed after restart");
        }
        return g_nFailures == 0 ? 0 : 1;
    }

    Setup(strRoot + "/dispatch", true);
    const QString strA = strRoot + "/a.bin", strB = strRoot + "/b.bin", strC = strRoot + "/c.bin";
    Write(strA, QByteArray(2 * 1024 * 1024, 'A')); Write(strB, QByteArray(2 * 1024 * 1024, 'B')); Write(strC, QByteArray(2 * 1024 * 1024, 'C'));
    {
        AppController controller; AcceptPrompts(controller);
        const QString strWait = controller.startSend({strA});
        const QString strSendB = controller.startSend({strB});
        const QString strSendC = controller.startSend({strC});
        Check(WaitFor([&] { return controller.getTask(strWait).value("shareReady").toBool() && controller.getTask(strSendB).value("shareReady").toBool() && controller.getTask(strSendC).value("shareReady").toBool(); }), "waiting sender does not block other ready senders at limit one");
        const QString strTarget = strRoot + "/dispatch/dst";
        const QString strRecvB = controller.startReceive(controller.getTask(strSendB).value("code").toString(), strTarget);
        const QString strRecvC = controller.startReceive(controller.getTask(strSendC).value("code").toString(), strTarget + "/sub");
        Check(!strRecvB.isEmpty() && !strRecvC.isEmpty() && controller.getTask(strRecvC).value("attempt").toInt() == 0, "overlapping directory second receive is queued");
        int nMaxSending = 0, nMaxReceiving = 0;
        QObject::connect(&controller, &AppController::taskRevisionChanged, &controller, [&] {
            int nSend = 0, nReceive = 0;
            for (int nRow = 0; nRow < controller.GetTaskModel()->rowCount(); ++nRow) {
                const QString strId = controller.GetTaskModel()->data(controller.GetTaskModel()->index(nRow, 0), TransferTaskModel::ROLE_TASK_ID).toString();
                const STaskSnapshot stTask = controller.GetTaskModel()->Snapshot(strId);
                if (stTask.enStage == ETaskStage::STAGE_TRANSFERRING) { stTask.enDirection == ETaskDirection::DIRECTION_SEND ? ++nSend : ++nReceive; }
            }
            nMaxSending = qMax(nMaxSending, nSend); nMaxReceiving = qMax(nMaxReceiving, nReceive);
        });
        Check(WaitFor([&] { return Terminal(controller, strRecvB) && Terminal(controller, strRecvC) && Terminal(controller, strSendB) && Terminal(controller, strSendC); }, 20000), "independent directions and same-directory FIFO complete");
        Check(nMaxSending <= 1 && nMaxReceiving <= 1 && Same(strB, strTarget + "/b.bin") && Same(strC, strTarget + "/sub/c.bin"), "concurrency limits and received hashes");
        Check(!Terminal(controller, strWait), "offline peer task remains independent");
        controller.cancelTask(strWait); Check(WaitFor([&] { return Terminal(controller, strWait); }), "cancel waiting task after engine acknowledgement");
    }

    Setup(strRoot + "/gates", true);
    {
        AppController controller; AcceptPrompts(controller);
        QVariantMap hashLimits = controller.GetSettings(); hashLimits.insert("receiveLimit", "2"); controller.saveSettings(hashLimits);
        const QString strSendB = controller.startSend({strB}), strSendC = controller.startSend({strC});
        Check(WaitFor([&] { return controller.getTask(strSendB).value("shareReady").toBool() && controller.getTask(strSendC).value("shareReady").toBool(); }), "parallel candidates ready");
        const QString strRecvB = controller.startReceive(controller.getTask(strSendB).value("code").toString(), strRoot + "/gates/b");
        const QString strRecvC = controller.startReceive(controller.getTask(strSendC).value("code").toString(), strRoot + "/gates/c");
        int nMaxActive = 0; bool bSawGate = false;
        QObject::connect(&controller, &AppController::taskRevisionChanged, &controller, [&] {
            int nActive = 0;
            for (const QString& strId : {strSendB, strSendC}) {
                const STaskSnapshot stTask = controller.GetTaskModel()->Snapshot(strId);
                if (stTask.enStage == ETaskStage::STAGE_TRANSFERRING) { ++nActive; }
                bSawGate |= stTask.enStage == ETaskStage::STAGE_QUEUED;
            }
            nMaxActive = qMax(nMaxActive, nActive);
        });
        Check(WaitFor([&] { return Terminal(controller, strSendB) && Terminal(controller, strSendC) && Terminal(controller, strRecvB) && Terminal(controller, strRecvC); }, 15000)
            && nMaxActive == 1 && bSawGate, "send admission gate enforces one while two receive slots available");
    }

    Setup(strRoot + "/queued-cancel", true);
    {
        AppController controller;
        QStringList lstIds;
        for (int nIndex = 0; nIndex < 6; ++nIndex) {
            const QString strFile = strRoot + "/queued-cancel/item-" + QString::number(nIndex) + ".bin";
            Write(strFile, "queued fixture"); lstIds.append(controller.startSend({strFile}));
        }
        Check(!lstIds.last().isEmpty() && controller.getTask(lstIds.last()).value("attempt").toInt() == 0, "bounded waiting sessions leave excess task queued");
        controller.cancelTask(lstIds.last());
        Check(Terminal(controller, lstIds.last()) && !Terminal(controller, lstIds.first()), "cancel queued task does not cancel active tasks");
        for (const QString& strId : lstIds) { controller.cancelTask(strId); }
        Check(WaitFor([&] { return CrocLibrary::Instance().GetTaskCount() == 0; }), "queued-cancel fixture releases sessions");
    }

    Setup(strRoot + "/recover", true);
    const QString strLarge = strRoot + "/resume.bin"; Write(strLarge, QByteArray(8 * 1024 * 1024, 'R'));
    QString strSend, strRecv, strOldCode; const QString strRecvDir = strRoot + "/recover/dst";
    {
        AppController controller; AcceptPrompts(controller);
        QVariantMap hashSecrets = controller.GetSettings(); hashSecrets.insert("password", "relay-secret-queue");
        Check(controller.saveSettings(hashSecrets), "session relay password set");
        strSend = controller.startSend({strLarge});
        Check(WaitFor([&] { return controller.getTask(strSend).value("shareReady").toBool(); }), "recovery sender ready");
        strOldCode = controller.getTask(strSend).value("code").toString(); strRecv = controller.startReceive(strOldCode, strRecvDir);
        Check(WaitFor([&] { const double dPercent = controller.getTask(strRecv).value("transferPercent").toDouble(); return dPercent > 12 && dPercent < 80; }), "interrupt at real partial progress");
    }
    const QByteArray arrJournal = Read(strRoot + "/recover/tasks.json");
    Check(!arrJournal.contains(strOldCode.toUtf8()) && !arrJournal.contains("relay-secret-queue") && !arrJournal.contains("\"password\"")
        && !Read(strRoot + "/recover/config.json").contains("relay-secret-queue"), "journal and settings contain no full code or relay password");
    {
        AppController controller; AcceptPrompts(controller);
        Check(controller.getTask(strSend).value("canRecover").toBool() && controller.getTask(strRecv).value("canRecover").toBool()
            && controller.getTask(strSend).value("code").toString().isEmpty() && controller.getTask(strSend).value("attempt").toInt() == 0
            && CrocLibrary::Instance().GetTaskCount() == 0, "restart restores interrupted records without networking");
        controller.resumeTask(strRecv, QString());
        Check(controller.getTask(strRecv).value("canRecover").toBool(), "receive recovery requires current code");
        controller.resumeTask(strSend);
        Check(controller.getTask(strSend).value("canRecover").toBool(), "relay password required again after restart");
        QVariantMap hashSecrets = controller.GetSettings(); hashSecrets.insert("password", "relay-secret-queue"); controller.saveSettings(hashSecrets);
        controller.resumeTask(strSend);
        Check(WaitFor([&] { return controller.getTask(strSend).value("shareReady").toBool(); }), "resume validates source and creates ready session");
        const QString strNewCode = controller.getTask(strSend).value("code").toString();
        Check(!strNewCode.isEmpty() && strNewCode != strOldCode, "restart sending generates fresh code to share");
        controller.resumeTask(strRecv, strNewCode);
        Check(WaitFor([&] { return Terminal(controller, strSend) && Terminal(controller, strRecv); }, 20000), "reconnected logical task IDs complete");
        Check(Same(strLarge, strRecvDir + "/resume.bin"), "resumed target SHA256 matches");
        qint64 llReused = 0;
        for (const QVariant& varFile : controller.getTask(strRecv).value("files").toList()) { llReused += varFile.toMap().value("reused_bytes").toLongLong(); }
        Check(llReused > 0, "engine confirms reusable bytes instead of app guessing");
    }

    Setup(strRoot + "/changed", true);
    QString strChanged;
    { AppController controller; strChanged = controller.startSend({strA}); Check(WaitFor([&] { return controller.getTask(strChanged).value("shareReady").toBool(); }), "changed-source record ready"); }
    Write(strA, "modified after shutdown");
    { AppController controller; controller.resumeTask(strChanged); Check(controller.getTask(strChanged).value("canRecover").toBool() && controller.getTask(strChanged).value("attempt").toInt() == 0, "changed source rejected before recovery"); controller.cancelTask(strChanged); }

    QTcpServer serverPort; serverPort.listen(QHostAddress::LocalHost, 0); const quint16 nPort = serverPort.serverPort(); serverPort.close();
    Setup(strRoot + "/late-relay", false, "127.0.0.1:" + QString::number(nPort), 30);
    {
        AppController controller; AcceptPrompts(controller);
        const QString strTask = controller.startSend({strB});
        Check(WaitFor([&] { return controller.getTask(strTask).value("attempt").toInt() >= 2; }), "failed connection remains recoverable");
        QProcess procRelay; procRelay.start(QString::fromLocal8Bit(_ppArgv[2]), {"relay", "--port", QString::number(nPort)});
        Check(procRelay.waitForStarted() && WaitFor([&] { return controller.getTask(strTask).value("shareReady").toBool(); }, 12000), "network service return reconnects same UUID");
        const QString strTarget = strRoot + "/late-relay/dst";
        const QString strReceive = controller.startReceive(controller.getTask(strTask).value("code").toString(), strTarget);
        Check(!strReceive.isEmpty() && WaitFor([&] { return Terminal(controller, strTask) && Terminal(controller, strReceive); }, 15000)
            && Same(strB, strTarget + "/b.bin"), "reconnected task completes with matching bytes");
        procRelay.kill(); procRelay.waitForFinished();
    }

    Setup(strRoot + "/retry", false, "127.0.0.1:1", 30);
    {
        AppController controller; const QString strTask = controller.startSend({strB});
        Check(WaitFor([&] { return controller.getTask(strTask).value("attempt").toInt() >= 2; }, 10000), "network failure retries same logical task");
        Check(!Terminal(controller, strTask) && controller.GetHistory()->count() == 0, "retry does not write premature failure history");
        Check(WaitFor([&] { return controller.getTask(strTask).value("canRecover").toBool(); }, 35000), "configured 30-second retention ends automatic retries");
        Check(CrocLibrary::Instance().GetTaskCount() == 0 && QFile::exists(strB) && !Read(strRoot + "/retry/tasks.json").isEmpty(), "retention closes engine and retains record and files");
        controller.cancelTask(strTask);
    }
    Setup(strRoot + "/corrupt", true);
    Write(strRoot + "/corrupt/tasks.json", "{bad journal");
    { AppController controller; Check(controller.GetTaskModel()->rowCount() == 0, "corrupt journal does not connect"); }
    Check(Read(strRoot + "/corrupt/tasks.json") == "{bad journal", "corrupt original journal preserved");
    Check(CrocLibrary::Instance().GetTaskCount() == 0, "all engine handles released");
    std::printf("QUEUE_FAILURES %d\n", g_nFailures); return g_nFailures == 0 ? 0 : 1;
}
