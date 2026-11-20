/**
****************************************************************
* @file    RegressionTests.cpp
* @brief   验收风险回归：文件保护、配置失败、确认隔离、进程收尾
* @author  Ray
* @date    2026-11-01
****************************************************************
*/
#include "AppController.h"
#include "CrocAdapter.h"
#include "TransferSafety.h"
#include "EngineSelfTest.h"
#include "CrocJsonMap.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>
#include <functional>
#include <cstdio>

namespace
{
int nFailures = 0;
void Check(bool _bOk, const char* _pName)
{
    std::printf("%s %s\n", _bOk ? "PASS" : "FAIL", _pName);
    if (!_bOk) { ++nFailures; }
}
bool WaitFor(const std::function<bool()>& _fnReady, int _nTimeout = 4000)
{
    QEventLoop loopWait;
    QTimer timerPoll;
    timerPoll.setInterval(5);
    QObject::connect(&timerPoll, &QTimer::timeout, &loopWait, [&]() { if (_fnReady()) { loopWait.quit(); } });
    QTimer::singleShot(_nTimeout, &loopWait, &QEventLoop::quit);
    if (_fnReady()) { return true; }
    timerPoll.start(); loopWait.exec(); return _fnReady();
}
void WriteFile(const QString& _strPath, const QByteArray& _arrData)
{
    QFile fileOutput(_strPath);
    Check(fileOutput.open(QIODevice::WriteOnly), "create isolated fixture");
    fileOutput.write(_arrData);
}
}

