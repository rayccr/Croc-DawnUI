import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

// 首页：三个功能入口、收发分栏的真实任务列表，以及共用的任务详情卡。
// 全部数据来自 AppController 的任务模型，没有任何演示数据。
ColumnLayout {
    id: home
    property bool narrow: false
    property real viewportHeight: 0
    readonly property bool fitToViewport: viewportHeight > 0
    readonly property real detailHeight: Math.max(0, viewportHeight - entryBar.implicitHeight - taskColumns.implicitHeight - spacing * 2)
    property string selectedTaskId: ""
    property bool revealCode: AppController.settings.mask !== "默认遮挡"
    onSelectedTaskIdChanged: {
        revealCode = AppController.settings.mask !== "默认遮挡";
        codeTimer.stop();
        if (taskActionsPopup) { taskActionsPopup.close(); }
        if (recoverCodeField) { recoverCodeField.text = ""; }
        refreshSelectedTask();
    }
    onVisibleChanged: { if (!visible && taskActionsPopup) { taskActionsPopup.close(); } }
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
    onHasTaskChanged: { if (!hasTask && taskActionsPopup) { taskActionsPopup.close(); } }
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

    spacing: Theme.compactDesktop ? 8 : 20

    component DetailButton: AppButton {
        implicitHeight: home.fitToViewport ? 32 : 36
        padding: 8
    }

    GridLayout {
        id: entryBar
        Layout.fillWidth: true
        columns: !home.fitToViewport && home.narrow ? 1 : 3
        readonly property bool condensed: home.fitToViewport && home.width < 480
        columnSpacing: Theme.compactDesktop ? 8 : 12
        rowSpacing: Theme.compactDesktop ? 6 : 10

        EntryCard {
            text: I18n.t("发送文件")
            shortText: I18n.t("发送")
            condensed: entryBar.condensed
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
            shortText: I18n.t("接收")
            condensed: entryBar.condensed
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
            condensed: entryBar.condensed
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
        id: taskColumns
        Layout.fillWidth: true
        columns: !home.fitToViewport && home.narrow ? 1 : 2
        columnSpacing: Theme.compactDesktop ? 8 : 12
        rowSpacing: Theme.compactDesktop ? 8 : 12
        readonly property bool bothListsEmpty: sendColumn.taskCount === 0 && receiveColumn.taskCount === 0
        readonly property bool fitsTwoRows: home.viewportHeight >= (home.width < 640 ? 452 : 390)
        readonly property int visibleRows: home.fitToViewport && (bothListsEmpty || !fitsTwoRows) ? 1 : 2

        TaskColumn {
            id: sendColumn
            direction: "send"
            bothListsEmpty: taskColumns.bothListsEmpty
            visibleRows: taskColumns.visibleRows
            taskModel: AppController.sendTasks
            selectedTaskId: home.selectedTaskId
            Layout.fillWidth: true
            onRealTaskSelected: function(taskId) {
                home.selectedTaskId = taskId;
            }
        }
        TaskColumn {
            id: receiveColumn
            direction: "receive"
            bothListsEmpty: taskColumns.bothListsEmpty
            visibleRows: taskColumns.visibleRows
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
        Layout.fillHeight: home.fitToViewport
        Layout.minimumHeight: home.fitToViewport ? 0 : -1
        Layout.preferredHeight: home.fitToViewport ? home.detailHeight : -1
        fillContentHeight: home.fitToViewport
        clip: home.fitToViewport
        padding: home.fitToViewport ? 10 : 16
        spacing: home.fitToViewport ? 6 : 8

        readonly property real contentWidth: Math.max(0, width - padding * 2)
        readonly property bool actionsFit: !home.fitToViewport || padding * 2 + detailHeader.implicitHeight
                                          + taskSummary.implicitHeight + spacing * 2 + taskActions.implicitHeight <= home.detailHeight

        RowLayout {
            id: detailHeader
            Layout.fillWidth: true
            BodyText {
                text: I18n.t("任务详情")
                font.pixelSize: Theme.compactDesktop ? 15 : 18
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            Rectangle {
                visible: !home.fitToViewport || home.hasTask
                Layout.preferredWidth: Math.min(stateLabel.implicitWidth + 18, (home.width - taskDetail.padding * 2) * 0.55)
                Layout.preferredHeight: home.fitToViewport ? 24 : 26
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
            Layout.fillHeight: home.fitToViewport
            horizontalAlignment: home.fitToViewport ? Text.AlignHCenter : Text.AlignLeft
            verticalAlignment: home.fitToViewport ? Text.AlignVCenter : Text.AlignTop
        }

        GridLayout {
            id: taskSummary
            visible: home.hasTask
            Layout.fillWidth: true
            columns: 2
            columnSpacing: home.narrow || Theme.compactDesktop ? 12 : 20
            rowSpacing: home.fitToViewport ? 4 : 8

            ProgressRing {
                Layout.rowSpan: 2
                Layout.preferredWidth: home.fitToViewport
                                       ? Math.min(home.narrow ? 88 : 104, Math.max(76, Math.round(home.detailHeight * 0.39)))
                                       : (home.narrow ? 88 : 104)
                Layout.preferredHeight: Layout.preferredWidth
                Layout.alignment: Qt.AlignCenter
                progress: home.taskPercent
                accent: home.isSend ? Theme.blue : Theme.green
                caption: home.hasTask && home.realTask.verifying ? I18n.t(home.fitToViewport ? "校验中" : "校验进度")
                         : home.hasTask && home.realTask.aggregateProgress ? I18n.t(home.fitToViewport ? "整批" : "整批传输进度")
                         : I18n.t(home.fitToViewport ? "当前文件" : "当前文件进度")
            }

            RowLayout {
                Layout.row: 0
                Layout.column: 1
                spacing: 8
                Layout.fillWidth: true
                Rectangle {
                    Layout.preferredWidth: home.fitToViewport ? 24 : 32
                    Layout.preferredHeight: Layout.preferredWidth
                    radius: Theme.radiusControl
                    color: Theme.sendSurface
                    UiIcon {
                        anchors.centerIn: parent
                        width: home.fitToViewport ? 16 : 22
                        height: width
                        symbol: "file"
                        tone: "send"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    BodyText {
                        id: selectedTaskName
                        text: home.taskName
                        font.pixelSize: home.fitToViewport ? 14 : 16
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                        HoverHandler { id: taskNameHover }
                        Controls.ToolTip.visible: selectedTaskName.truncated && taskNameHover.hovered
                        Controls.ToolTip.text: selectedTaskName.text
                    }
                    Hint {
                        visible: !Theme.compactDesktop
                        text: I18n.t("真实任务")
                    }
                }
            }

            GridLayout {
                Layout.row: 1
                Layout.column: 1
                Layout.fillWidth: true
                columns: home.fitToViewport && taskDetail.width >= 560 ? 4 : 2
                columnSpacing: home.narrow ? 8 : 16
                rowSpacing: home.fitToViewport ? 3 : 6
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

        Item {
            id: inlineTaskActions
            visible: home.hasTask
            Layout.fillWidth: true
            Layout.fillHeight: home.fitToViewport
            implicitHeight: taskDetail.actionsFit ? taskActions.implicitHeight : openTaskActions.implicitHeight
            clip: home.fitToViewport

            DetailButton {
                id: openTaskActions
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                visible: !taskDetail.actionsFit
                text: I18n.t(home.taskError === "" ? "任务操作" : "查看错误与操作")
                onClicked: taskActionsPopup.open()
            }
        }

        Controls.Popup {
            id: taskActionsPopup
            parent: Controls.Overlay.overlay
            anchors.centerIn: parent
            width: taskDetail.width
            height: Math.min(popupHeading.implicitHeight + popupContent.spacing + taskActions.implicitHeight + padding * 2,
                             parent ? Math.max(0, parent.height - 32) : 0)
            padding: taskDetail.padding
            modal: true
            focus: true
            background: Rectangle {
                color: Theme.panel
                radius: Theme.radiusPanel
                border.color: Theme.glassEdge
            }
            contentItem: ColumnLayout {
                id: popupContent
                spacing: 8

                RowLayout {
                    id: popupHeading
                    Layout.fillWidth: true
                    BodyText {
                        text: I18n.t("任务操作")
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                    DetailButton {
                        text: I18n.t("返回详情")
                        onClicked: taskActionsPopup.close()
                    }
                }
                Controls.ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth
                    contentHeight: popupTaskActions.height
                    Item {
                        id: popupTaskActions
                        width: taskDetail.contentWidth
                        height: taskActions.implicitHeight
                    }
                }
            }
        }

        // 同一组控件在卡片和弹层之间移动，输入内容及任务操作保持一致。
        GridLayout {
            id: taskActions
            parent: taskActionsPopup.visible ? popupTaskActions : inlineTaskActions
            anchors.bottom: parent.bottom
            width: taskDetail.contentWidth
            height: implicitHeight
            enabled: taskDetail.actionsFit || taskActionsPopup.visible
            opacity: enabled ? 1 : 0
            columns: shareInRow ? 2 : 1
            columnSpacing: 8
            rowSpacing: taskDetail.spacing
            readonly property bool shareInRow: home.fitToViewport && home.hasTask && home.realTask.shareReady
                                               && !home.realTask.canRecover && home.realTask.retrySeconds < 0
                                               && taskDetail.contentWidth >= 620

            GridLayout {
                visible: home.taskError !== ""
                Layout.fillWidth: true
                Layout.columnSpan: taskActions.columns
                columns: home.fitToViewport ? 2 : 1
                rowSpacing: taskDetail.spacing
                Hint {
                    id: taskErrorPreview
                    text: home.taskError
                    color: "#ff9c8a"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    maximumLineCount: home.fitToViewport ? 1 : 2
                    elide: Text.ElideRight
                }
                DetailButton {
                    visible: taskErrorPreview.truncated
                    text: I18n.t("查看完整错误")
                    implicitHeight: 32
                    padding: 6
                    onClicked: home.notifyRequested(home.taskError)
                }
            }
            GridLayout {
                id: shareActions
                readonly property real inlineWidth: revealAction.implicitWidth + copyAction.implicitWidth + columnSpacing * 2 + 120
                readonly property real sideBySideWidth: Math.max(inlineWidth, Math.floor((taskDetail.contentWidth - taskActions.columnSpacing) / 2))
                readonly property bool stackCode: !taskActions.shareInRow && taskDetail.contentWidth < inlineWidth
                visible: home.hasTask && home.realTask.shareReady
                Layout.fillWidth: true
                Layout.minimumWidth: taskActions.shareInRow ? inlineWidth : 0
                Layout.preferredWidth: taskActions.shareInRow ? sideBySideWidth : -1
                Layout.maximumWidth: taskActions.shareInRow ? sideBySideWidth : Infinity
                columns: stackCode ? 2 : 3
                columnSpacing: 8
                rowSpacing: 6
                FieldBox {
                    text: home.hasTask ? home.realTask.code : ""
                    echoMode: home.revealCode ? TextInput.Normal : TextInput.Password
                    readOnly: true
                    implicitHeight: home.fitToViewport ? 32 : 36
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
            Rule { visible: home.hasTask && !home.fitToViewport }

            Hint {
                visible: home.hasTask && home.realTask.retrySeconds >= 0
                text: "自动重连剩余 " + (home.hasTask ? home.realTask.retrySeconds : 0) + " 秒 · 原会话可能已结束，部分文件保留"
                Layout.fillWidth: true
                Layout.columnSpan: taskActions.columns
            }
            GridLayout {
                visible: home.hasTask && home.realTask.canRecover
                Layout.fillWidth: true
                Layout.columnSpan: taskActions.columns
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
                Layout.preferredWidth: taskActions.shareInRow ? 1 : -1
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
}
