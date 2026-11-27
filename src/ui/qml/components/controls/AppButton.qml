import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

// 通用按钮：可选图标、色调与强调底色；禁用时整体变淡。
Controls.Button {
    id: button
    property string symbol: ""
    property string tone: "muted"
    property color tint: "transparent"
    property color labelColor: Theme.text
    property string toolTipText: ""
    implicitHeight: Theme.compactDesktop ? 36 : 44
    implicitWidth: Math.max(Theme.compactDesktop ? 76 : 92, labelRow.implicitWidth + (Theme.compactDesktop ? 20 : 28))
    padding: Theme.compactDesktop ? 8 : 12
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45
    Controls.ToolTip.visible: toolTipText !== "" && (hovered || visualFocus)
    Controls.ToolTip.text: toolTipText
    Controls.ToolTip.delay: 400

    background: Rectangle {
        radius: Theme.radiusControl
        color: button.down ? Theme.pressed : button.hovered && button.tint === "transparent" ? Theme.hover : button.tint
        border.color: button.activeFocus ? Theme.blue : Theme.line
    }

    contentItem: RowLayout {
        id: labelRow
        spacing: Theme.compactDesktop ? 6 : 8
        UiIcon {
            visible: button.symbol !== ""
            symbol: button.symbol
            tone: button.tone
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            Layout.alignment: button.text === "" ? Qt.AlignHCenter : Qt.AlignVCenter
        }
        BodyText {
            visible: button.text !== "" || !Theme.compactDesktop
            text: button.text
            color: button.labelColor
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.NoWrap
        }
    }
}
