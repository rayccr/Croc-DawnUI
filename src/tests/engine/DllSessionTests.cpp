// DLL 接入回归：真实收发、稳定任务ID、就绪口令、失败终态及释放。
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <QCryptographicHash>
#include "CrocDllSession.h"
#include "CrocLibrary.h"

// 覆盖/跳过/拒收必须通过真实prompt决定，不能改为自动接受。
bool RunPromptScenario(CrocLibrary& _libEngine, const SCrocConfig& _stConfig, const QString& _strFile, const QString& _strDir, int _nDecision)
{
    QDir().mkpath(_strDir);
    const QString strTarget = _strDir + "/" + QFileInfo(_strFile).fileName();
    const bool bExisting = _nDecision == 2 || _nDecision == 3;
    if (bExisting) { QFile fileOld(strTarget); if (!fileOld.open(QIODevice::WriteOnly)) { return false; } fileOld.write("keep-this-original"); }
    CrocDllSession sessionCase(&_libEngine);
    QEventLoop loopCase;
    bool bOk = true, bStartedReceive = false, bSawChoice = false, bReceiverFinished = false;
    int nFinishedCase = 0;
    QObject::connect(&sessionCase, &CrocDllSession::taskUpdated, &loopCase, [&](const STaskSnapshot& _stTask) {
        if (_stTask.strTaskId == "case-send" && _stTask.bShareReady && !bStartedReceive) {
            bStartedReceive = true;
            const QString strCode = _stTask.strCode;
            QTimer::singleShot(0, &loopCase, [&, strCode] { bOk &= sessionCase.StartReceive("case-recv", strCode, _strDir, _stConfig); });
        }
    });
    QObject::connect(&sessionCase, &CrocDllSession::promptRequested, &loopCase,
        [&](const QString& _strTask, const QString& _strPrompt, const QString& _strKind, const QVariantMap&) {
        if (_strKind == "receive_accept") {
            bOk &= sessionCase.AnswerPrompt(_strTask, _strPrompt, _nDecision == 0 ? 0 : 1);
            if (_nDecision == 0) { bSawChoice = true; }
        } else if (_strKind == "overwrite") {
            bSawChoice = true;
            QFile fileOld(strTarget); fileOld.open(QIODevice::ReadOnly);
            bOk &= fileOld.readAll() == "keep-this-original";
            fileOld.close();
            bOk &= !sessionCase.AnswerPrompt("case-send", _strPrompt, _nDecision);
            bOk &= sessionCase.AnswerPrompt(_strTask, _strPrompt, _nDecision);
            bOk &= !sessionCase.AnswerPrompt(_strTask, _strPrompt, _nDecision);
        } else { bOk = false; sessionCase.Cancel(_strTask); }
    });
    QObject::connect(&sessionCase, &CrocDllSession::taskFinished, &loopCase,
        [&](const QString& _strTask, int _nExit, bool, const QString&) {
        ++nFinishedCase;
        if (_strTask == "case-recv") {
            bReceiverFinished = true;
            if (_nDecision == 2) { bOk &= _nExit == 0; }
            if (_nDecision != 2) { sessionCase.Cancel("case-send"); }
        }
        if (nFinishedCase == 2) { loopCase.quit(); }
    });
    bOk &= sessionCase.StartSend("case-send", {_strFile}, {}, _stConfig);
    QTimer::singleShot(15000, &loopCase, &QEventLoop::quit);
    loopCase.exec();
    QFile fileResult(strTarget); fileResult.open(QIODevice::ReadOnly);
    if (_nDecision == 3) { bOk &= fileResult.readAll() == "keep-this-original"; }
    if (_nDecision == 0) { bOk &= !QFileInfo::exists(strTarget); }
    if (_nDecision == 2) { QFile fileSource(_strFile); fileSource.open(QIODevice::ReadOnly); bOk &= QCryptographicHash::hash(fileResult.readAll(), QCryptographicHash::Sha256) == QCryptographicHash::hash(fileSource.readAll(), QCryptographicHash::Sha256); }
    bOk &= bSawChoice && bReceiverFinished && nFinishedCase == 2 && _libEngine.GetTaskCount() == 0;
    qInfo("PROMPT_CASE decision=%d pass=%d", _nDecision, bOk);
    return bOk;
}

