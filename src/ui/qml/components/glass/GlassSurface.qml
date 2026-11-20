import QtQuick

// 共用玻璃材质：环境层模糊采样、边缘折射和随指针变化的反光。
// HoverHandler 不抓取按键或拖放；文字和控件始终在材质层上方绘制。
Rectangle {
    id: glass
    property color glassTint: Theme.panel
    property color accent: Theme.blue
    property real tintStrength: 0.58
    property real glassOpacity: 0.94
    property bool interactiveLight: true
    readonly property bool shaderActive: GraphicsInfo.api !== GraphicsInfo.Software
                                        && GraphicsInfo.api !== GraphicsInfo.Unknown
                                        && Theme.glassSource !== null
                                        && glassShader.status !== ShaderEffect.Error
    property point sourceOrigin: Qt.point(0, 0)
    property real lightX: lightHover.hovered ? lightHover.point.position.x : width * 0.24
    property real lightY: lightHover.hovered ? lightHover.point.position.y : -height * 0.18
    property real lightAmount: lightHover.hovered ? 1 : 0

    radius: Theme.radiusPanel
    color: shaderActive ? "transparent" : Qt.rgba(glassTint.r, glassTint.g, glassTint.b, 0.88)
    border.color: Theme.glassEdge
    border.width: shaderActive ? 0 : 1

    Behavior on lightX { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
    Behavior on lightY { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
    Behavior on lightAmount { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

    function updateSourceOrigin() {
        if (Theme.glassBackdrop) {
            sourceOrigin = glass.mapToItem(Theme.glassBackdrop, 0, 0);
        }
    }
    onXChanged: Qt.callLater(updateSourceOrigin)
    onYChanged: Qt.callLater(updateSourceOrigin)
    onWidthChanged: Qt.callLater(updateSourceOrigin)
    onHeightChanged: Qt.callLater(updateSourceOrigin)
    onVisibleChanged: Qt.callLater(updateSourceOrigin)
    Component.onCompleted: Qt.callLater(updateSourceOrigin)
    Connections {
        target: Theme
        function onGlassViewportRevisionChanged() { glass.updateSourceOrigin(); }
    }
    Connections {
        target: glass.parent
        function onXChanged() { Qt.callLater(glass.updateSourceOrigin); }
        function onYChanged() { Qt.callLater(glass.updateSourceOrigin); }
    }

    ShaderEffect {
        id: glassShader
        anchors.fill: parent
        visible: glass.shaderActive
        // 所有颜色以非预乘向量传入，最终只在输出时乘不透明度。
        property var source: Theme.glassSource
        property vector2d canvasSize: Qt.vector2d(Math.max(1, Theme.glassCanvasWidth), Math.max(1, Theme.glassCanvasHeight))
        property vector4d sourceBounds: Qt.vector4d(glass.sourceOrigin.x, glass.sourceOrigin.y, glass.width, glass.height)
        property vector4d shapeInfo: Qt.vector4d(glass.width, glass.height, glass.radius, glass.glassOpacity)
        property vector4d tintInfo: Qt.vector4d(glass.glassTint.r, glass.glassTint.g, glass.glassTint.b, glass.tintStrength)
        property vector4d accentInfo: Qt.vector4d(glass.accent.r, glass.accent.g, glass.accent.b, 1)
        property vector4d edgeInfo: Qt.vector4d(glass.border.color.r, glass.border.color.g, glass.border.color.b, glass.border.color.a)
        property vector4d lightInfo: Qt.vector4d(glass.lightX, glass.lightY, glass.lightAmount, 0)
        fragmentShader: "qrc:/ui/shaders/glass.frag.qsb"
    }

    HoverHandler {
        id: lightHover
        enabled: glass.interactiveLight && glass.shaderActive
        blocking: false
    }
}
