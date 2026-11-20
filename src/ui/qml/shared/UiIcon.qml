import QtQuick

// SVG 图标来自 HTML 参考文件，独立资源避免依赖图标字体。
Image {
    property string symbol: "file"
    property string tone: "muted"
    source: symbol === "" ? "" : "qrc:/ui/icons/" + symbol + "-" + tone + ".svg"
    sourceSize.width: width * Screen.devicePixelRatio
    sourceSize.height: height * Screen.devicePixelRatio
    width: 22
    height: 22
    fillMode: Image.PreserveAspectFit
}
