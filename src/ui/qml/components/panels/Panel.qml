import QtQuick
import QtQuick.Layouts

// 面板容器：统一圆角、描边与内部留白；子项自动放入内部 ColumnLayout。
GlassSurface {
    id: panel
    property int padding: 22
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
        spacing: 16
    }
}
