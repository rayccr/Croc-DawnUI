import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

// 首页任务列：列表窗口固定为两条标准任务行高度，更多任务在本列内滚动。
// 数据只来自 AppController 的过滤模型，没有任何演示数据。
// 紧凑首页高度不足时可用一条完整任务行。
Panel {
    id: taskColumn
    property string direction: "send"
    property var taskModel: null
    property string selectedTaskId: ""
    property bool bothListsEmpty: false
    property int visibleRows: 2
    readonly property int taskCount: realTasks.count
    readonly property int taskRowHeight: Theme.compactDesktop ? 54 : 76

    signal realTaskSelected(string taskId)

    padding: Theme.compactDesktop ? 10 : 16
    spacing: Theme.compactDesktop ? 8 : 16
    accent: direction === "send" ? Theme.blue : Theme.green

    RowLayout {
        Layout.fillWidth: true
        BodyText {
            text: taskColumn.direction === "send" ? I18n.t("发送任务") : I18n.t("接收任务")
            font.weight: Font.DemiBold
            Layout.fillWidth: true
            wrapMode: Theme.compactDesktop ? Text.NoWrap : Text.Wrap
            elide: Theme.compactDesktop ? Text.ElideRight : Text.ElideNone
        }
        Hint {
            text: realTasks.count + I18n.t(" 项")
        }
    }

    ListView {
        id: realTasks
        readonly property int rowsHeight: taskColumn.taskRowHeight * taskColumn.visibleRows + spacing * (taskColumn.visibleRows - 1)
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.compactDesktop && taskColumn.bothListsEmpty ? Math.min(72, rowsHeight) : rowsHeight
        clip: true
        spacing: Theme.compactDesktop ? 6 : 8
        model: taskColumn.taskModel
        onContentYChanged: ++Theme.glassViewportRevision
        boundsBehavior: Flickable.StopAtBounds
        Controls.ScrollBar.vertical: Controls.ScrollBar { policy: Controls.ScrollBar.AsNeeded }

        delegate: Controls.AbstractButton {
            id: realRow
            required property int index
            required property string taskId
            required property string name
            required property string stageText
            required property real percent
            required property string speed
            width: realTasks.width
            height: taskColumn.taskRowHeight
            padding: Theme.compactDesktop ? 7 : 10
            rightPadding: Theme.compactDesktop && realTasks.count > taskColumn.visibleRows ? 13 : padding
            hoverEnabled: true
            Accessible.name: taskTitle.text
            Accessible.description: taskState.text
            Controls.ToolTip.visible: Theme.compactDesktop && (taskTitle.truncated || taskState.truncated) && (hovered || visualFocus)
            Controls.ToolTip.text: taskTitle.text + "\n" + taskState.text
            Controls.ToolTip.delay: 400

            background: GlassSurface {
                radius: Theme.radiusControl
                glassTint: Theme.hover
                accent: taskColumn.accent
                opacity: (taskColumn.selectedTaskId !== "" && taskColumn.selectedTaskId === realRow.taskId) || realRow.hovered || realRow.visualFocus ? 1 : 0.50
                border.color: taskColumn.selectedTaskId === realRow.taskId || realRow.visualFocus ? taskColumn.accent : Theme.glassEdge
            }

            contentItem: ColumnLayout {
                spacing: Theme.compactDesktop ? 3 : 5
                BodyText {
                    id: taskTitle
                    text: realRow.name !== "" ? realRow.name : I18n.t("等待文件清单")
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                    font.weight: Font.Medium
                }
                RowLayout {
                    Layout.fillWidth: true
                    Hint {
                        visible: !Theme.compactDesktop || realRow.speed !== ""
                        text: realRow.speed !== "" ? realRow.speed : "—"
                        font.pixelSize: 11
                        Layout.fillWidth: !Theme.compactDesktop
                    }
                    Hint {
                        id: taskState
                        text: I18n.t(realRow.stageText) + (realRow.percent < 0 ? "" : " · " + realRow.percent.toFixed(2) + "%")
                        font.pixelSize: 11
                        Layout.fillWidth: Theme.compactDesktop
                        horizontalAlignment: Theme.compactDesktop && realRow.speed !== "" ? Text.AlignRight : Text.AlignLeft
                        wrapMode: Theme.compactDesktop ? Text.NoWrap : Text.Wrap
                        elide: Theme.compactDesktop ? Text.ElideRight : Text.ElideNone
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    height: 3
                    radius: 2
                    color: Theme.line
                    opacity: realRow.percent < 0 ? 0 : 1
                    Rectangle {
                        height: 3
                        radius: 2
                        width: parent.width * Math.max(0, realRow.percent) / 100
                        color: taskColumn.direction === "send" ? Theme.blue : Theme.green
                    }
                }
            }

            onClicked: taskColumn.realTaskSelected(realRow.taskId)
        }

        Hint {
            anchors.centerIn: parent
            text: I18n.t("暂无任务")
            visible: realTasks.count === 0
        }
    }
}
