import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGC
import QGroundControl
import QGroundControl.Controls

/// Compact real-time plot module. It deliberately contains no value/stat cards:
/// the plotted curves are the primary content.
Item {
    id: root

    readonly property color _ink: "#102A43"
    readonly property color _muted: "#627D98"
    readonly property color _line: "#D9E2EC"
    readonly property color _soft: "#F5F8FB"
    readonly property color _teal: "#008C9E"
    readonly property real _gap: ScreenTools.defaultFontPixelWidth * 0.75
    readonly property real _pad: ScreenTools.defaultFontPixelWidth * 0.9
    readonly property real _radius: ScreenTools.defaultFontPixelWidth * 0.8
    readonly property var sensor: SimpleTest

    property int displayMode: sensor.surveyMode === 1 ? 1 : 0
    property real scrollOffset: 0
    property int windowSeconds: 20
    property bool removeMean: false
    property bool liveFollow: true
    property var _scalarSeries: ({})
    property var _vectorSeries: ({})

    readonly property var colorsX: [ "#E29A2D", "#2C8FBF", "#2F9E7A" ]
    readonly property var colorsY: [ "#E29A2D", "#2C8FBF", "#2F9E7A" ]
    readonly property var colorsZ: [ "#E29A2D", "#2C8FBF", "#2F9E7A" ]

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    Component.onCompleted: refreshSeries()

    Connections {
        target: root.sensor
        function onDataUpdated() {
            if (root.visible && root.liveFollow && !refreshTimer.running) refreshTimer.start()
        }
    }

    Timer {
        id: refreshTimer
        interval: 220
        repeat: false
        onTriggered: root.refreshSeries()
    }

    function refreshSeries() {
        if (!root.liveFollow) return
        // The scalar layout also needs the raw fluxgate XYZ row, while the
        // vector layout uses the same cached series for its three panels.
        root._scalarSeries = root.sensor.magSeries(root.windowSeconds, root.removeMean)
        root._vectorSeries = root.sensor.magVectorSeries(root.windowSeconds, root.removeMean)
    }

    function setDisplayMode(mode) {
        root.displayMode = mode
        root.refreshSeries()
    }

    component PlotPanel: Rectangle {
        id: panel
        property string title
        property var lines: []
        onLinesChanged: canvas.requestPaint()
        Layout.fillWidth: true
        Layout.preferredWidth: 180
        Layout.preferredHeight: 178
        radius: root._radius
        color: "#FFFFFF"
        border.width: 1
        border.color: root._line

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root._pad * 0.8
            spacing: root._gap * 0.35

            RowLayout {
                Layout.fillWidth: true
                spacing: root._gap * 0.5
                QGCLabel {
                    text: panel.title
                    color: root._ink
                    font.family: "Times New Roman"
                    font.bold: true
                    font.pointSize: ScreenTools.smallFontPointSize
                }
                Item { Layout.fillWidth: true }
                Repeater {
                    model: panel.lines
                    RowLayout {
                        spacing: 3
                        Rectangle { Layout.preferredWidth: 7; Layout.preferredHeight: 7; radius: 3.5; color: modelData.color }
                        QGCLabel { text: modelData.name; color: root._muted; font.family: "Times New Roman"; font.bold: true; font.pointSize: ScreenTools.smallFontPointSize * 0.78 }
                    }
                }
            }

            Canvas {
                id: canvas
                Layout.fillWidth: true
                Layout.fillHeight: true
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                property bool valuesDirty: false

                onPaint: {
                    var ctx = getContext("2d")
                    var w = width, h = height
                    ctx.clearRect(0, 0, w, h)
                    var left = 30, right = 8, top = 5, bottom = 16
                    var plotW = Math.max(1, w - left - right)
                    var plotH = Math.max(1, h - top - bottom)
                    var all = []
                    for (var l = 0; l < panel.lines.length; ++l) {
                        var values = panel.lines[l].values || []
                        for (var i = 0; i < values.length; ++i) {
                            if (isFinite(values[i])) all.push(values[i])
                        }
                    }
                    if (all.length < 2) {
                        ctx.fillStyle = "rgba(98,125,152,0.7)"
                        ctx.font = "bold 11px 'Times New Roman'"
                        ctx.textAlign = "center"
                        ctx.fillText(qsTr("等待数据…"), w / 2, h / 2)
                        return
                    }
                    var minV = Math.min.apply(null, all)
                    var maxV = Math.max.apply(null, all)
                    if (maxV - minV < 0.001) { minV -= 0.5; maxV += 0.5 }
                    var range = maxV - minV
                    var pad = Math.max(range * 0.10, 0.1)
                    minV -= pad
                    maxV += pad
                    ctx.strokeStyle = "#E8EEF4"
                    ctx.lineWidth = 1
                    for (var g = 0; g <= 3; ++g) {
                        var gy = top + plotH * g / 3
                        ctx.beginPath()
                        ctx.moveTo(left, gy)
                        ctx.lineTo(left + plotW, gy)
                        ctx.stroke()
                    }
                    for (var gl = 0; gl < panel.lines.length; ++gl) {
                        var vals = panel.lines[gl].values || []
                        if (vals.length < 2) continue
                        ctx.strokeStyle = panel.lines[gl].color
                        ctx.lineWidth = 2.2
                        ctx.lineJoin = "round"
                        ctx.beginPath()
                        for (var p = 0; p < vals.length; ++p) {
                            var px = left + plotW * p / (vals.length - 1)
                            var py = top + plotH * (1 - (vals[p] - minV) / (maxV - minV))
                            if (p === 0) ctx.moveTo(px, py)
                            else ctx.lineTo(px, py)
                        }
                        ctx.stroke()
                    }
                    ctx.fillStyle = root._muted
                    ctx.font = "bold 9px 'Times New Roman'"
                    ctx.textAlign = "left"
                    ctx.fillText(maxV.toFixed(1), 2, top + 7)
                    ctx.fillText(minV.toFixed(1), 2, top + plotH)
                }
            }
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: content.implicitHeight + _pad * 2
        clip: true
        onContentYChanged: root.scrollOffset = contentY

        ColumnLayout {
            id: content
            x: _pad
            y: _pad
            width: parent.width - _pad * 2
            spacing: root._gap

            RowLayout {
                Layout.fillWidth: true
                spacing: root._gap * 0.5
                QGCLabel {
                    text: qsTr("实时曲线")
                    color: root._ink
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
                Item { Layout.fillWidth: true }
                QGCButton {
                    text: qsTr("标量 Scalar")
                    primary: root.displayMode === 0
                    onClicked: root.setDisplayMode(0)
                }
                QGCButton {
                    text: qsTr("矢量 Vector")
                    primary: root.displayMode === 1
                    onClicked: root.setDisplayMode(1)
                }
                Repeater {
                    model: [5, 20, 100]
                    QGCButton {
                        text: modelData + "s"
                        primary: root.windowSeconds === modelData
                        onClicked: {
                            root.windowSeconds = modelData
                            root.refreshSeries()
                        }
                    }
                }
                QGCLabel { text: qsTr("去均值"); color: root._muted; font.pointSize: ScreenTools.smallFontPointSize }
                Switch {
                    checked: root.removeMean
                    onToggled: {
                        root.removeMean = checked
                        root.refreshSeries()
                    }
                }
                QGCLabel {
                    visible: !root.liveFollow
                    text: qsTr("已暂停")
                    color: root._muted
                    font.pointSize: ScreenTools.smallFontPointSize
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.displayMode === 0
                spacing: root._gap
                PlotPanel {
                    Layout.fillWidth: true
                    title: "S1"
                    lines: [ { name: "S1", color: root.colorsX[0], values: root._scalarSeries.s1 || [] } ]
                }
                PlotPanel {
                    Layout.fillWidth: true
                    title: "S2"
                    lines: [ { name: "S2", color: root.colorsX[1], values: root._scalarSeries.s2 || [] } ]
                }
                PlotPanel {
                    Layout.fillWidth: true
                    title: "S1 − S2"
                    lines: [ { name: "Δ", color: root.colorsX[2], values: root._scalarSeries.diff || [] } ]
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    columnSpacing: root._gap
                    rowSpacing: root._gap
                    PlotPanel {
                        title: "磁通门 MX"
                        lines: [ { name: "MX", color: root.colorsX[0], values: root._vectorSeries.s1x || [] } ]
                    }
                    PlotPanel {
                        title: "磁通门 MY"
                        lines: [ { name: "MY", color: root.colorsY[1], values: root._vectorSeries.s1y || [] } ]
                    }
                    PlotPanel {
                        title: "磁通门 MZ"
                        lines: [ { name: "MZ", color: root.colorsZ[2], values: root._vectorSeries.s1z || [] } ]
                    }
                }
            }

            // Vector: the X/Y/Z plots of one gate are side by side, not
            // overlaid in one canvas. Each row is one gate and each column is
            // one axis, so the time axis stays aligned across the row.
            GridLayout {
                Layout.fillWidth: true
                visible: root.displayMode === 1
                columns: 3
                columnSpacing: root._gap
                rowSpacing: root._gap
                PlotPanel {
                    title: "S1 · X"
                    lines: [ { name: "X", color: root.colorsX[0], values: root._vectorSeries.s1x || [] } ]
                }
                PlotPanel {
                    title: "S1 · Y"
                    lines: [ { name: "Y", color: root.colorsY[1], values: root._vectorSeries.s1y || [] } ]
                }
                PlotPanel {
                    title: "S1 · Z"
                    lines: [ { name: "Z", color: root.colorsZ[2], values: root._vectorSeries.s1z || [] } ]
                }
                PlotPanel {
                    title: "S2 · X"
                    lines: [ { name: "X", color: root.colorsX[0], values: root._vectorSeries.s2x || [] } ]
                }
                PlotPanel {
                    title: "S2 · Y"
                    lines: [ { name: "Y", color: root.colorsY[1], values: root._vectorSeries.s2y || [] } ]
                }
                PlotPanel {
                    title: "S2 · Z"
                    lines: [ { name: "Z", color: root.colorsZ[2], values: root._vectorSeries.s2z || [] } ]
                }
                PlotPanel {
                    title: "S1−S2 · X"
                    lines: [ { name: "ΔX", color: root.colorsX[0], values: root._vectorSeries.dx || [] } ]
                }
                PlotPanel {
                    title: "S1−S2 · Y"
                    lines: [ { name: "ΔY", color: root.colorsY[1], values: root._vectorSeries.dy || [] } ]
                }
                PlotPanel {
                    title: "S1−S2 · Z"
                    lines: [ { name: "ΔZ", color: root.colorsZ[2], values: root._vectorSeries.dz || [] } ]
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                font.family: "Times New Roman"
                text: root.displayMode === 0
                      ? qsTr("标量布局：S1、S2 与 S1−S2 独立图窗")
                      : qsTr("矢量布局：每个传感器的 X/Y/Z 横向独立显示；切换布局不改变采集协议")
                color: root._muted
                font.pointSize: ScreenTools.smallFontPointSize * 0.86
            }
            Item { Layout.fillHeight: true }
        }
    }
}