bool RunSourceChange(CrocLibrary& _libEngine, const SCrocConfig& _stConfig, const QString& _strDir, int _nKind)
{
    QDir().mkpath(_strDir + "/dst");
    const QString strFile = _strDir + "/protected.bin";
    QFile fileFixture(strFile);
    if (!fileFixture.open(QIODevice::WriteOnly)) { return false; }
    fileFixture.write(QByteArray(4 * 1024 * 1024, 'P')); fileFixture.close();
    CrocDllSession sessionTest(&_libEngine);
    SCrocConfig stConfig = _stConfig; stConfig.dUploadLimitMbps = 0.5;
    QEventLoop loopCase;
    bool bReceiveStarted = false, bChanged = false, bDetected = false;
    int nFinished = 0;
    QObject::connect(&sessionTest, &CrocDllSession::promptRequested, &loopCase, [&](const QString& _strTask, const QString& _strPrompt, const QString&, const QVariantMap&) { sessionTest.AnswerPrompt(_strTask, _strPrompt, 1); });
    QObject::connect(&sessionTest, &CrocDllSession::taskUpdated, &loopCase, [&](const STaskSnapshot& _stTask) {
        if (_stTask.strTaskId == "protect-send" && _stTask.bShareReady && !bReceiveStarted) {
            bReceiveStarted = true; const QString strCode = _stTask.strCode;
            if (_nKind == 0) { bChanged = QFile::remove(strFile); }
            QTimer::singleShot(0, &loopCase, [&, strCode] { sessionTest.StartReceive("protect-recv", strCode, _strDir + "/dst", stConfig); });
        }
        if (_nKind != 0 && _stTask.strTaskId == "protect-send" && _stTask.dPercent > 0 && _stTask.dPercent < 100 && !bChanged) {
            bChanged = true;
            if (_nKind == 0) { bChanged = QFile::remove(strFile); }
            else if (_nKind == 1) { QFile fileChange(strFile); fileChange.open(QIODevice::WriteOnly | QIODevice::Append); fileChange.write("changed"); }
            else { QFile::rename(strFile, strFile + ".original"); QFile fileReplace(strFile); fileReplace.open(QIODevice::WriteOnly); fileReplace.write(QByteArray(4 * 1024 * 1024, 'R')); }
        }
    });
    QObject::connect(&sessionTest, &CrocDllSession::taskFinished, &loopCase, [&](const QString& _strTask, int _nExit, bool _bCancel, const QString& _strError) {
        if (_strTask == "protect-send") {
            bDetected = _nExit != 0 && !_bCancel && !_strError.isEmpty();
            qInfo("SOURCE_RESULT kind=%d exit=%d cancel=%d changed=%d error=%s", _nKind, _nExit, _bCancel, bChanged, qPrintable(_strError));
            sessionTest.Cancel("protect-recv");
        }
        if (++nFinished == 2) { loopCase.quit(); }
    });
    const bool bStarted = sessionTest.StartSend("protect-send", {strFile}, "1234-protect-test", stConfig);
    QTimer::singleShot(18000, &loopCase, &QEventLoop::quit); loopCase.exec();
    const bool bPass = bStarted && bChanged && bDetected && nFinished == 2 && _libEngine.GetTaskCount() == 0;
    qInfo("SOURCE_CASE kind=%d pass=%d finished=%d tasks=%d", _nKind, bPass, nFinished, _libEngine.GetTaskCount());
    return bPass;
}

