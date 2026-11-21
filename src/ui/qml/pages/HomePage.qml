import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

// 首页：三个功能入口、收发分栏的真实任务列表，以及共用的任务详情卡。
// 全部数据来自 AppController 的任务模型，没有任何演示数据。
ColumnLayout {
    id: home
    property bool narrow: false
    property string selectedTaskId: ""
    property bool revealCode: AppController.settings.mask !== "默认遮挡"
    onSelectedTaskIdChanged: { revealCode = AppController.settings.mask !== "默认遮挡"; codeTimer.stop(); if (recoverCodeField) { recoverCodeField.text = ""; } refreshSelectedTask(); }
    Timer { id: codeTimer; interval: 15000; onTriggered: home.revealCode = AppController.settings.mask !== "默认遮挡" }

    property var realTask: null
    Connections { target: AppController; function onTaskRevisionChanged() { home.refreshSelectedTask(); } }
    function refreshSelectedTask() {
        if (selectedTaskId === "") {
            realTask = null;
            return;
        }
        var hashTask = AppController.getTask(selectedTaskId);
        if (hashTask && hashTask.cancelled) {
            selectedTaskId = "";
            return;
        }
        realTask = (hashTask && hashTask.taskId !== undefined) ? hashTask : null;
    }

    readonly property bool hasTask: realTask !== null
    readonly property bool isSend: realTask !== null ? realTask.direction === "send" : true
    readonly property string taskName: hasTask ? realTask.name : ""
    readonly property string taskStage: hasTask ? I18n.t(realTask.stage) : I18n.t("暂无任务")
    readonly property double taskPercent: hasTask ? realTask.percent : -1
    readonly property string taskSpeed: hasTask && realTask.speed !== "" ? realTask.speed : "—"
    readonly property string taskPeer: hasTask && realTask.peer !== "" ? realTask.peer : I18n.t("未知")
    readonly property string taskError: hasTask ? realTask.error : ""
    readonly property string taskFileProgress: (hasTask && realTask.fileCount > 1)
                                                ? (realTask.fileIndex + " / " + realTask.fileCount) : "—"

    signal navigateTo(string page)
    signal notifyRequested(string text)

    spacing: 20

    component DetailButton: AppButton {
        implicitHeight: 36
        padding: 8
    }

    GridLayout {
        Layout.fillWidth: true
        columns: home.narrow ? 1 : 3
        columnSpacing: 12
        rowSpacing: 10

        EntryCard {
            text: I18n.t("发送文件")
            symbol: "up"
            description: I18n.t("选择文件，生成分享口令")
            accent: Theme.blue
            surface: Theme.sendSurface
            tone: "send"
            narrow: home.narrow
            Layout.fillWidth: true
            onClicked: home.navigateTo("send")
        }
        EntryCard {
            text: I18n.t("接收文件")
            symbol: "down"
            description: I18n.t("输入口令，轻松接收文件")
            accent: Theme.green
            surface: Theme.receiveSurface
            tone: "receive"
            narrow: home.narrow
            Layout.fillWidth: true
            onClicked: home.navigateTo("receive")
        }
        EntryCard {
            text: I18n.t("传输历史")
            symbol: "clock"
            description: I18n.t("查看记录，快速找到文件")
            accent: Theme.purple
            surface: Theme.historySurface
            tone: "history"
            narrow: home.narrow
            Layout.fillWidth: true
            onClicked: home.navigateTo("history")
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: home.narrow ? 1 : 2
        columnSpacing: 12
        rowSpacing: 12

        TaskColumn {
            direction: "send"
            taskModel: AppController.sendTasks
            selectedTaskId: home.selectedTaskId
            Layout.fillWidth: true
            onRealTaskSelected: function(taskId) {
                home.selectedTaskId = taskId;
            }
        }
        TaskColumn {
            direction: "receive"
            taskModel: AppController.receiveTasks
            selectedTaskId: home.selectedTaskId
            Layout.fillWidth: true
            onRealTaskSelected: function(taskId) {
                home.selectedTaskId = taskId;
            }
        }
    }

    Panel {
        id: taskDetail
        Layout.fillWidth: true
        padding: 16
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            BodyText {
                text: I18n.t("任务详情")
                font.pixelSize: 18
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            Rectangle {
                Layout.preferredWidth: Math.min(stateLabel.implicitWidth + 18, (home.width - taskDetail.padding * 2) * 0.55)
                Layout.preferredHeight: 26
                radius: 13
                color: home.isSend ? Theme.sendSurface : Theme.receiveSurface
                Hint {
                    id: stateLabel
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 9
                    anchors.rightMargin: 9
                    text: home.hasTask ? home.taskStage : I18n.t("暂无任务")
                    color: home.isSend ? Theme.blue : Theme.green
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.NoWrap
                    elide: Text.ElideRight
                    HoverHandler { id: stateHover }
                    Controls.ToolTip.visible: truncated && stateHover.hovered
                    Controls.ToolTip.text: text
                }
            }
        }

        Hint {
            visible: !home.hasTask
            text: I18n.t("选择任务后查看详细信息")
            Layout.fillWidth: true
        }

        GridLayout {
            visible: home.hasTask
            Layout.fillWidth: true
            columns: 2
            columnSpacing: home.narrow ? 12 : 20
            rowSpacing: 8

            ProgressRing {
                Layout.rowSpan: 2
                Layout.preferredWidth: home.narrow ? 88 : 104
                Layout.preferredHeight: Layout.preferredWidth
                Layout.alignment: Qt.AlignCenter
                progress: home.taskPercent
                accent: home.isSend ? Theme.blue : Theme.green
                caption: home.hasTask && home.realTask.verifying ? I18n.t("校验进度") : (home.hasTask && home.realTask.aggregateProgress ? I18n.t("整批传输进度") : I18n.t("当前文件进度"))
            }

            RowLayout {
                Layout.row: 0
                Layout.column: 1
                spacing: 8
                Layout.fillWidth: true
                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    radius: Theme.radiusControl
                    color: Theme.sendSurface
                    UiIcon {
                        anchors.centerIn: parent
                        symbol: "file"
                        tone: "send"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    BodyText {
                        text: home.taskName
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                    }
                    Hint {
                        text: I18n.t("真实任务")
                    }
                }
            }

            GridLayout {
                Layout.row: 1
                Layout.column: 1
                Layout.fillWidth: true
                columns: 2
                columnSpacing: home.narrow ? 8 : 16
                rowSpacing: 6
                Metric {
                    caption: home.hasTask && home.realTask.aggregateProgress ? I18n.t("整批传输进度") : I18n.t("当前文件进度")
                    value: home.hasTask && home.realTask.transferPercent >= 0 ? home.realTask.transferPercent.toFixed(2) + "%" : "—"
                    Layout.fillWidth: true
                }
                Metric {
                    caption: I18n.t("传输速度")
                    value: home.taskSpeed
                    Layout.fillWidth: true
                }
                Metric {
                    caption: I18n.t("当前文件 / 序号")
                    value: home.taskFileProgress + (home.hasTask && home.realTask.filePercent >= 0 ? " · " + home.realTask.filePercent.toFixed(2) + "%" : "")
                    Layout.fillWidth: true
                }
                Metric {
                    caption: home.isSend ? I18n.t("接收端地址") : I18n.t("发送端地址")
                    value: home.taskPeer
                    Layout.fillWidth: true
                }
            }
        }

        Hint {
            id: taskErrorPreview
            visible: home.taskError !== ""
            text: home.taskError
            color: "#ff9c8a"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            maximumLineCount: 2
            elide: Text.ElideRight
        }
        DetailButton {
            visible: taskErrorPreview.visible && taskErrorPreview.truncated
            text: I18n.t("查看完整错误")
            implicitHeight: 32
            padding: 6
            onClicked: home.notifyRequested(home.taskError)
        }
        GridLayout {
            id: shareActions
            readonly property bool stackCode: taskDetail.width - taskDetail.padding * 2
                                              < revealAction.implicitWidth + copyAction.implicitWidth + columnSpacing * 2 + 120
            visible: home.hasTask && home.realTask.shareReady
            Layout.fillWidth: true
            columns: stackCode ? 2 : 3
            columnSpacing: 8
            rowSpacing: 6
            FieldBox {
                text: home.hasTask ? home.realTask.code : ""
                echoMode: home.revealCode ? TextInput.Normal : TextInput.Password
                readOnly: true
                implicitHeight: 36
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.columnSpan: shareActions.stackCode ? 2 : 1
            }
            DetailButton {
                id: revealAction
                Layout.fillWidth: shareActions.stackCode
                text: home.revealCode ? I18n.t("隐藏口令") : I18n.t("显示口令")
                onClicked: {
                    home.revealCode = !home.revealCode;
                    if (home.revealCode && AppController.settings.mask === "默认遮挡") { codeTimer.restart(); } else { codeTimer.stop(); }
                }
            }
            DetailButton {
                id: copyAction
                Layout.fillWidth: shareActions.stackCode
                text: I18n.t("复制口令")
                onClicked: {
                    AppController.copyToClipboard(home.realTask.code);
                    home.notifyRequested(I18n.t("已复制所选任务的口令"));
                }
            }
        }
        Rule { visible: home.hasTask }

        Hint {
            visible: home.hasTask && home.realTask.retrySeconds >= 0
            text: "自动重连剩余 " + (home.hasTask ? home.realTask.retrySeconds : 0) + " 秒 · 原会话可能已结束，部分文件保留"
            Layout.fillWidth: true
        }
        GridLayout {
            visible: home.hasTask && home.realTask.canRecover
            Layout.fillWidth: true
            columns: home.narrow ? 1 : 2
            rowSpacing: 8

            FieldBox {
                id: recoverCodeField
                visible: home.hasTask && home.realTask.needsCode
                placeholderText: I18n.t("恢复接收：输入对方当前有效口令")
                echoMode: home.revealCode ? TextInput.Normal : TextInput.Password
                Layout.fillWidth: true
            }
            DetailButton {
                text: home.isSend ? I18n.t("检查并恢复发送") : I18n.t("检查并重新接收")
                enabled: home.isSend || recoverCodeField.text.length >= 6
                onClicked: { AppController.resumeTask(home.selectedTaskId, recoverCodeField.text); recoverCodeField.text = ""; }
            }
        }
        Flow {
            visible: home.hasTask
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 8

            DetailButton {
                visible: home.hasTask && !home.realTask.terminal
                text: home.hasTask && home.realTask.canRecover
                      ? I18n.t(home.isSend ? "移除中断记录" : "移除记录并清理未完成文件") : I18n.t("取消任务")
                onClicked: {
                    if (home.realTask.canRecover) { AppController.discardInterruptedTask(home.selectedTaskId); }
                    else { AppController.cancelTask(home.selectedTaskId); }
                }
            }
            DetailButton {
                text: I18n.t("连接详情")
                onClicked: {
                    var task = home.realTask;
                    if (!task) {
                        home.notifyRequested(I18n.t("暂无连接信息"));
                        return;
                    }
                    var paragraphs = [
                        I18n.t("对端：") + home.taskPeer + "\n本任务本地中继端口：" + (task.localRelayPort || "—")
                            + I18n.t("\nIP 不代表已验证的身份。")
                    ];
                    if (task.aggregateProgress) {
                        var verifyPercent = task.verifyPercent >= 0 ? task.verifyPercent.toFixed(2) + "%" : "—";
                        paragraphs.push(I18n.t("校验进度") + ": " + verifyPercent
                                        + (task.verifyStatus ? " · " + task.verifyStatus : ""));
                    }
                    if (task.resultSummary) { paragraphs.push(task.resultSummary); }
                    paragraphs.push(home.isSend
                        ? I18n.t("本端发送结束仅代表引擎结束，不能保证已取得对方保存确认。DLL圆环显示整批进度，校验时单独显示校验进度；旧进程后端保留当前文件语义。")
                        : I18n.t("文件由传输引擎校验。传输与保存完成前，请保持双方在线。"));
                    home.notifyRequested(paragraphs.join("\n\n"));
                }
            }
            DetailButton {
                visible: home.hasTask && (home.realTask.files || []).length > 0
                text: I18n.t("文件结果详情")
                onClicked: home.notifyRequested(home.realTask.files.map(function(file) { return file.path + "\n" + file.status + " · " + file.bytes_done + "/" + file.size + " bytes"; }).join("\n\n"))
            }
        }
    }
}
