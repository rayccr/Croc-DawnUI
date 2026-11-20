import QtQuick

// 应用内部的环境光背景。材质只采样这一层，不采样文字、口令或其他窗口。
Canvas {
    id: backdrop
    renderTarget: Canvas.Image
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        var ctx = getContext("2d");
        ctx.reset();
        ctx.fillStyle = "#0b0e13";
        ctx.fillRect(0, 0, width, height);
        function glow(x, y, radius, color) {
            var gradient = ctx.createRadialGradient(x, y, 0, x, y, radius);
            gradient.addColorStop(0, color);
            gradient.addColorStop(1, "transparent");
            ctx.fillStyle = gradient;
            ctx.fillRect(0, 0, width, height);
        }
        var span = Math.max(width, height);
        glow(width * 0.08, height * 0.22, span * 0.66, "#1b4b7092");
        glow(width * 0.94, height * 0.38, span * 0.64, "#154d7969");
        glow(width * 0.57, height * 0.98, span * 0.55, "#166b538e");
        // 一束很淡的斜向晨光，为边缘折射提供可见的纹理。
        var dawn = ctx.createLinearGradient(0, 0, width, height);
        dawn.addColorStop(0, "transparent");
        dawn.addColorStop(0.38, "#00c2d9ff");
        dawn.addColorStop(0.52, "#05c2d9ff");
        dawn.addColorStop(0.66, "#00c2d9ff");
        dawn.addColorStop(1, "transparent");
        ctx.fillStyle = dawn;
        ctx.fillRect(0, 0, width, height);
    }
}
