import QtQuick
import QtQuick.Controls.Basic as Controls

// 文本输入框：统一高度、边框与聚焦颜色。
Controls.TextField {
    color: Theme.text
    placeholderTextColor: Theme.placeholder
    font.pixelSize: 14
    implicitHeight: Theme.compactDesktop ? 38 : 46
    leftPadding: 12
    rightPadding: 12
    selectByMouse: true

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.field
        border.color: parent.activeFocus ? Theme.blue : Theme.line
    }
}
