import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts

// 接收页：输入口令、选择保存位置、启动接收。
// 接收前确认由引擎提示、界面弹窗完成，不在这一页伪造文件清单。
ColumnLayout {
    id: receivePage
    property string savePath: ""
    property bool narrow: false
    property bool useDefaultPath: true
    Connections {
        target: AppController
        function onSettingsChanged() { if (receivePage.useDefaultPath) { receivePage.savePath = AppController.defaultSaveDir; } }
    }

    signal taskStarted(string taskId)

    // 首次进入时用设置里的默认保存目录，用户仍可自行选择
    Component.onCompleted: {
        if (receivePage.savePath === "") {
            receivePage.savePath = AppController.defaultSaveDir;
        }
    }

    spacing: 20

    FolderDialog {
        id: saveDirDialog
        title: I18n.t("选择保存文件夹")
        onAccepted: { receivePage.savePath = AppController.localPath(String(selectedFolder)); receivePage.useDefaultPath = false; }
    }

    Panel {
        Layout.fillWidth: true

        BodyText {
            text: I18n.t("接收口令")
            font.pixelSize: 13
        }

        FieldBox {
            id: codeField
            Layout.fillWidth: true
            placeholderText: I18n.t("例如 7-word-phrase")
        }

        Hint {
            text: I18n.t("输入发送方分享的口令，双方需要保持在线。")
            Layout.fillWidth: true
        }

        BodyText {
            text: I18n.t("保存位置")
            font.pixelSize: 13
        }

        RowLayout {
            Layout.fillWidth: true
            FieldBox {
                Layout.fillWidth: true
                text: receivePage.savePath !== "" ? receivePage.savePath : I18n.t("尚未选择")
                readOnly: true
            }
            AppButton {
                text: I18n.t("浏览")
                symbol: "folder"
                onClicked: saveDirDialog.open()
            }
        }

        AppButton {
            text: I18n.t("沿用默认目录")
            visible: !receivePage.useDefaultPath
            onClicked: { receivePage.useDefaultPath = true; receivePage.savePath = AppController.defaultSaveDir; }
        }
        RowLayout {
            Layout.fillWidth: true
            AppButton {
                text: I18n.t("开始接收")
                tint: Theme.green
                labelColor: Theme.onAccent
                enabled: codeField.text.trim().length >= 6 && receivePage.savePath !== ""
                             && AppController.engineAvailable
                onClicked: {
                    var strTaskId = AppController.startReceive(codeField.text.trim(), receivePage.savePath);
                    if (strTaskId !== "") {
                        receivePage.taskStarted(strTaskId);
                    }
                }
            }
            Hint {
                text: receivePage.savePath === "" ? I18n.t("请先选择保存位置")
                     : (codeField.text.trim().length < 6 ? I18n.t("口令至少 6 个字符") : I18n.t("对方在线时才可能开始传输"))
                Layout.fillWidth: true
            }
        }
    }
}
