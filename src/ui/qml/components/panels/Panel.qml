import QtQuick
import QtQuick.Layouts

// 面板容器：统一圆角、描边与内部留白；子项自动放入内部 ColumnLayout。
GlassSurface {
    id: panel
    property int padding: Theme.compactDesktop ? 12 : 22
    property bool fillContentHeight: false
    property alias spacing: panelBody.spacing
    default property alias contents: panelBody.data
    implicitHeight: panelBody.implicitHeight + padding * 2
    glassTint: Theme.panel
    radius: Theme.radiusPanel
    border.color: Theme.glassEdge

    ColumnLayout {
        id: panelBody
        x: panel.padding
        y: panel.padding
        width: panel.width - panel.padding * 2
        height: panel.fillContentHeight ? Math.max(0, panel.height - panel.padding * 2) : implicitHeight
        spacing: Theme.compactDesktop ? 10 : 16
    }
}
