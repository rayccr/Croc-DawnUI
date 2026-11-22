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
    readonly property bool android: Qt.platform.os === "android"
    readonly property string enteredCode: codeField.text.trim()
    readonly property bool canStart: enteredCode.length >= 6 && savePath !== "" && AppController.engineAvailable
    Connections {
        target: AppController
        function onSettingsChanged() { if (receivePage.useDefaultPath) { receivePage.savePath = AppController.defaultSaveDir; } }
    }

    signal taskStarted(string taskId)

    function startReceive() {
        if (!canStart) {
            return;
        }
        var strTaskId = AppController.startReceive(enteredCode, savePath);
        if (strTaskId !== "") {
            taskStarted(strTaskId);
        }
    }

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
            onAccepted: receivePage.startReceive()
        }

        Hint {
            text: I18n.t("请使用发送方分享的当前口令。")
            Layout.fillWidth: true
        }

        BodyText {
            text: I18n.t("保存位置")
            font.pixelSize: 13
        }

        RowLayout {
            visible: !receivePage.android
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
            visible: !receivePage.android && !receivePage.useDefaultPath
            onClicked: { receivePage.useDefaultPath = true; receivePage.savePath = AppController.defaultSaveDir; }
        }
        Hint {
            visible: receivePage.android
            text: I18n.t("文件先保存到应用内，完成后可从传输历史导出。")
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            AppButton {
                text: I18n.t("开始接收")
                tint: Theme.green
                labelColor: Theme.onAccent
                enabled: receivePage.canStart
                onClicked: receivePage.startReceive()
            }
            Hint {
                text: {
                    if (!AppController.engineAvailable) {
                        return I18n.t("接收引擎不可用，请检查安装后重启。");
                    }
                    if (receivePage.savePath === "") {
                        return I18n.t("请先选择保存位置");
                    }
                    if (receivePage.enteredCode.length < 6) {
                        return I18n.t("口令至少 6 个字符");
                    }
                    return I18n.t("点击开始接收，或按回车。");
                }
                Layout.fillWidth: true
            }
        }
    }
}
