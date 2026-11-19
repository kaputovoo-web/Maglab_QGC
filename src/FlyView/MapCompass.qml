import QtQuick
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls

/// Lightweight map compass used by the Fly View. The outer ring stays upright;
/// the black/white diamond needle rotates opposite to the map bearing so that
/// its white/black north edge always points at true north on screen.
Item {
    id:     root
    width:  54
    height: 54

    property real bearing: 0

    Rectangle {
        anchors.fill: parent
        radius:       width / 2
        color:        Qt.rgba(1, 1, 1, 0.60)
        border.width: 1
        border.color: Qt.rgba(0.62, 0.72, 0.82, 0.38)
    }

    Item {
        id:                 needle
        anchors.fill:       parent
        rotation:           -root.bearing
        antialiasing:       true

        Canvas {
            id:             canvas
            anchors.fill:   parent
            antialiasing:   true

            onPaint: {
                const ctx = getContext("2d")
                const cx = width / 2
                const cy = height / 2 + 2
                const r = Math.min(width, height) * 0.30
                ctx.clearRect(0, 0, width, height)

                // North half: dark charcoal
                ctx.beginPath()
                ctx.moveTo(cx, cy - r)
                ctx.lineTo(cx + r * 0.48, cy)
                ctx.lineTo(cx - r * 0.48, cy)
                ctx.closePath()
                ctx.fillStyle = "#1F2937"
                ctx.fill()

                // South half: white with a light outline
                ctx.beginPath()
                ctx.moveTo(cx, cy + r)
                ctx.lineTo(cx + r * 0.48, cy)
                ctx.lineTo(cx - r * 0.48, cy)
                ctx.closePath()
                ctx.fillStyle = "#FFFFFF"
                ctx.fill()
                ctx.strokeStyle = "#9AA9B8"
                ctx.lineWidth = 1
                ctx.stroke()

                // Centre dot
                ctx.beginPath()
                ctx.arc(cx, cy, 2.2, 0, Math.PI * 2)
                ctx.fillStyle = "#FFFFFF"
                ctx.fill()
                ctx.strokeStyle = "#1F2937"
                ctx.lineWidth = 1
                ctx.stroke()

                ctx.font = "bold 9px 'Times New Roman'"
                ctx.fillStyle = "#1F2937"
                ctx.textAlign = "center"
                ctx.fillText("N", cx, cy - r - 3)
            }

            Connections {
                target: root
                function onBearingChanged() { canvas.requestPaint() }
            }
        }
    }

    ToolTip.visible: hoverEnabled && !ScreenTools.isMobile
    ToolTip.text:    qsTr("指北针")
    ToolTip.delay:   500

    MouseArea {
        id:             hoverArea
        anchors.fill:   parent
        hoverEnabled:   true
        acceptedButtons: Qt.NoButton
    }

    property alias hoverEnabled: hoverArea.hoverEnabled
}
