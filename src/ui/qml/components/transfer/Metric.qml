import QtQuick
import QtQuick.Layouts

// 详情指标：小标题 + 数值。
ColumnLayout {
    property string caption
    property string value
    spacing: 5

    Hint {
        text: parent.caption
        Layout.fillWidth: true
    }
    BodyText {
        text: parent.value
        font.weight: Font.Medium
        Layout.fillWidth: true
    }
}
