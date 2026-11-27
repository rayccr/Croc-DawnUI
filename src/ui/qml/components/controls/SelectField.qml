import QtQuick
import QtQuick.Controls.Basic as Controls
import QtQuick.Layouts

// 下拉选择控件：在 Basic 风格 ComboBox 上重绘输入区、箭头与弹出列表。
// 默认的 Basic 弹层是白底默认行高，和深色界面不匹配，所以整块重绘。
// 选项来自 options（字符串数组），选中结果通过 currentIndex / currentText 读取。
Controls.ComboBox {
    id: select
    property var options: []
    implicitHeight: Theme.compactDesktop ? 36 : 44
    implicitWidth: 220
    model: options
    padding: Theme.compactDesktop ? 8 : 12
    spacing: 8
    hoverEnabled: true

    background: Rectangle {
        radius: Theme.radiusControl
        color: select.hovered && !select.popup.visible ? Theme.hover : Theme.field
        border.color: select.activeFocus || select.popup.visible ? Theme.blue : Theme.line
    }

    contentItem: BodyText {
        leftPadding: select.leftPadding
        rightPadding: select.indicator.width + select.spacing
        text: I18n.t(select.displayText)
        color: select.enabled ? Theme.text : Theme.muted
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        wrapMode: Text.NoWrap
    }

    indicator: UiIcon {
        x: select.width - width - select.rightPadding
        y: select.topPadding + (select.availableHeight - height) / 2
        width: 16
        height: 16
        symbol: "arrow"
        tone: "muted"
        rotation: select.popup.visible ? -90 : 90
        Behavior on rotation { NumberAnimation { duration: 120 } }
    }

    delegate: Controls.ItemDelegate {
        id: option
        required property int index
        required property var modelData
        width: ListView.view ? ListView.view.width : select.width
        height: Theme.compactDesktop ? 34 : 38
        hoverEnabled: true
        highlighted: select.highlightedIndex === index

        background: Rectangle {
            radius: 8
            color: option.highlighted ? Theme.hover : "transparent"
        }

        contentItem: RowLayout {
            spacing: 8
            BodyText {
                text: I18n.t(option.modelData)
                font.pixelSize: 13
                Layout.fillWidth: true
                elide: Text.ElideRight
                wrapMode: Text.NoWrap
            }
            UiIcon {
                visible: select.currentIndex === option.index
                symbol: "check"
                tone: "send"
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
            }
        }
    }

    popup: Controls.Popup {
        y: select.height + 6
        width: select.width
        padding: 6
        implicitHeight: Math.min(contentItem.implicitHeight + padding * 2, 300)
        closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside

        background: GlassSurface {
            glassTint: Theme.panel
            tintStrength: 0.78
            glassOpacity: 0.98
            radius: 12
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: select.popup.visible ? select.delegateModel : null
            currentIndex: select.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            Controls.ScrollIndicator.vertical: Controls.ScrollIndicator { }
        }

        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 100 } }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 80 } }
    }
}
