import QtQuick

// 正文文本：统一颜色、换行与渲染方式。
Text {
    color: Theme.text
    font.pixelSize: 14
    textFormat: Text.PlainText
    wrapMode: Text.Wrap
    renderType: Text.NativeRendering
}
