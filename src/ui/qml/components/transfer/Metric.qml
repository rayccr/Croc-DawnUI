import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

// 详情指标：小标题 + 数值。
ColumnLayout {
    property string caption
    property string value
    spacing: 3
    Layout.minimumWidth: 0
    Layout.preferredWidth: 1

    Hint {
        text: parent.caption
        Layout.fillWidth: true
        maximumLineCount: 2
        elide: Text.ElideRight
    }
    BodyText {
        text: parent.value
        font.weight: Font.Medium
        Layout.fillWidth: true
        wrapMode: Text.NoWrap
        elide: Text.ElideRight
        HoverHandler { id: valueHover }
        Controls.ToolTip.visible: truncated && valueHover.hovered
        Controls.ToolTip.text: text
    }
}
