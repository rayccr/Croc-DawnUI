import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Controls.Basic as Controls

// 历史页：本地记录、名称搜索、收发筛选与清空。
// 记录来自 AppController 的 HistoryModel，已写入本地 JSON Lines 文件。
ColumnLayout {
    id: historyPage
    property bool narrow: false
    property string filter: "全部"
    readonly property bool android: Qt.platform.os === "android"

    FolderDialog {
        id: exportDialog
        property string sourcePath: ""
        title: I18n.t("选择导出文件夹")
        onAccepted: AppController.exportAndroidReceive(sourcePath, String(selectedFolder))
    }

    Controls.Popup {
        id: clearPopup; anchors.centerIn: parent; width: Math.min(420, historyPage.width); modal: true; padding: 20
        background: GlassSurface { glassTint: Theme.panel; tintStrength: 0.78; glassOpacity: 0.98; radius: 12 }
        contentItem: ColumnLayout {
            spacing: 12
            BodyText { text: I18n.t("清空全部历史？") }
            Hint { text: I18n.t("只删除历史记录，不删除传输文件或部分文件。此操作不可撤回。"); Layout.fillWidth: true }
            RowLayout {
                AppButton { text: I18n.t("取消"); onClicked: clearPopup.close() }
                AppButton { text: I18n.t("确认清空"); onClicked: { AppController.history.clearAll(); clearPopup.close(); } }
            }
        }
    }
    spacing: Theme.compactDesktop ? 10 : 20

    Panel {
        Layout.fillWidth: true

        GridLayout {
            Layout.fillWidth: true
            columns: historyPage.narrow ? 1 : 2

            FieldBox {
                id: searchInput
                Layout.fillWidth: true
                placeholderText: I18n.t("搜索文件名…")
                onTextChanged: AppController.history.setSearch(text)
            }

            RowLayout {
                Repeater {
                    model: ["全部", "发送", "接收"]
                    AppButton {
                        required property string modelData
                        text: I18n.t(modelData)
                        tint: historyPage.filter === modelData ? Theme.historySurface : "transparent"
                        onClicked: {
                            historyPage.filter = modelData;
                            AppController.history.setDirection(modelData);
                        }
                    }
                }
            }
        }

        ListView {
            id: historyList
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(380, Math.max(60, contentHeight))
            clip: true
            spacing: Theme.compactDesktop ? 6 : 8
            model: AppController.history
            boundsBehavior: Flickable.StopAtBounds

            delegate: ColumnLayout {
                id: historyRow
                required property string name
                required property string direction
                required property string sizeText
                required property string result
                required property string peer
                required property string timeText
                required property string error
                required property string location
                required property string files
                property bool expanded: false
                width: historyList.width

                Layout.fillWidth: true
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.compactDesktop ? 8 : 14

                    Rectangle {
                        width: Theme.compactDesktop ? 32 : 42
                        height: width
                        radius: Theme.radiusControl
                        color: historyRow.direction === "发送" ? Theme.sendSurface : Theme.receiveSurface
                        UiIcon {
                            anchors.centerIn: parent
                            symbol: historyRow.direction === "发送" ? "up" : "down"
                            tone: historyRow.direction === "发送" ? "send" : "receive"
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        BodyText {
                            text: historyRow.name
                            Layout.fillWidth: true
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                            wrapMode: Text.NoWrap
                        }
                        Hint {
                            text: I18n.t(historyRow.direction) + " · " + historyRow.sizeText + " · " + historyRow.timeText
                            Layout.fillWidth: true
                        }
                        Hint {
                            text: I18n.t("对端：") + (historyRow.peer !== "" ? historyRow.peer : I18n.t("未知"))
                            Layout.fillWidth: true
                        }
                        Hint {
                            visible: historyRow.error !== ""
                            text: historyRow.error
                            color: "#ff9c8a"
                            Layout.fillWidth: true
                        }
                    }

                    Hint {
                        visible: true
                        text: I18n.t(historyRow.result)
                        color: historyRow.result === "已完成" || historyRow.result.indexOf("接收结束") >= 0 ? Theme.green
                               : (historyRow.result.indexOf("本端发送结束") >= 0 ? Theme.blue
                               : (historyRow.result === "已取消" ? Theme.muted : "#ff9c8a"))
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    AppButton { text: historyRow.expanded ? I18n.t("收起详情") : I18n.t("任务详情"); onClicked: historyRow.expanded = !historyRow.expanded }
                    Item { Layout.fillWidth: true }
                    AppButton { visible: !historyPage.android; text: I18n.t("打开位置"); enabled: historyRow.location !== "" && (historyRow.result.indexOf("接收结束") >= 0 || historyRow.result.indexOf("本端发送结束") >= 0); onClicked: AppController.openLocation(historyRow.location) }
                    AppButton {
                        visible: historyPage.android && historyRow.direction === "接收"
                                 && historyRow.location !== "" && historyRow.result.indexOf("接收结束") >= 0
                        enabled: !AppController.androidFileBusy
                        text: I18n.t("导出文件")
                        onClicked: { exportDialog.sourcePath = historyRow.location; exportDialog.open(); }
                    }
                }
                Hint {
                    visible: historyRow.expanded
                    text: I18n.t("位置：") + (historyRow.location || I18n.t("旧记录未保存位置")) + I18n.t("\n文件：\n") + (historyRow.files || I18n.t("未取得完整清单")) + I18n.t("\n对端信息由引擎提供，地址不作为身份认证。")
                    Layout.fillWidth: true
                }
                Rule { }
            }

            Hint {
                anchors.centerIn: parent
                text: searchInput.text !== "" || historyPage.filter !== "全部" ? I18n.t("没有匹配记录") : I18n.t("暂无传输记录")
                visible: historyList.count === 0
            }
        }

        Rule { }

        RowLayout {
            Layout.fillWidth: true
            Hint {
                text: I18n.t("记录保存在本地，包含文件路径、结果与引擎提供的对端信息，不保存口令或密钥。")
                Layout.fillWidth: true
            }
            AppButton {
                text: I18n.t("清空历史")
                enabled: AppController.history.visibleCount > 0
                onClicked: clearPopup.open()
            }
        }
    }
}
