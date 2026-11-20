/**
****************************************************************
* @file    FakeCroc.cpp
* @brief   仅用于回归测试的可控进程，不作为产品传输引擎
* @author  Ray
* @date    2026-11-01
****************************************************************
*/
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QThread>
#include <cstdio>
#include <iostream>

int main(int _nArgc, char* _ppArgv[])
{
    QCoreApplication appTest(_nArgc, _ppArgv);
    const QStringList lstArgs = appTest.arguments();
    if (lstArgs.contains(QStringLiteral("--version"))) { std::puts("croc version 11.6.0 (test double)"); return 0; }
    const QString strMode = qEnvironmentVariable("CROCD_FAKE_MODE");
    if (strMode == QStringLiteral("fail-tail")) { std::fputs("[error] permission denied secret-test-code", stdout); std::fflush(stdout); return 1; }
    if (strMode == QStringLiteral("skip")) { std::fputs("No files transferred.\n", stderr); std::fflush(stderr); return 0; }
    if (lstArgs.contains(QStringLiteral("send")))
    {
        std::fputs("On the other computer, run:\n  croc secret-test-code\n", stderr); std::fflush(stderr);
        if (strMode == QStringLiteral("slow")) { QThread::sleep(10); }
        if (strMode == QStringLiteral("send-ui")) { QThread::msleep(700); }
        return 0;
    }
    const QString strFileName = strMode == QStringLiteral("long-name")
        ? QStringLiteral("41135911C8237A7A69F9968D4A19E311.jpg") : QStringLiteral("mock.bin");
    std::fprintf(stderr, "Accept '%s' (3 B) from 'test-peer'? (Y/n)", qPrintable(strFileName)); std::fflush(stderr);
    std::string strAnswer;
    std::getline(std::cin, strAnswer);
    if (strAnswer != "y") { std::fputs("\nNo files transferred.\n", stderr); std::fflush(stderr); return 0; }
    const int nOut = lstArgs.indexOf(QStringLiteral("--out"));
    if (nOut < 0 || nOut + 1 >= lstArgs.size()) { return 1; }
    QFile fileTarget(QDir(lstArgs.at(nOut + 1)).filePath(strFileName));
    if (!fileTarget.open(QIODevice::WriteOnly)) { return 1; }
    fileTarget.write("abc"); fileTarget.close();
    std::fprintf(stderr, "\n%s 100%% |xxx| (3/3 B) 1/1\n", strMode == QStringLiteral("long-name") ? "41135911C..." : "mock.bin"); std::fflush(stderr);
    return 0;
}
