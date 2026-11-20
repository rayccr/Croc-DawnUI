import QtQuick
import QtQuick.Controls.Basic as Controls

// 首页功能入口卡片：只负责外观与点击，跳转由使用方决定。
Controls.AbstractButton {
    id: entry
    property string symbol
    property string description
    property color accent
    property color surface
    property string tone
    property bool narrow: false
    implicitHeight: narrow ? 94 : 146
    hoverEnabled: true

    background: GlassSurface {
        radius: Theme.radiusPanel
        glassTint: entry.surface
        accent: entry.accent
        tintStrength: 0.68
        border.color: entry.hovered || entry.activeFocus ? entry.accent : Theme.glassEdge
    }

    contentItem: Item {
        UiIcon {
            id: icon
            x: 22
            y: entry.narrow ? 30 : 22
            width: 27
            height: 27
            symbol: entry.symbol
            tone: entry.tone
        }
        BodyText {
            x: entry.narrow ? 65 : 22
            y: entry.narrow ? 20 : 69
            width: parent.width - x - 44
            text: entry.text
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }
        Hint {
            x: entry.narrow ? 65 : 22
            y: entry.narrow ? 49 : 104
            width: parent.width - x - 22
            text: entry.description
            font.pixelSize: 13
        }
        UiIcon {
            x: parent.width - 38
            y: entry.narrow ? 24 : 73
            width: 16
            height: 16
            symbol: "arrow"
            tone: entry.tone
        }
    }
}
