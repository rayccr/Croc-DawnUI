/** @file SendPageCheck.h @brief 开发构建的发送页真实事件链回归检查 */
#ifndef CROCD_SEND_PAGE_CHECK_H
#define CROCD_SEND_PAGE_CHECK_H
#include <QString>
class QQuickWindow;
void StartSendPageCheck(QQuickWindow* _pWindow, const QString& _strFile, const QString& _strReport);
#endif
