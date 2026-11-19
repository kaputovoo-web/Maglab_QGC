import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGC
import QGroundControl
import QGroundControl.Controls

/// Semi-transparent live scalar-magnetometer curve for Fly View.
/// The data source is selectable without leaving the flight screen.
Item {
    id: root
    visible: false
    // Keep this overlay small. The Fly View loader positions it at the
    // top-right; it must never stretch to the loader's full size.
    width: 156
    height: 84
    x: 0
    y: 0
    z: QGroundControl.zOrderTopMost - 1
    clip: true

    property int sourceIndex: 0
    property var _series: ({})
    readonly property var sensor: SimpleTest
    readonly property var sources: [qsTr("S1"), qsTr("S2"), qsTr("S1 − S2")]
    readonly property color _cyan: "#36D1DC"
    readonly property color _bg: Qt.rgba(1.0, 1.0, 1.0, 0.68)
    readonly property color _ink: "#132B45"
    readonly property color _muted: "#607086"

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: root._bg
        border.width: 1
        border.color: Qt.rgba(0.45, 0.55, 0.65, 0.35)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 5
            QGCLabel {
                text: qsTr("磁场曲线")
                color: root._ink
                font.family: "Times New Roman"
                font.bold: true
                font.pointSize: ScreenTools.smallFontPointSize
            }
            Item { Layout.fillWidth: true }
            QGCComboBox {
                Layout.preferredWidth: 52
                model: root.sources
                currentIndex: root.sourceIndex
                font.pointSize: ScreenTools.smallFontPointSize * 0.85
                onActivated: (index) => {
                    root.sourceIndex = index
                    chart.requestPaint()
                }
            }
            QGCButton {
                text: "×"
                pointSize: ScreenTools.smallFontPointSize
                onClicked: root.visible = false
            }
        }

        Canvas {
            id: chart
            Layout.fillWidth: true
            Layout.fillHeight: true
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                var series = root._series || {}
                var values = root.sourceIndex === 0 ? (series.s1 || [])
                           : root.sourceIndex === 1 ? (series.s2 || [])
                           : (series.diff || [])
                var left = 4, right = 4, top = 4, bottom = 4
                var plotW = Math.max(1, width - left - right)
                var plotH = Math.max(1, height - top - bottom)
                if (values.length < 2) {
                    ctx.fillStyle = "rgba(19,43,69,0.62)"
                    ctx.font = "bold 11px 'Times New Roman'"
                    ctx.textAlign = "center"
                    ctx.fillText(qsTr("等待磁测数据…"), width / 2, height / 2)
                    return
                }
                var minV = Infinity, maxV = -Infinity
                for (var i = 0; i < values.length; ++i) {
                    minV = Math.min(minV, values[i])
                    maxV = Math.max(maxV, values[i])
                }
                if (!isFinite(minV) || !isFinite(maxV)) return
                var pad = Math.max((maxV - minV) * 0.12, 0.5)
                minV -= pad
                maxV += pad
                ctx.strokeStyle = "rgba(19,43,69,0.16)"
                ctx.lineWidth = 1
                for (var g = 0; g <= 2; ++g) {
                    var gy = top + plotH * g / 2
                    ctx.beginPath()
                    ctx.moveTo(left, gy)
                    ctx.lineTo(left + plotW, gy)
                    ctx.stroke()
                }
                ctx.strokeStyle = root._cyan
                ctx.lineWidth = 2.2
                ctx.lineJoin = "round"
                ctx.beginPath()
                for (var p = 0; p < values.length; ++p) {
                    var px = left + plotW * p / (values.length - 1)
                    var py = top + plotH * (1 - (values[p] - minV) / (maxV - minV))
                    if (p === 0) ctx.moveTo(px, py)
                    else ctx.lineTo(px, py)
                }
                ctx.stroke()
                ctx.fillStyle = "rgba(19,43,69,0.62)"
                ctx.font = "bold 10px 'Times New Roman'"
                ctx.textAlign = "left"
                ctx.fillText(maxV.toFixed(1), 5, 11)
                ctx.fillText(minV.toFixed(1), 5, height - 4)
            }
        }
    }

    Timer {
        interval: 220
        running: root.visible
        repeat: true
        onTriggered: {
            root._series = root.sensor.magSeries(20, false)
            chart.requestPaint()
        }
    }
}
