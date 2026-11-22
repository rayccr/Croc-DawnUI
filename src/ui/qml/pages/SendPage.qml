import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts

// 发送页：选择文件 / 文件夹、待发送清单、真实口令与发送操作。
// 显示当前会话的口令，复制仍以 AppController 的真实就绪状态为准。
ColumnLayout {
    id: sendPage
    objectName: "sendPage"
    property var selectedUrls: []
    property bool narrow: false
    property string activeTaskId: ""
    property bool revealCode: AppController.settings.mask !== "默认遮挡"
    readonly property var selectedItems: AppController.describeSelection(selectedUrls)
    readonly property int selectionCount: selectedItems.length
    property var activeTask: null
    property string startError: ""
    readonly property bool android: Qt.platform.os === "android"
    function refreshActiveTask() {
        activeTask = activeTaskId !== "" ? AppController.getTask(activeTaskId) : null;
    }
    onActiveTaskIdChanged: refreshActiveTask()
    Connections {
        target: AppController
        function onTaskRevisionChanged() { sendPage.refreshActiveTask(); }
        function onCodeReady(taskId, code) { if (taskId === sendPage.activeTaskId) { sendPage.refreshActiveTask(); } }
        function onAndroidDocumentsImported(paths) { sendPage.addUrls(paths); }
    }
    readonly property string displayCode: activeTask && !activeTask.terminal ? activeTask.code : ""
    readonly property string shareCode: activeTask && activeTask.shareReady ? displayCode : ""
    readonly property bool customCode: AppController.settings.phrase === "单次任务自定义"
    Timer { id: codeTimer; interval: 15000; onTriggered: sendPage.revealCode = AppController.settings.mask !== "默认遮挡" }
    function addUrls(urls) {
        var values = selectedUrls.slice();
        for (var i = 0; i < urls.length; ++i) { values.push(String(urls[i])); }
        var checked = AppController.describeSelection(values);
        selectedUrls = checked.map(function(item) { return item.path; });
        activeTaskId = "";
        revealCode = AppController.settings.mask !== "默认遮挡";
        if (checked.length < values.length) { notifyRequested(I18n.t("已去重并忽略不存在或不支持的非本地路径，请核对清单。")); }
    }
    function addPickedUrls(urls) {
        if (!android) { addUrls(urls); return; }
        AppController.importAndroidDocuments(Array.from(urls, url => String(url)));
    }
    function removeItem(index) {
        var values = selectedUrls.slice(); values.splice(index, 1); selectedUrls = values;
        activeTaskId = ""; revealCode = AppController.settings.mask !== "默认遮挡";
    }

    function startSelectedTask() {
        var lstPaths = sendPage.selectedUrls.map(function(url) { return sendPage.localPath(url); });
        sendPage.startError = "";
        var strTaskId = AppController.startSend(lstPaths, sendPage.customCode ? customCodeField.text : "");
        if (strTaskId !== "") {
            sendPage.activeTaskId = strTaskId;
            sendPage.refreshActiveTask();
            sendPage.revealCode = AppController.settings.mask !== "默认遮挡";
            codeTimer.stop();
            customCodeField.clear();
            sendPage.taskStarted(strTaskId);
        } else {
            sendPage.startError = AppController.lastEventText;
        }
        return strTaskId;
    }

    signal taskStarted(string taskId)
    signal notifyRequested(string text)

    spacing: 20

    FileDialog {
        id: fileDialog
        title: I18n.t("选择要发送的文件")
        fileMode: FileDialog.OpenFiles
        onAccepted: sendPage.addPickedUrls(selectedFiles)
    }

    FolderDialog {
        id: folderDialog
        title: I18n.t("选择要发送的文件夹")
        options: sendPage.android ? FolderDialog.ReadOnly : 0
        onAccepted: sendPage.addPickedUrls([selectedFolder])
    }

    // 把 QML 的 file:// URL 转换成 croc 可接受的本地路径
    function localPath(urlValue) {
        return AppController.localPath(String(urlValue));
    }

    Panel {
        Layout.fillWidth: true

        GlassSurface {
            Layout.fillWidth: true
            implicitHeight: 218
            radius: 12
            glassTint: Theme.field
            accent: Theme.blue
            tintStrength: 0.60
            border.color: dropArea.containsDrag ? Theme.blue : Theme.glassEdge

            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 32
                spacing: 12
                UiIcon {
                    symbol: "up"
                    tone: "send"
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    Layout.alignment: Qt.AlignHCenter
                }
                BodyText {
                    text: I18n.t(sendPage.android ? "选择发送内容" : "将文件拖到这里")
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignHCenter
                }
                Hint {
                    text: I18n.t(sendPage.android ? "从设备中选择文件或文件夹。" : "或从设备中选择文件 / 文件夹")
                    Layout.alignment: Qt.AlignHCenter
                }
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    AppButton {
                        text: I18n.t("选择文件")
                        enabled: !AppController.androidFileBusy
                        onClicked: fileDialog.open()
                    }
                    AppButton {
                        text: I18n.t("选择文件夹")
                        enabled: !AppController.androidFileBusy
                        onClicked: folderDialog.open()
                    }
                }
            }

            // 原生区域明确处理 DragEnter/Move/Drop，固定为 Copy，不移动原文件。
            FileDropArea {
                id: dropArea
                objectName: "sendDropArea"
                anchors.fill: parent
                visible: !sendPage.android
                onFilesDropped: function(paths) { sendPage.addUrls(paths); }
            }
        }

        Hint { text: AppController.androidFileBusy ? I18n.t("正在准备文件…") : (sendPage.selectedItems.length ? (sendPage.activeTask ? I18n.t("本次任务文件 · ") : I18n.t("待发送清单 · ")) + sendPage.selectedItems.length + I18n.t(" 项") : I18n.t("尚未选择文件")); Layout.fillWidth: true }
        Repeater {
            model: sendPage.selectedItems
            RowLayout {
                required property int index
                required property var modelData
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true
                    BodyText { text: modelData.name + (modelData.directory ? I18n.t(" · 文件夹") : " · " + modelData.bytes + I18n.t(" 字节")); Layout.fillWidth: true; elide: Text.ElideRight; wrapMode: Text.NoWrap }
                    Hint { visible: !sendPage.android; text: modelData.path; Layout.fillWidth: true }
                }
                AppButton { text: I18n.t("移除"); implicitWidth: 66; onClicked: sendPage.removeItem(index) }
            }
        }
        FieldBox {
            id: customCodeField; visible: sendPage.customCode; Layout.fillWidth: true
            placeholderText: I18n.t("本次自定义口令（至少 6 字符）"); echoMode: sendPage.revealCode ? TextInput.Normal : TextInput.Password
        }
        Hint { visible: sendPage.customCode; text: I18n.t("使用英文字母、数字和短横线。请使用难猜的口令，并确认对方曲线和中继配置兼容。"); Layout.fillWidth: true }

        BodyText {
            text: I18n.t("分享口令")
            font.pixelSize: 13
        }

        GridLayout {
            Layout.fillWidth: true
            columns: sendPage.narrow ? 2 : 3
            rowSpacing: 8
            columnSpacing: 6
            FieldBox {
                Layout.fillWidth: true
                Layout.columnSpan: sendPage.narrow ? 2 : 1
                objectName: "shareCodeField"
                text: sendPage.displayCode
                echoMode: sendPage.revealCode ? TextInput.Normal : TextInput.Password
                placeholderText: sendPage.activeTask ? (sendPage.activeTask.terminal ? I18n.t("任务已结束，重新发送将生成新口令") : I18n.t("等待任务启动")) : (sendPage.selectedUrls.length ? I18n.t("启动发送后显示口令") : I18n.t("先选择要发送的文件"))
                readOnly: true
            }
            AppButton {
                text: sendPage.revealCode ? I18n.t("隐藏口令") : I18n.t("显示口令"); enabled: sendPage.displayCode !== ""
                onClicked: {
                    sendPage.revealCode = !sendPage.revealCode;
                    if (sendPage.revealCode && AppController.settings.mask === "默认遮挡") { codeTimer.restart(); } else { codeTimer.stop(); }
                }
            }
            AppButton {
                text: I18n.t("复制口令")
                symbol: "copy"
                enabled: sendPage.shareCode !== ""
                onClicked: {
                    AppController.copyToClipboard(sendPage.shareCode);
                    sendPage.notifyRequested(I18n.t("口令已复制到剪贴板"));
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            AppButton {
                objectName: "startSendButton"
                text: sendPage.activeTask ? (sendPage.activeTask.terminal ? I18n.t("再次发送") : I18n.t("任务进行中")) : I18n.t("开始发送")
                tint: Theme.blue
                labelColor: Theme.onAccent
                enabled: sendPage.selectedItems.length > 0 && AppController.engineAvailable
                         && !(sendPage.activeTask && !sendPage.activeTask.terminal)
                         && (!sendPage.customCode || customCodeField.text.length >= 6)
                onClicked: {
                    sendPage.startSelectedTask();
                }
            }
            Hint {
                text: {
                    if (sendPage.startError !== "") { return sendPage.startError; }
                    if (!sendPage.activeTask) {
                        return sendPage.selectedUrls.length ? I18n.t("连接就绪后可分享口令") : I18n.t("选择文件后才能开始发送");
                    }
                    var status = I18n.t(sendPage.activeTask.stage);
                    if (sendPage.activeTask.error) { return status + " · " + sendPage.activeTask.error; }
                    if (sendPage.displayCode !== "" && sendPage.shareCode === "") {
                        return status + " · " + I18n.t("口令已生成，连接就绪后可分享");
                    }
                    return status;
                }
                Layout.fillWidth: true
            }
        }
    }
}
