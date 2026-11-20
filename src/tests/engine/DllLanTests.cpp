// DLL仅局域网验证：不启动croc.exe，不配置任何可用公网中继。
#include "CrocDllSession.h"
#include "CrocLibrary.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTimer>

int main(int _nArgc, char* _ppArgv[])
{
    QCoreApplication appTest(_nArgc, _ppArgv);
    if (_nArgc < 2) { return 2; }
    QTemporaryDir dirTest(QString::fromLocal8Bit(_ppArgv[1]) + "/dll-lan-XXXXXX");
    dirTest.setAutoRemove(false);
    if (!dirTest.isValid()) { return 3; }
    const QString strSource = dirTest.path() + "/payload.bin";
    QFile fileSource(strSource);
    if (!fileSource.open(QIODevice::WriteOnly)) { return 4; }
    fileSource.write(QByteArray(2 * 1024 * 1024, 'L')); fileSource.close();
    QTcpServer serverOccupied;
    const bool bOccupied = serverOccupied.listen(QHostAddress::Any, 9841);
    CrocLibrary& libEngine = CrocLibrary::Instance();
    if (!libEngine.Load() || libEngine.Initialize() != 0) { qInfo("LAN_LOAD_FAILED %s", qPrintable(libEngine.GetLastErrorText())); return 5; }
    bool bPass = true;
    for (int nRound = 0; nRound < 2; ++nRound)
    {
        const QString strTarget = dirTest.path() + "/dst-" + QString::number(nRound); QDir().mkpath(strTarget);
        CrocDllSession sessionTest(&libEngine);
        SCrocConfig stConfig; stConfig.bLocalOnly = true; stConfig.strCurve = "p256";
        stConfig.strRelay = "198.51.100.1:1"; // 保留文档地址，不应发生公网连接。
        QEventLoop loopCase;
        bool bStartedReceive = false, bReady = false; int nFinished = 0;
        QString strPrimaryPort, strOtherPort;
        QObject::connect(&sessionTest, &CrocDllSession::promptRequested, &loopCase, [&](const QString& _strTask, const QString& _strPrompt, const QString&, const QVariantMap&) { bPass &= sessionTest.AnswerPrompt(_strTask, _strPrompt, 1); });
        QObject::connect(&sessionTest, &CrocDllSession::taskUpdated, &loopCase, [&](const STaskSnapshot& _stTask) {
            if (_stTask.strTaskId == "lan-other") { strOtherPort = _stTask.m_strLocalRelayPort; }
            if (_stTask.strTaskId == "lan-send" && _stTask.bShareReady && !bStartedReceive) {
                bStartedReceive = true; bReady = true; const QString strCode = _stTask.strCode;
                strPrimaryPort = _stTask.m_strLocalRelayPort;
                if (nRound == 1) { stConfig.m_strLanAddress = "127.0.0.1:" + strPrimaryPort; }
                QTimer::singleShot(0, &loopCase, [&, strCode] { bPass &= sessionTest.StartReceive("lan-recv", strCode, strTarget, stConfig); });
            }
        });
        QObject::connect(&sessionTest, &CrocDllSession::taskFinished, &loopCase, [&](const QString& _strTask, int _nExit, bool _bCancel, const QString& _strError) {
            qInfo("LAN_FINISH task=%s exit=%d cancel=%d error=%s", qPrintable(_strTask), _nExit, _bCancel, qPrintable(_strError));
            if (_strTask != "lan-other") { bPass &= _nExit == 0 && !_bCancel && _strError.isEmpty(); }
            else { bPass &= _bCancel; }
            if (_strTask == "lan-recv") { sessionTest.Cancel("lan-other"); }
            if (++nFinished == 3) { loopCase.quit(); }
        });
        bPass &= sessionTest.StartSend("lan-other", {strSource}, "1234-lan-other", stConfig);
        const bool bStarted = sessionTest.StartSend("lan-send", {strSource}, "1234-lan-test-" + QString::number(nRound), stConfig);
        qInfo("LAN_START round=%d result=%d error=%s", nRound, bStarted, qPrintable(sessionTest.GetLastError()));
        bPass &= bStarted;
        if (!bStarted) { break; }
        QTimer::singleShot(18000, &loopCase, &QEventLoop::quit); loopCase.exec();
        QFile fileResult(strTarget + "/payload.bin"); fileSource.open(QIODevice::ReadOnly);
        bPass &= fileResult.open(QIODevice::ReadOnly) && QCryptographicHash::hash(fileResult.readAll(), QCryptographicHash::Sha256) == QCryptographicHash::hash(fileSource.readAll(), QCryptographicHash::Sha256);
        fileSource.close();
        if (nFinished != 3) { sessionTest.Cancel("lan-send"); sessionTest.Cancel("lan-recv"); sessionTest.Cancel("lan-other"); QEventLoop loopCleanup; QTimer::singleShot(1500, &loopCleanup, &QEventLoop::quit); loopCleanup.exec(); }
        bPass &= bReady && nFinished == 3 && libEngine.GetTaskCount() == 0 && !strPrimaryPort.isEmpty() && !strOtherPort.isEmpty() && strPrimaryPort != strOtherPort;
        QTcpServer serverReleased; bPass &= serverReleased.listen(QHostAddress::Any, strPrimaryPort.toUShort());
        qInfo("LAN_PORTS primary=%s other=%s released=%d", qPrintable(strPrimaryPort), qPrintable(strOtherPort), serverReleased.isListening());
        qInfo("LAN_ROUND round=%d pass=%d busy_port_fixture=%d", nRound, bPass, bOccupied);
    }
    libEngine.Shutdown();
    return bPass ? 0 : 1;
}
