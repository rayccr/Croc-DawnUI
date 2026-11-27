pragma Singleton

import QtQuick

// 主题常量：颜色与尺寸的唯一来源，页面和组件都从这里取值，避免各处散落色值。
QtObject {
    readonly property bool compactDesktop: Qt.platform.os !== "android" && Qt.platform.os !== "ios"
    readonly property color background: "#0d1014"
    readonly property color panel: "#15191e"
    readonly property color field: "#111419"
    readonly property color hover: "#20252c"
    readonly property color pressed: "#34383e"
    readonly property color line: "#34383e"
    readonly property color rule: "#2a2e33"
    readonly property color text: "#f1f2f3"
    readonly property color muted: "#a8afb8"
    readonly property color placeholder: "#727b86"
    readonly property color blue: "#76adff"
    readonly property color green: "#67d3ab"
    readonly property color purple: "#bca2ff"
    readonly property color onAccent: "#101820"
    readonly property color sendSurface: "#141e2a"
    readonly property color receiveSurface: "#14231f"
    readonly property color historySurface: "#211b2c"
    readonly property int radiusPanel: compactDesktop ? 12 : 18
    readonly property int radiusControl: 10

    // 玻璃材质参数与唯一环境纹理；无持续动画，静止时无需反复重绘。
    readonly property color glassEdge: "#438c9db8"
    property var glassSource: null
    property var glassBackdrop: null
    property real glassCanvasWidth: 1
    property real glassCanvasHeight: 1
    property int glassViewportRevision: 0
}
