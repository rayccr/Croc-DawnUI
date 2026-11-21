import QtQuick

// 进度圆环：由 Canvas 绘制；progress 为负值时表示无法计量，只显示占位。
Item {
    id: ring
    property real progress: -1
    property color accent: Theme.blue
    property string caption: I18n.t("传输进度")
    implicitWidth: 104
    implicitHeight: 104

    Canvas {
        id: ringCanvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            ctx.lineWidth = 5;
            ctx.lineCap = "round";
            var r = (Math.min(width, height) - 8) / 2;
            // 底色与 Theme.rule 一致，Canvas 需要字符串形式的颜色。
            ctx.strokeStyle = "#2a2e33";
            ctx.beginPath();
            ctx.arc(width / 2, height / 2, r, 0, Math.PI * 2);
            ctx.stroke();
            if (ring.progress >= 0) {
                ctx.strokeStyle = ring.accent;
                ctx.beginPath();
                ctx.arc(width / 2, height / 2, r, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * ring.progress / 100);
                ctx.stroke();
            }
        }

        Connections {
            target: ring
            function onProgressChanged() { ringCanvas.requestPaint() }
            function onAccentChanged() { ringCanvas.requestPaint() }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 4
        BodyText {
            width: ring.width
            text: ring.progress < 0 ? "—" : ring.progress.toFixed(2) + "%"
            wrapMode: Text.NoWrap
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: 18
            font.pixelSize: 27
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
        }
        Hint {
            width: ring.width
            text: ring.caption
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 11
        }
    }
}
