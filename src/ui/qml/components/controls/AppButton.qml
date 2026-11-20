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
    implicitHeight: 44
    implicitWidth: Math.max(92, labelRow.implicitWidth + 28)
    padding: 12
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45

    background: Rectangle {
        radius: Theme.radiusControl
        color: button.down ? Theme.pressed : button.hovered && button.tint === "transparent" ? Theme.hover : button.tint
        border.color: button.activeFocus ? Theme.blue : Theme.line
    }

    contentItem: RowLayout {
        id: labelRow
        spacing: 8
        UiIcon {
            visible: button.symbol !== ""
            symbol: button.symbol
            tone: button.tone
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
        }
        BodyText {
            text: button.text
            color: button.labelColor
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.NoWrap
        }
    }
}
