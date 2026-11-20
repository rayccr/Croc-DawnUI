import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

// 首页任务列：列表窗口固定为两条标准任务行高度，更多任务在本列内滚动。
// 数据只来自 AppController 的过滤模型，没有任何演示数据。
Panel {
    id: taskColumn
    property string direction: "send"
    property var taskModel: null
    property string selectedTaskId: ""

    signal realTaskSelected(string taskId)

    padding: 16
    accent: direction === "send" ? Theme.blue : Theme.green

    RowLayout {
        Layout.fillWidth: true
        BodyText {
            text: taskColumn.direction === "send" ? I18n.t("发送任务") : I18n.t("接收任务")
            font.weight: Font.DemiBold
            Layout.fillWidth: true
        }
        Hint {
            text: realTasks.count + I18n.t(" 项")
        }
    }

    ListView {
        id: realTasks
        Layout.fillWidth: true
        Layout.preferredHeight: 160
        clip: true
        spacing: 8
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
            height: 76
            hoverEnabled: true

            background: GlassSurface {
                radius: Theme.radiusControl
                glassTint: Theme.hover
                accent: taskColumn.accent
                opacity: (taskColumn.selectedTaskId !== "" && taskColumn.selectedTaskId === realRow.taskId) || realRow.hovered ? 1 : 0.50
                border.color: taskColumn.selectedTaskId === realRow.taskId ? taskColumn.accent : Theme.glassEdge
            }

            contentItem: ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 5
                BodyText {
                    text: realRow.name !== "" ? realRow.name : I18n.t("等待文件清单")
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                    font.weight: Font.Medium
                }
                RowLayout {
                    Layout.fillWidth: true
                    Hint { text: realRow.speed !== "" ? realRow.speed : "—"; font.pixelSize: 11; Layout.fillWidth: true }
                    Hint {
                        text: I18n.t(realRow.stageText) + (realRow.percent < 0 ? "" : " · " + realRow.percent.toFixed(2) + "%")
                        font.pixelSize: 11
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