int main(int _nArgc, char* _ppArgv[])
{
    QGuiApplication appTest(_nArgc, _ppArgv);
    STaskSnapshot stMapped;
    QString strMapError;
    Check(MapCrocSnapshotJson(R"({"direction":"receive","phase":"verifying","state":"running","peer_address":"192.168.1.3","totals":{"files":2,"bytes_total":1000},"file":{"index":1,"name":"b.bin","size":200,"bytes_done":50},"transfer":{"percent":62.5,"speed_bps":null,"eta_seconds":null},"verification":{"status":"running","bytes_done":420,"bytes_total":1000}})", stMapped, strMapError)
        && stMapped.dPercent == 62.5 && stMapped.m_dFilePercent == 25 && stMapped.m_dVerifyPercent == 42
        && stMapped.nFileIndex == 2 && stMapped.nFileCount == 2 && stMapped.strPeer == "192.168.1.3"
        && stMapped.strSpeed.isEmpty() && !stMapped.bTerminal, "independent batch file verification progress");
    Check(MapCrocSnapshotJson(R"({"phase":"awaiting_confirmation","state":"running","transfer":{"percent":null},"verification":{"bytes_total":null,"bytes_done":null}})", stMapped, strMapError)
        && stMapped.dPercent < 0 && stMapped.m_dVerifyPercent < 0 && stMapped.enStage == ETaskStage::STAGE_AWAITING_CONFIRM, "null is unknown and confirmation phase mapped");
    Check(MapCrocSnapshotJson(R"({"phase":"failed","state":"failed"})", stMapped, strMapError) && stMapped.bTerminal && stMapped.enStage == ETaskStage::STAGE_FAILED, "ABI terminal state mapped");
    QTemporaryDir dirTest;
    Check(dirTest.isValid(), "isolated directory");
    const QString strRoot = dirTest.path();
    const QString strSource = strRoot + QStringLiteral("/资料 space.bin");
    const QString strFake = QCoreApplication::applicationDirPath() + QStringLiteral("/FakeCroc.exe");
    WriteFile(strSource, "abc");
    const QVariantMap hashManifest = TransferSafety::InspectFiles({strSource});
    Check(hashManifest.value(QStringLiteral("error")).toString().isEmpty() && hashManifest.value(QStringLiteral("bytes")).toLongLong() == 3, "exact unicode manifest");
    Check(TransferSafety::FilesUnchanged(hashManifest.value(QStringLiteral("files")).toList()), "unchanged identity");
    WriteFile(strSource, "modified");
    Check(!TransferSafety::FilesUnchanged(hashManifest.value(QStringLiteral("files")).toList()), "changed source detected");
    Check(!TransferSafety::InspectFiles({strRoot + QStringLiteral("/missing")}).value(QStringLiteral("error")).toString().isEmpty(), "missing source rejected");
    Check(RunEngineSelfTest(strFake, strRoot) == 2 && QFile::exists(strSource), "selftest refuses existing directory without deleting");

    const QString strConfig = strRoot + QStringLiteral("/config.json");
    qputenv("FLOWDROP_CONFIG_PATH", strConfig.toUtf8());
    qputenv("FLOWDROP_HISTORY_PATH", (strRoot + QStringLiteral("/history.jsonl")).toUtf8());
    qputenv("FLOWDROP_CROC_PATH", strFake.toUtf8());
    qputenv("FLOWDROP_ENGINE_HOME", (strRoot + QStringLiteral("/home")).toUtf8());
    ConfigStore storeConfig;
    QVariantMap hashSettings = storeConfig.GetValues();
    hashSettings.insert(QStringLiteral("password"), QStringLiteral("private-relay-password"));
    storeConfig.SetValues(hashSettings);
    Check(storeConfig.Save(), "atomic config save");
    QFile fileConfig(strConfig); fileConfig.open(QIODevice::ReadOnly);
    Check(!fileConfig.readAll().contains("private-relay-password"), "password absent from plain json"); fileConfig.close();
    ConfigStore storeReload; storeReload.Load();
    Check(storeReload.GetValues().value(QStringLiteral("password")).toString().isEmpty(), "password not restored from disk");
    WriteFile(strConfig, "{\"password\":\"old-secret\",\"rate\":\"0\"}");
    ConfigStore storeMigration;
    Check(storeMigration.Load(), "legacy password migration");
    fileConfig.open(QIODevice::ReadOnly);
    Check(!fileConfig.readAll().contains("old-secret"), "legacy plaintext removed atomically"); fileConfig.close();
    hashSettings.insert(QStringLiteral("rate"), QStringLiteral("invalid"));
    Check(!ConfigStore::Validate(hashSettings).isEmpty(), "invalid rate rejected");
    hashSettings.insert(QStringLiteral("rate"), QStringLiteral("0"));
    hashSettings.insert(QStringLiteral("policy"), QStringLiteral("指定路径"));
    Check(!ConfigStore::Validate(hashSettings).isEmpty(), "unsupported restricted policy refused");
    hashSettings.insert(QStringLiteral("policy"), QStringLiteral("自动连接"));
    hashSettings.insert(QStringLiteral("proxy"), QStringLiteral("SOCKS5"));
    Check(!ConfigStore::Validate(hashSettings).isEmpty(), "unverified proxy refused");
    hashSettings.insert(QStringLiteral("proxy"), QStringLiteral("关闭"));

    AppController appController;
    Check(appController.localPath(QUrl::fromLocalFile(strSource).toString()) == strSource, "local URL handled by QUrl");
    Check(appController.localPath(QStringLiteral("https://example.com/file")).isEmpty(), "remote URL rejected");
    Check(appController.startReceive(QStringLiteral("update"), strRoot).isEmpty(), "CLI subcommand rejected as receive code");
    const QVariantMap hashBefore = appController.GetSettings();
    qputenv("FLOWDROP_CONFIG_PATH", strRoot.toUtf8());  // 目标是目录，写入必须失败
    Check(!appController.saveSettings(hashSettings) && appController.GetSettings() == hashBefore, "failed save keeps prior settings");
    qputenv("FLOWDROP_CONFIG_PATH", strConfig.toUtf8());

    qputenv("CROCD_FAKE_MODE", "receive");
    int nPrompts = 0;
    int nFinished = 0;
    QObject::connect(&appController, &AppController::receivePrompt, [&]() { ++nPrompts; });
    QObject::connect(&appController, &AppController::taskFinished, [&]() { ++nFinished; });
    const QString strRecvA = appController.startReceive(QStringLiteral("alpha-code"), strRoot + QStringLiteral("/recvA"));
    Check(appController.startReceive(QStringLiteral("other-code"), strRoot + QStringLiteral("/recvA")).isEmpty(), "same receive directory reserved while active");
    Check(WaitFor([&]() { return nPrompts == 1; }), "first receive prompt");
    const QString strPromptA = appController.GetPromptId();
    const QString strRecvB = appController.startReceive(QStringLiteral("bravo-code"), strRoot + QStringLiteral("/recvB"));
    Check(WaitFor([&]() { return nPrompts == 2; }), "second receive prompt queued");
    Check(appController.GetPromptId() == strPromptA, "second task cannot overwrite current prompt");
    appController.answerPromptFor(QStringLiteral("stale-id"), true);
    Check(appController.GetPromptId() == strPromptA, "stale prompt answer ignored");
    appController.answerPromptFor(strPromptA, true);
    const QString strPromptB = appController.GetPromptId();
    Check(!strPromptB.isEmpty() && strPromptB != strPromptA, "next prompt has independent id");
    appController.answerPromptFor(strPromptB, false);
    Check(WaitFor([&]() { return nFinished == 2; }), "two task terminal events");
    Check(QFile::exists(strRoot + QStringLiteral("/recvA/mock.bin")) && !QFile::exists(strRoot + QStringLiteral("/recvB/mock.bin")), "accept/reject target correct task");
    Check(appController.getTask(strRecvB).value(QStringLiteral("stage")).toString().contains(QStringLiteral("跳过")), "zero exit with rejection is not success");
    Check(!appController.GetPromptVisible(), "finished prompts removed");

    qputenv("CROCD_FAKE_MODE", "long-name");
    const QString strRecvLong = appController.startReceive(QStringLiteral("long-file-code"), strRoot + QStringLiteral("/recvLong"));
    Check(WaitFor([&]() { return appController.GetPromptVisible(); }), "long filename confirmation");
    appController.answerPrompt(true);
    Check(WaitFor([&]() { return appController.getTask(strRecvLong).value(QStringLiteral("terminal")).toBool(); }), "long filename receive end");
    const QVariantMap hashLongTask = appController.getTask(strRecvLong);
    Check(hashLongTask.value(QStringLiteral("error")).toString().isEmpty()
        && hashLongTask.value(QStringLiteral("stage")).toString().contains(QStringLiteral("接收结束"))
        && hashLongTask.value(QStringLiteral("fileCount")).toInt() == 1
        && hashLongTask.value(QStringLiteral("name")).toString() == QStringLiteral("41135911C8237A7A69F9968D4A19E311.jpg"), "abbreviated progress name cannot turn success into failure");

    SCrocConfig stConfig;
    stConfig.strCrocPath = strFake;
    stConfig.strWorkHome = strRoot + QStringLiteral("/home");
    qputenv("CROCD_FAKE_MODE", "send-success");
    {
        CrocAdapter adapterTest(stConfig);
        STaskSnapshot stLast;
        QObject::connect(&adapterTest, &CrocAdapter::TaskUpdated, [&](const STaskSnapshot& _stTask) { stLast = _stTask; });
        adapterTest.StartSend(QStringLiteral("send-natural"), {strSource});
        Check(WaitFor([&]() { return stLast.bTerminal; }), "natural send end");
        Check(stLast.enStage == ETaskStage::STAGE_SENT && stLast.strCode.isEmpty() && !stLast.bShareReady, "sender does not claim remote saved and clears secret");
    }
    qputenv("CROCD_FAKE_MODE", "fail-tail");
    {
        CrocAdapter adapterTest(stConfig);
        STaskSnapshot stLast;
        QObject::connect(&adapterTest, &CrocAdapter::TaskUpdated, [&](const STaskSnapshot& _stTask) { stLast = _stTask; });
        adapterTest.StartReceive(QStringLiteral("tail"), QStringLiteral("secret-test-code"), strRoot);
        Check(WaitFor([&]() { return stLast.bTerminal; }), "unterminated output processed");
        Check(stLast.enStage == ETaskStage::STAGE_FAILED && stLast.strError.contains(QStringLiteral("权限")) && !stLast.strError.contains(QStringLiteral("secret-test-code")), "error actionable and redacted");
    }
    qputenv("CROCD_FAKE_MODE", "slow");
    {
        CrocAdapter adapterTest(stConfig);
        int nEnd = 0;
        QObject::connect(&adapterTest, &CrocAdapter::TaskFinished, [&]() { ++nEnd; });
        adapterTest.StartSend(QStringLiteral("cancel"), {strSource});
        adapterTest.Cancel(QStringLiteral("cancel"));
        adapterTest.Cancel(QStringLiteral("cancel"));
        Check(WaitFor([&]() { return nEnd == 1; }), "cancel finishes exactly once");
    }
    {
        CrocAdapter adapterTest(stConfig);
        adapterTest.StartSend(QStringLiteral("shutdown-A"), {strSource});
        adapterTest.StartSend(QStringLiteral("shutdown-B"), {strSource});
        adapterTest.Shutdown();
        Check(!adapterTest.IsRunning(QStringLiteral("shutdown-A")) && !adapterTest.IsRunning(QStringLiteral("shutdown-B")), "active shutdown safe and repeatable");
        adapterTest.Shutdown();
    }
    {
        CrocAdapter adapterTest(stConfig);
        STaskSnapshot stLast;
        QObject::connect(&adapterTest, &CrocAdapter::TaskUpdated, [&](const STaskSnapshot& _stTask) { stLast = _stTask; });
        adapterTest.StartSend(QStringLiteral("source-change"), {strSource});
        WriteFile(strSource, "changed during transfer");
        Check(WaitFor([&]() { return stLast.bTerminal; }), "runtime source change stops task");
        Check(stLast.enStage == ETaskStage::STAGE_FAILED, "changed source is failure, not user cancellation");
    }
    std::printf("Failures: %d\n", nFailures);
    return nFailures == 0 ? 0 : 1;
}