int main(int _nArgc, char* _ppArgv[])
{
    QCoreApplication appTest(_nArgc, _ppArgv);
    if (_nArgc < 3) { return 2; }
    QTemporaryDir dirTest(QString::fromLocal8Bit(_ppArgv[1]) + "/dll-reg-XXXXXX");
    dirTest.setAutoRemove(false);
    if (!dirTest.isValid()) { return 3; }
    QDir().mkpath(dirTest.path()+"/dst");
    const QString strFile=dirTest.path()+QStringLiteral("/中文 payload.bin");
    QFile fileSource(strFile); fileSource.open(QIODevice::WriteOnly); fileSource.write(QByteArray(2*1024*1024,'D')); fileSource.close();
    QTcpServer serverPort; serverPort.listen(QHostAddress::LocalHost,0);
    const quint16 nPort=serverPort.serverPort(); serverPort.close();
    QProcess procRelay; procRelay.start(QString::fromLocal8Bit(_ppArgv[2]), {"relay","--port",QString::number(nPort)});
    if (!procRelay.waitForStarted()) { return 4; }
    bool bRelay=false;
    for (int nAttempt=0; nAttempt<40; ++nAttempt) { QTcpSocket socketProbe; socketProbe.connectToHost("127.0.0.1",nPort); if(socketProbe.waitForConnected(100)){bRelay=true;break;} QThread::msleep(100); }
    CrocLibrary& libEngine=CrocLibrary::Instance();
    if (!bRelay || !libEngine.Load() || libEngine.Initialize()!=0) { procRelay.kill();procRelay.waitForFinished(); return 5; }
    bool bPass=true, bReady=false, bReceiveStarted=false; int nFinished=0, nProgress=0;
    {
        CrocDllSession sessionTest(&libEngine);
        SCrocConfig stConfig; stConfig.strRelay="127.0.0.1:"+QString::number(nPort); stConfig.strCurve="invalid";
        bPass &= !sessionTest.StartSend("invalid",{strFile},"secret-regression",stConfig) && !sessionTest.GetLastError().isEmpty();
        stConfig.strCurve="p256"; stConfig.dUploadLimitMbps=1;
        QEventLoop loopTransfer;
        QObject::connect(&sessionTest,&CrocDllSession::promptRequested,&loopTransfer,[&](const QString& _strTask,const QString& _strPrompt,const QString& _strKind,const QVariantMap&){
            bPass &= _strTask == "recv-ui-id" && _strKind == "receive_accept";
            bPass &= !sessionTest.AnswerPrompt("wrong-task",_strPrompt,1);
            bPass &= sessionTest.AnswerPrompt(_strTask,_strPrompt,1);
            bPass &= !sessionTest.AnswerPrompt(_strTask,_strPrompt,1);
        });
        QObject::connect(&sessionTest,&CrocDllSession::taskUpdated,&loopTransfer,[&](const STaskSnapshot& _stTask){
            bPass &= _stTask.strTaskId=="send-ui-id" || _stTask.strTaskId=="recv-ui-id";
            if (_stTask.dPercent>0 && _stTask.dPercent<100) {++nProgress;}
            if (_stTask.strTaskId=="send-ui-id" && _stTask.bShareReady && !bReceiveStarted) {
                bReady=!_stTask.strCode.isEmpty(); bReceiveStarted=true;
                const QString strCode=_stTask.strCode;
                QTimer::singleShot(0,&loopTransfer,[&,strCode]{ bPass &= sessionTest.StartReceive("recv-ui-id",strCode,dirTest.path()+"/dst",stConfig); });
            }
        });
        QObject::connect(&sessionTest,&CrocDllSession::taskFinished,&loopTransfer,[&](const QString&,int _nExit,bool _bCancel,const QString& _strError){bPass &= _nExit==0 && !_bCancel && _strError.isEmpty(); if(++nFinished==2)loopTransfer.quit();});
        const bool bStarted=sessionTest.StartSend("send-ui-id",{strFile},"1234-test-secret",stConfig); qInfo("START_RESULT %d %s", bStarted, qPrintable(sessionTest.GetLastError())); bPass &= bStarted;
        QTimer::singleShot(20000,&loopTransfer,&QEventLoop::quit); loopTransfer.exec();
        bPass &= nFinished==2 && bReady && nProgress>0 && libEngine.GetTaskCount()==0;
    }
    QFile fileReceived(dirTest.path()+QStringLiteral("/dst/中文 payload.bin")); fileReceived.open(QIODevice::ReadOnly); fileSource.open(QIODevice::ReadOnly);
    bPass &= fileReceived.size()==fileSource.size() && QCryptographicHash::hash(fileReceived.readAll(),QCryptographicHash::Sha256)==QCryptographicHash::hash(fileSource.readAll(),QCryptographicHash::Sha256);
    bool bFailed=false;
    {
        CrocDllSession sessionFailure(&libEngine); QEventLoop loopFailure;
        QObject::connect(&sessionFailure,&CrocDllSession::taskFinished,&loopFailure,[&](const QString&,int _nExit,bool _bCancel,const QString& _strError){bFailed=_nExit!=0 && !_bCancel && !_strError.isEmpty();loopFailure.quit();});
        SCrocConfig stBad; stBad.strRelay="127.0.0.1:1";stBad.strCurve="p256";
        bPass &= sessionFailure.StartSend("bad-relay",{strFile},"secret-regression",stBad);
        QTimer::singleShot(15000,&loopFailure,&QEventLoop::quit);loopFailure.exec();
        bPass &= bFailed && libEngine.GetTaskCount()==0;
    }
    SCrocConfig stPromptConfig; stPromptConfig.strRelay="127.0.0.1:"+QString::number(nPort); stPromptConfig.strCurve="p256";
    for (int nDecision : {0, 2, 3}) { bPass &= RunPromptScenario(libEngine, stPromptConfig, strFile, dirTest.path()+"/choice-"+QString::number(nDecision), nDecision); }
    for (int nKind : {0, 1, 2}) { bPass &= RunSourceChange(libEngine, stPromptConfig, dirTest.path()+"/source-"+QString::number(nKind), nKind); }
    libEngine.Shutdown();procRelay.kill();procRelay.waitForFinished();
    qInfo("DLL_REGRESSION pass=%d stable_ids=1 ready=%d finished=%d progress_samples=%d failure_preserved=%d",bPass,bReady,nFinished,nProgress,bFailed);
    return bPass?0:1;
}
