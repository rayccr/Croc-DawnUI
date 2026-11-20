import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

// 应用主窗口：品牌顶栏、页面宿主、全局提示。
// 页面内容分别在 HomePage / SendPage / ReceivePage / HistoryPage / SettingsPage，
// 公共外观在 Theme 与各组件文件中，数据只来自 AppController 的真实任务。
Controls.ApplicationWindow {
    id: window
    width: 1120
    height: 760
    minimumWidth: 360
    minimumHeight: 480
    visible: false  // C++ 根据屏幕可用区域确定尺寸后再显示，避免首次弹出超高窗口。
    title: "Croc-DawnUI"
    color: Theme.background
    font.family: "Microsoft YaHei UI"
    font.pixelSize: 14

    background: Item {
        GlassBackdrop { id: glassBackdrop; anchors.fill: parent }
        ShaderEffectSource {
            id: glassTexture
            visible: false
            sourceItem: glassBackdrop
            live: true
            smooth: true
            textureSize: Qt.size(Math.max(1, Math.ceil(width)), Math.max(1, Math.ceil(height)))
            anchors.fill: parent
        }
        Component.onCompleted: {
            Theme.glassBackdrop = glassBackdrop;
            Theme.glassSource = glassTexture;
            Theme.glassCanvasWidth = Qt.binding(function() { return glassBackdrop.width; });
            Theme.glassCanvasHeight = Qt.binding(function() { return glassBackdrop.height; });
        }
    }
    onWidthChanged: Qt.callLater(function() { ++Theme.glassViewportRevision; })
    onHeightChanged: Qt.callLater(function() { ++Theme.glassViewportRevision; })
    onCurrentPageChanged: Qt.callLater(function() { ++Theme.glassViewportRevision; })

    property string currentPage: "home"
    property bool exitConfirmed: false
    onClosing: function(close) {
        if (AppController.hasActiveTasks && !exitConfirmed) { close.accepted = false; exitPopup.open(); }
    }
    property string lastEvent: AppController.lastEventText
    readonly property bool narrow: width < 640
    readonly property bool isSubPage: currentPage === "send" || currentPage === "receive" || currentPage === "settings"

    function navigate(page) {
        currentPage = page;
        contentScroll.contentItem.contentY = 0;
    }

    function notify(message) {
        messageText.text = I18n.t(message);
        messagePopup.open();
    }

    // 引擎事件通过控制器属性传来（信号在 QML 侧不可用），属性变化时再弹提示
    onLastEventChanged: {
        if (lastEvent !== "") {
            notify(lastEvent);
        }
    }

    header: Rectangle {
        height: 72
        color: "#c00b0e13"

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.rule
        }

        RowLayout {
            width: Math.min(parent.width - 32, 1040)
            height: parent.height
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 10

            Controls.AbstractButton {
                implicitWidth: brandRow.implicitWidth
                implicitHeight: 44
                onClicked: window.navigate("home")
                contentItem: RowLayout {
                    id: brandRow
                    spacing: 10
                    Rectangle {
                        width: 40
                        height: 40
                        radius: 12
                        color: Theme.sendSurface
                        UiIcon {
                            anchors.centerIn: parent
                            source: "qrc:/ui/icons/dawn-logo.svg"
                            width: 36
                            height: 36
                        }
                    }
                    BodyText {
                        text: "Croc-DawnUI"
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Hint {
                visible: window.width > 700
                text: AppController.engineStatus
            }

            AppButton {
                text: window.narrow ? "" : I18n.t("设置")
                implicitWidth: window.narrow ? 44 : 92
                symbol: "settings"
                onClicked: window.navigate("settings")
            }
        }
    }

    Controls.ScrollView {
        id: contentScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        Connections {
            target: contentScroll.contentItem
            function onContentYChanged() { ++Theme.glassViewportRevision; }
        }

        ColumnLayout {
            id: pageBody
            width: Math.min(contentScroll.availableWidth - 32, window.isSubPage ? 760 : 1040)
            x: (contentScroll.availableWidth - width) / 2
            spacing: 20
            onHeightChanged: Qt.callLater(function() { ++Theme.glassViewportRevision; })

            Item { Layout.preferredHeight: 8 }

            AppButton {
                visible: window.currentPage !== "home"
                text: I18n.t("返回首页")
                symbol: "back"
                onClicked: window.navigate("home")
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                Hint {
                    text: ({
                        home: "CROC-DAWNUI / FILE TRANSFER",
                        send: "SEND / 01",
                        receive: "RECEIVE / 02",
                        history: "HISTORY / 03",
                        settings: "SETTINGS / 04"
                    })[window.currentPage]
                    font.pixelSize: 11
                    font.letterSpacing: 1.8
                }
                BodyText {
                    text: ({
                        home: I18n.t("让文件传输更简单"),
                        send: I18n.t("发送文件"),
                        receive: I18n.t("接收文件"),
                        history: I18n.t("传输历史"),
                        settings: I18n.t("设置")
                    })[window.currentPage]
                    font.pixelSize: window.narrow ? 24 : 28
                    font.weight: Font.Bold
                    Layout.fillWidth: true
                }
                Hint {
                    text: ({
                        home: I18n.t("发送、接收，一切恰到好处。"),
                        send: I18n.t("选好文件，再把口令交给对方。"),
                        receive: I18n.t("一个口令，让文件来到你身边。"),
                        history: I18n.t("每一次传输，都有迹可循。"),
                        settings: I18n.t("更改仅影响新任务。")
                    })[window.currentPage]
                    font.pixelSize: 14
                    Layout.fillWidth: true
                }
            }

            HomePage {
                visible: window.currentPage === "home"
                Layout.fillWidth: true
                narrow: window.narrow
                onNavigateTo: function(page) { window.navigate(page) }
                onNotifyRequested: function(text) { window.notify(text) }
            }

            SendPage {
                visible: window.currentPage === "send"
                Layout.fillWidth: true
                narrow: window.narrow
                onTaskStarted: function(taskId) {
                    window.notify(I18n.t("发送任务已创建，正在准备。口令生成后请核对对应任务再分享。"));
                }
                onNotifyRequested: function(text) { window.notify(text) }
            }

            ReceivePage {
                visible: window.currentPage === "receive"
                Layout.fillWidth: true
                narrow: window.narrow
                onTaskStarted: function(taskId) {
                    window.navigate("home");
                    window.notify(I18n.t("接收任务已启动，等待对方在线"));
                }
            }

            HistoryPage {
                visible: window.currentPage === "history"
                Layout.fillWidth: true
                narrow: window.narrow
            }

            SettingsPage {
                visible: window.currentPage === "settings"
                Layout.fillWidth: true
                narrow: window.narrow
                onNotifyRequested: function(text) { window.notify(text) }
                onNavigateRequested: function(page) { window.navigate(page) }
            }

            RowLayout {
                Layout.fillWidth: true
                Hint {
                    text: I18n.t("Croc-DawnUI 安全端到端文件传输")
                    Layout.fillWidth: true
                    font.pixelSize: 11
                }
            }

            Item { Layout.preferredHeight: 12 }
        }
    }

    Controls.Popup {
        id: exitPopup
        anchors.centerIn: parent
        width: Math.min(440, window.width - 40)
        modal: true; padding: 20
        background: GlassSurface { glassTint: Theme.panel; tintStrength: 0.78; glassOpacity: 0.98; radius: 12 }
        contentItem: ColumnLayout {
            spacing: 16
            BodyText { text: I18n.t("仍有活动任务"); font.weight: Font.DemiBold }
            Hint { text: I18n.t("退出会中断本机任务，对方可能需要重新连接或重新获取口令。已写入的部分文件会保留；此版本尚未实现托盘和自动恢复。"); Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                AppButton { text: I18n.t("继续传输"); onClicked: exitPopup.close() }
                Item { Layout.fillWidth: true }
                AppButton { text: I18n.t("中断并退出"); onClicked: { window.exitConfirmed = true; Qt.quit(); } }
            }
        }
    }

    // 接收确认 / 续传确认：必须由用户明确选择，不让引擎的默认值代替决定
    Controls.Popup {
        id: promptPopup
        anchors.centerIn: parent
        width: Math.min(460, window.width - 40)
        padding: 20
        modal: true
        visible: AppController.promptVisible
        closePolicy: Controls.Popup.NoAutoClose

        background: GlassSurface {
            glassTint: Theme.panel
            tintStrength: 0.78
            glassOpacity: 0.98
            radius: 12
        }

        contentItem: ColumnLayout {
            spacing: 16
            BodyText {
                text: I18n.t(AppController.promptTitle)
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
            Hint {
                text: AppController.promptBody
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppButton {
                    text: AppController.promptKind === "overwrite" ? "跳过" : I18n.t("拒绝")
                    property string pressedPromptId: ""
                    onPressed: pressedPromptId = AppController.promptId
                    onClicked: AppController.promptKind === "overwrite" ? AppController.answerPromptDecisionFor(pressedPromptId, 3) : AppController.answerPromptFor(pressedPromptId, false)
                }
                AppButton {
                    text: AppController.promptKind === "overwrite" || AppController.promptKind === "folder_overwrite" ? "覆盖" : I18n.t("接受")
                    property string pressedPromptId: ""
                    onPressed: pressedPromptId = AppController.promptId
                    tint: Theme.blue
                    labelColor: Theme.onAccent
                    onClicked: AppController.answerPromptFor(pressedPromptId, true)
                }
                AppButton {
                    visible: AppController.promptCanResume
                    text: "续传"
                    property string pressedPromptId: ""
                    onPressed: pressedPromptId = AppController.promptId
                    onClicked: AppController.answerPromptDecisionFor(pressedPromptId, 4)
                }
            }
        }
    }

    Controls.Popup {
        id: messagePopup
        anchors.centerIn: parent
        width: Math.min(420, window.width - 40)
        padding: 20
        modal: true
        closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside

        background: GlassSurface {
            glassTint: Theme.hover
            tintStrength: 0.78
            glassOpacity: 0.98
            radius: 12
        }

        contentItem: ColumnLayout {
            spacing: 16
            BodyText {
                id: messageText
                Layout.fillWidth: true
            }
            AppButton {
                text: I18n.t("知道了")
                Layout.alignment: Qt.AlignRight
                onClicked: messagePopup.close()
            }
        }
    }
}
