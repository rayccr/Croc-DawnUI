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
    property bool condensed: false
    property string shortText: text
    implicitHeight: Theme.compactDesktop ? 44 : (narrow ? 94 : 146)
    hoverEnabled: true
    Accessible.name: text
    Accessible.description: description
    Controls.ToolTip.visible: Theme.compactDesktop && (hovered || visualFocus)
    Controls.ToolTip.text: description
    Controls.ToolTip.delay: 400

    background: GlassSurface {
        radius: Theme.compactDesktop ? Theme.radiusControl : Theme.radiusPanel
        glassTint: entry.surface
        accent: entry.accent
        tintStrength: 0.68
        border.color: entry.hovered || entry.activeFocus ? entry.accent : Theme.glassEdge
    }

    contentItem: Item {
        UiIcon {
            id: icon
            x: entry.condensed ? 8 : (Theme.compactDesktop ? 12 : 22)
            y: Theme.compactDesktop ? (parent.height - height) / 2 : (entry.narrow ? 30 : 22)
            width: entry.condensed ? 16 : (Theme.compactDesktop ? 20 : 27)
            height: width
            symbol: entry.symbol
            tone: entry.tone
        }
        BodyText {
            x: entry.condensed ? 30 : (Theme.compactDesktop ? 40 : (entry.narrow ? 65 : 22))
            y: Theme.compactDesktop ? (parent.height - height) / 2 : (entry.narrow ? 20 : 69)
            width: parent.width - x - (entry.condensed ? 8 : (Theme.compactDesktop ? 32 : 44))
            text: entry.condensed ? entry.shortText : entry.text
            font.pixelSize: Theme.compactDesktop ? 14 : 18
            font.weight: Font.DemiBold
            wrapMode: Theme.compactDesktop ? Text.NoWrap : Text.Wrap
            elide: Theme.compactDesktop ? Text.ElideRight : Text.ElideNone
        }
        Hint {
            visible: !Theme.compactDesktop
            x: entry.narrow ? 65 : 22
            y: entry.narrow ? 49 : 104
            width: parent.width - x - 22
            text: entry.description
            font.pixelSize: 13
        }
        UiIcon {
            visible: !entry.condensed
            x: parent.width - (Theme.compactDesktop ? 24 : 38)
            y: Theme.compactDesktop ? (parent.height - height) / 2 : (entry.narrow ? 24 : 73)
            width: Theme.compactDesktop ? 14 : 16
            height: width
            symbol: "arrow"
            tone: entry.tone
        }
    }
}
