import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGC
import QGroundControl
import QGroundControl.Controls

Rectangle {
    id: root
    color: "transparent"

    readonly property color _navy: "#0B2239"
    readonly property color _cyan: "#00A6B8"
    readonly property color _text: "#132B45"
    readonly property color _muted: "#607086"
    readonly property color _border: "#DCE5EE"
    readonly property color _ok: "#1E9E6A"
    readonly property color _warn: "#D97706"
    readonly property color _error: "#D64545"

    readonly property real _margin: ScreenTools.defaultFontPixelWidth * 0.6
    readonly property real _cardPad: ScreenTools.defaultFontPixelWidth * 0.9
    readonly property real _radius: ScreenTools.defaultFontPixelWidth * 0.7
    readonly property bool _narrow: width < 520
    readonly property var sensor: SimpleTest

    property int module: 0              // 0 realtime, 1 magnetic map, 2 data storage
    property int displayMode: 0         // display-only; never changes survey/transport mode
    property bool showVectorTotal: true
    property bool showVectorDiff: true
    property real scrollOffset: 0
    property int windowSeconds: 20
    property bool removeMean: false
    property bool autoScale: true
    property bool liveFollow: true
    property bool showS1: true
    property bool showS2: true
    property bool showDiff: true
    property real _lockedMin: -1
    property real _lockedMax: 1
    property var _frozenSeries: null
    property var _liveSeries: null
    property var savedFiles: []
    property string importError: ""

    property real _nowMs: Date.now()
    readonly property bool sensorOnline: sensor.lastSampleMs > 0 && (_nowMs - sensor.lastSampleMs) < 3000
    readonly property string lastUpdateText: sensor.lastSampleMs > 0
                                              ? Qt.formatDateTime(new Date(sensor.lastSampleMs), "HH:mm:ss")
                                              : "—"

    QGCPalette { id: qgcPal; colorGroupEnabled: true }
    Handle { id: generator }

    Component.onCompleted: {
        root.displayMode = root.sensor.surveyMode === 1 ? 1 : 0
        root.refreshSavedFiles()
    }
    onModuleChanged: {
        if (root.module === 2) root.refreshSavedFiles()
    }
    Connections {
        target: root.sensor
        function onSurveyModeChanged() {
            root.displayMode = root.sensor.surveyMode === 1 ? 1 : 0
        }
        function onSaveFlagChanged() {
            if (!root.sensor.saveFlag) root.refreshSavedFiles()
        }
    }

    function refreshSavedFiles() {
        root.savedFiles = MagLabPaths.surveyFiles()
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            root._nowMs = Date.now()
            if (!root.liveFollow) {
                chart.requestPaint()
            } else if (root._liveSeries === null) {
                root.refreshLiveSeries()
            }
        }
    }

    // Coalesce incoming UDP samples into at most ~5 chart repaints per second.
    // The C++ series is cached by sample revision, and the Canvas never calls
    // back into C++ from onPaint.
    Timer {
        id: chartRefreshTimer
        interval: 180
        repeat: false
        onTriggered: root.refreshLiveSeries()
    }

    Connections {
        target: root.sensor
        function onDataUpdated() {
            if (root.visible && root.liveFollow && !chartRefreshTimer.running) {
                chartRefreshTimer.start()
            }
        }
    }

    function valueText(value) {
        return isFinite(value) ? Number(value).toFixed(3) : "—"
    }

    function rangeText(value) {
        if (!isFinite(value)) return "—"
        return Math.abs(value) >= 1000 ? Number(value).toFixed(0) : Number(value).toFixed(1)
    }

    function clock(seconds) {
        var s = Math.max(0, Math.floor(seconds))
        var m = Math.floor(s / 60)
        var r = s % 60
        return (m < 10 ? "0" : "") + m + ":" + (r < 10 ? "0" : "") + r
    }

    function refreshLiveSeries() {
        if (!root.liveFollow) return
        root._liveSeries = root.sensor.magSeries(root.windowSeconds, root.removeMean)
        chart.requestPaint()
    }

    function captureFrozen() {
        root._frozenSeries = root.sensor.magSeries(root.windowSeconds, root.removeMean)
    }

    function setWindow(seconds) {
        root.windowSeconds = seconds
        if (root.liveFollow) root.refreshLiveSeries()
        else {
            root.captureFrozen()
            chart.requestPaint()
        }
    }

    function setMean(enabled) {
        root.removeMean = enabled
        if (root.liveFollow) root.refreshLiveSeries()
        else {
            root.captureFrozen()
            chart.requestPaint()
        }
    }

    function setFollow(enabled) {
        root.liveFollow = enabled
        if (enabled) root.refreshLiveSeries()
        else root.captureFrozen()
        chart.requestPaint()
    }

    function importPath(path) {
        var clean = String(path)
        try { clean = decodeURIComponent(clean) } catch (e) { }
        clean = clean.replace(/^file:\/\//, "")
        if (!/\.(csv|txt)$/i.test(clean)) {
            root.importError = qsTr("请选择 .csv 或 .txt 测量数据文件")
            return
        }
        root.importError = ""
        generator.selectSource(clean)
    }

    component Card: Rectangle {
        id: card
        default property alias content: cardColumn.data
        Layout.fillWidth: true
        implicitHeight: cardColumn.implicitHeight + root._cardPad * 2
        color: "#FFFFFF"
        radius: root._radius
        border.width: 1
        border.color: root._border
        ColumnLayout {
            id: cardColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: root._cardPad
            spacing: root._margin
        }
    }

    component SectionTitle: RowLayout {
        property string title
        property string hint: ""
        Layout.fillWidth: true
        spacing: root._margin
        Rectangle {
            Layout.preferredWidth: 3
            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.1
            radius: 2
            color: root._cyan
        }
        QGCLabel {
            text: title
            color: root._navy
            font.bold: true
            font.pointSize: ScreenTools.mediumFontPointSize
            elide: Text.ElideRight
        }
        Item { Layout.fillWidth: true }
        QGCLabel {
            visible: hint !== ""
            text: hint
            color: root._muted
            font.pointSize: ScreenTools.smallFontPointSize
            elide: Text.ElideRight
        }
    }

    component ValueTile: Rectangle {
        id: tile
        property string label
        property string channel
        property real value
        property color accent
        Layout.fillWidth: true
        implicitHeight: tileColumn.implicitHeight + root._cardPad * 1.25
        radius: root._radius
        color: "#FFFFFF"
        border.width: 1
        border.color: root._border
        ColumnLayout {
            id: tileColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: root._cardPad
            spacing: root._margin * 0.35
            RowLayout {
                spacing: root._margin * 0.5
                Rectangle {
                    Layout.preferredWidth: 7
                    Layout.preferredHeight: 7
                    radius: 3.5
                    color: tile.accent
                }
                QGCLabel {
                    Layout.fillWidth: true
                    text: tile.label
                    color: root._muted
                    font.pointSize: ScreenTools.smallFontPointSize
                    elide: Text.ElideRight
                }
                QGCLabel {
                    text: tile.channel
                    color: tile.accent
                    font.pointSize: ScreenTools.smallFontPointSize * 0.85
                    font.bold: true
                }
            }
            RowLayout {
                spacing: root._margin * 0.4
                QGCLabel {
                    text: root.valueText(tile.value)
                    color: root._navy
                    font.bold: true
                    font.pointSize: ScreenTools.largeFontPointSize * 1.15
                }
                QGCLabel {
                    text: "nT"
                    color: root._muted
                    font.pointSize: ScreenTools.smallFontPointSize
                }
                Item { Layout.fillWidth: true }
                QGCLabel {
                    text: qsTr("更新 %1").arg(root.lastUpdateText)
                    color: root._muted
                    font.pointSize: ScreenTools.smallFontPointSize * 0.8
                }
            }
        }
    }

    component AxisTile: Rectangle {
        id: axisTile
        Layout.fillWidth: true
        implicitHeight: axisColumn.implicitHeight + root._cardPad * 1.25
        radius: root._radius
        color: "#FFFFFF"
        border.width: 1
        border.color: root._border
        ColumnLayout {
            id: axisColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: root._cardPad
            spacing: root._margin * 0.2
            RowLayout {
                Layout.fillWidth: true
                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("三轴磁通门")
                    color: root._muted
                    font.pointSize: ScreenTools.smallFontPointSize
                }
                QGCLabel {
                    text: "XYZ"
                    color: root._cyan
                    font.bold: true
                    font.pointSize: ScreenTools.smallFontPointSize * 0.85
                }
            }
            Repeater {
                model: [
                    { label: "X", value: root.sensor.axisX },
                    { label: "Y", value: root.sensor.axisY },
                    { label: "Z", value: root.sensor.axisZ }
                ]
                RowLayout {
                    Layout.fillWidth: true
                    spacing: root._margin * 0.3
                    QGCLabel {
                        text: modelData.label
                        color: root._muted
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: root.valueText(modelData.value) + " nT"
                        color: root._navy
                        font.bold: true
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                }
            }
        }
    }

    component LegendChip: Item {
        id: chip
        property string label
        property color swatch
        property bool active: true
        signal toggled()
        Layout.preferredWidth: chipRow.implicitWidth
        Layout.preferredHeight: chipRow.implicitHeight
        RowLayout {
            id: chipRow
            anchors.fill: parent
            spacing: root._margin * 0.4
            Rectangle {
                Layout.preferredWidth: 9
                Layout.preferredHeight: 9
                radius: 4.5
                color: chip.swatch
                opacity: chip.active ? 1 : 0.25
            }
            QGCLabel {
                text: chip.label
                color: chip.active ? root._text : root._muted
                font.pointSize: ScreenTools.smallFontPointSize
                opacity: chip.active ? 1 : 0.5
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: chip.toggled()
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: mainColumn.implicitHeight
        onContentYChanged: root.scrollOffset = contentY
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: mainColumn
            width: parent.width
            spacing: root._margin

            RowLayout {
                Layout.fillWidth: true
                visible: root.module === 0
                spacing: root._margin * 0.7
                QGCLabel {
                    text: qsTr("实时显示")
                    color: root._navy
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
                Item { Layout.fillWidth: true }
                QGCButton {
                    text: qsTr("标量 Scalar")
                    primary: root.displayMode === 0
                    onClicked: root.displayMode = 0
                }
                QGCButton {
                    text: qsTr("矢量 Vector")
                    primary: root.displayMode === 1
                    onClicked: root.displayMode = 1
                }
            }

            GridLayout {
                Layout.fillWidth: true
                visible: root.module === 0 && root.displayMode === 0
                columns: root._narrow ? 1 : 4
                columnSpacing: root._margin
                rowSpacing: root._margin

                ValueTile {
                    label: qsTr("光泵 1")
                    channel: "CH1"
                    value: root.sensor.scalar1
                    accent: "#E29A2D"
                }
                ValueTile {
                    label: qsTr("光泵 2")
                    channel: "CH2"
                    value: root.sensor.scalar2
                    accent: "#2C8FBF"
                }
                ValueTile {
                    label: qsTr("差分")
                    channel: "Δ"
                    value: root.sensor.scalar1 - root.sensor.scalar2
                    accent: "#2F9E7A"
                }
                AxisTile { visible: root.displayMode === 0 }
            }

            Card {
                visible: root.module === 0 && root.displayMode === 1
                SectionTitle {
                    title: qsTr("矢量分量")
                    hint: qsTr("Vector")
                }
                GridLayout {
                    Layout.fillWidth: true
                    // Keep label/value pairs together. A 3-column grid splits
                    // pairs across rows and makes the vector readings look
                    // corrupted on narrow Android layouts.
                    columns: root._narrow ? 2 : 6
                    columnSpacing: root._margin
                    rowSpacing: root._margin * 0.6
                    QGCLabel { text: qsTr("FG1 X"); color: root._muted }
                    QGCLabel { text: root.valueText(root.sensor.fg1x) + " nT"; font.bold: true }
                    QGCLabel { text: qsTr("FG1 Y"); color: root._muted }
                    QGCLabel { text: root.valueText(root.sensor.fg1y) + " nT"; font.bold: true }
                    QGCLabel { text: qsTr("FG1 Z"); color: root._muted }
                    QGCLabel { text: root.valueText(root.sensor.fg1z) + " nT"; font.bold: true }
                    QGCLabel { text: qsTr("FG2 X"); color: root._muted }
                    QGCLabel { text: root.valueText(root.sensor.fg2x) + " nT"; font.bold: true }
                    QGCLabel { text: qsTr("FG2 Y"); color: root._muted }
                    QGCLabel { text: root.valueText(root.sensor.fg2y) + " nT"; font.bold: true }
                    QGCLabel { text: qsTr("FG2 Z"); color: root._muted }
                    QGCLabel { text: root.valueText(root.sensor.fg2z) + " nT"; font.bold: true }
                }
                QGCLabel {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: root._muted
                    font.pointSize: ScreenTools.smallFontPointSize * 0.85
                    text: qsTr("矢量布局显示双磁通门 XYZ；S1/S2 为两路总场模长，差分用于磁图。切换布局不会改变底层采集协议。")
                }
            }

            Card {
                visible: root.module === 0 && root.displayMode === 1
                SectionTitle {
                    title: qsTr("总场与差分")
                    hint: qsTr("可单独显示")
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: root._margin * 0.7
                    RowLayout {
                        spacing: root._margin * 0.35
                        QGCLabel { text: qsTr("显示总场"); color: root._muted; font.pointSize: ScreenTools.smallFontPointSize }
                        Switch { checked: root.showVectorTotal; onToggled: root.showVectorTotal = checked }
                    }
                    RowLayout {
                        spacing: root._margin * 0.35
                        QGCLabel { text: qsTr("显示差分"); color: root._muted; font.pointSize: ScreenTools.smallFontPointSize }
                        Switch { checked: root.showVectorDiff; onToggled: root.showVectorDiff = checked }
                    }
                    Item { Layout.fillWidth: true }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: root._margin
                    ValueTile {
                        visible: root.showVectorTotal
                        label: qsTr("FG1 总场")
                        channel: "S1"
                        value: root.sensor.scalar1
                        accent: "#E29A2D"
                    }
                    ValueTile {
                        visible: root.showVectorTotal
                        label: qsTr("FG2 总场")
                        channel: "S2"
                        value: root.sensor.scalar2
                        accent: "#2C8FBF"
                    }
                    ValueTile {
                        visible: root.showVectorDiff
                        label: qsTr("总场差分")
                        channel: "Δ"
                        value: root.sensor.scalar1 - root.sensor.scalar2
                        accent: "#2F9E7A"
                    }
                }
            }

            Card {
                visible: root.module === 0
                SectionTitle {
                    title: qsTr("实时曲线")
                    hint: qsTr("采样 %1 · %2").arg(root.sensor.sampleCount).arg(root.sensorOnline ? qsTr("实时") : qsTr("已暂停"))
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: root._margin * 1.2

                    RowLayout {
                        spacing: root._margin * 0.35
                        QGCLabel {
                            text: qsTr("时间窗")
                            color: root._muted
                            font.pointSize: ScreenTools.smallFontPointSize
                        }
                        Repeater {
                            model: [5, 20, 100]
                            QGCButton {
                                text: modelData + "s"
                                primary: root.windowSeconds === modelData
                                onClicked: root.setWindow(modelData)
                            }
                        }
                    }

                    RowLayout {
                        spacing: root._margin * 0.5
                        QGCLabel {
                            text: qsTr("去均值")
                            color: root._muted
                            font.pointSize: ScreenTools.smallFontPointSize
                        }
                        Switch {
                            checked: root.removeMean
                            onCheckedChanged: root.setMean(checked)
                        }
                    }

                    RowLayout {
                        spacing: root._margin * 0.5
                        QGCLabel {
                            text: qsTr("自动缩放")
                            color: root._muted
                            font.pointSize: ScreenTools.smallFontPointSize
                        }
                        Switch {
                            checked: root.autoScale
                            onCheckedChanged: {
                                root.autoScale = checked
                                chart.requestPaint()
                            }
                        }
                    }

                    QGCButton {
                        text: root.liveFollow ? qsTr("实时跟随") : qsTr("已暂停")
                        primary: root.liveFollow
                        onClicked: root.setFollow(!root.liveFollow)
                    }
                    QGCButton {
                        text: qsTr("适配")
                        onClicked: {
                            root.autoScale = true
                            root.setFollow(true)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root._margin * 1.2
                    LegendChip {
                        label: "S1"
                        swatch: "#E29A2D"
                        active: root.showS1
                        onToggled: { root.showS1 = !root.showS1; chart.requestPaint() }
                    }
                    LegendChip {
                        label: "S2"
                        swatch: "#2C8FBF"
                        active: root.showS2
                        onToggled: { root.showS2 = !root.showS2; chart.requestPaint() }
                    }
                    LegendChip {
                        label: "S1 − S2"
                        swatch: "#2F9E7A"
                        active: root.showDiff
                        onToggled: { root.showDiff = !root.showDiff; chart.requestPaint() }
                    }
                    Item { Layout.fillWidth: true }
                    QGCLabel {
                        text: root.removeMean ? qsTr("窗口均值已移除") : qsTr("原始读数")
                        color: root._muted
                        font.pointSize: ScreenTools.smallFontPointSize * 0.85
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(root._narrow ? 170 : 220, Math.min(340, width * 0.42))

                    Canvas {
                        id: chart
                        anchors.fill: parent
                        property real hoverX: -1

                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.fillStyle = "#FFFFFF"
                            ctx.fillRect(0, 0, width, height)

                            var w = width
                            var h = height
                            var left = 44
                            var right = 10
                            var top = 10
                            var bottom = 22
                            var plotW = Math.max(1, w - left - right)
                            var plotH = Math.max(1, h - top - bottom)

                            var series = root.liveFollow ? (root._liveSeries || {})
                                                         : (root._frozenSeries || {})
                            var s1 = series.s1 || []
                            var s2 = series.s2 || []
                            var diff = series.diff || []
                            var n = Math.min(s1.length, s2.length, diff.length)

                            ctx.strokeStyle = "#E8EEF4"
                            ctx.lineWidth = 1
                            ctx.fillStyle = root._muted
                            ctx.font = "bold 10px 'Times New Roman'"
                            for (var g = 0; g <= 4; ++g) {
                                var gy = top + plotH * g / 4
                                ctx.beginPath()
                                ctx.moveTo(left, gy)
                                ctx.lineTo(left + plotW, gy)
                                ctx.stroke()
                            }
                            for (var v = 0; v <= 4; ++v) {
                                var gx = left + plotW * v / 4
                                ctx.beginPath()
                                ctx.moveTo(gx, top)
                                ctx.lineTo(gx, top + plotH)
                                ctx.stroke()
                            }

                            if (n < 2) {
                                ctx.fillStyle = root._muted
                                ctx.font = "bold 13px 'Times New Roman'"
                                ctx.textAlign = "center"
                                ctx.fillText(root.sensorOnline ? qsTr("等待新的采样数据…")
                                                               : qsTr("传感器未连接，等待 UDP %1 数据").arg(MagSurveySettings.localPort), w / 2, h / 2 - 4)
                                ctx.font = "bold 11px 'Times New Roman'"
                                ctx.fillText(qsTr("连接后曲线会自动开始"), w / 2, h / 2 + 16)
                                ctx.textAlign = "left"
                                return
                            }

                            var minV = Infinity
                            var maxV = -Infinity
                            for (var i = 0; i < n; ++i) {
                                if (root.showS1) { minV = Math.min(minV, s1[i]); maxV = Math.max(maxV, s1[i]) }
                                if (root.showS2) { minV = Math.min(minV, s2[i]); maxV = Math.max(maxV, s2[i]) }
                                if (root.showDiff) { minV = Math.min(minV, diff[i]); maxV = Math.max(maxV, diff[i]) }
                            }
                            if (!isFinite(minV) || !isFinite(maxV)) {
                                minV = -1
                                maxV = 1
                            }
                            if (root.removeMean) {
                                var span = Math.max(Math.abs(minV), Math.abs(maxV), 0.001)
                                minV = -span
                                maxV = span
                            }
                            if (maxV - minV < 0.001) {
                                minV -= 0.5
                                maxV += 0.5
                            }
                            var pad = (maxV - minV) * 0.1
                            minV -= pad
                            maxV += pad

                            if (root.autoScale) {
                                root._lockedMin = minV
                                root._lockedMax = maxV
                            } else {
                                minV = root._lockedMin
                                maxV = root._lockedMax
                                if (maxV - minV < 0.001) {
                                    minV -= 0.5
                                    maxV += 0.5
                                }
                            }

                            function yFor(value) {
                                return top + plotH * (1 - (value - minV) / (maxV - minV))
                            }

                            ctx.strokeStyle = root._muted
                            ctx.fillStyle = root._muted
                            ctx.font = "bold 10px 'Times New Roman'"
                            ctx.textAlign = "right"
                            for (var yl = 0; yl <= 4; ++yl) {
                                var yv = maxV - (maxV - minV) * yl / 4
                                ctx.fillText(root.rangeText(yv), left - 6, top + plotH * yl / 4 + 3)
                            }
                            ctx.textAlign = "left"
                            ctx.fillText("-" + root.windowSeconds + "s", left, h - 5)
                            ctx.textAlign = "center"
                            ctx.fillText("-" + Math.round(root.windowSeconds / 2) + "s", left + plotW / 2, h - 5)
                            ctx.textAlign = "right"
                            ctx.fillText("0s", left + plotW, h - 5)

                            if (root.removeMean) {
                                ctx.strokeStyle = "#C9D5E2"
                                ctx.beginPath()
                                ctx.moveTo(left, yFor(0))
                                ctx.lineTo(left + plotW, yFor(0))
                                ctx.stroke()
                            }

                            function drawLine(values, color) {
                                ctx.strokeStyle = color
                                ctx.lineWidth = 2.2
                                ctx.lineJoin = "round"
                                ctx.beginPath()
                                for (var p = 0; p < n; ++p) {
                                    var px = left + plotW * p / (n - 1)
                                    var py = yFor(values[p])
                                    if (p === 0) ctx.moveTo(px, py)
                                    else ctx.lineTo(px, py)
                                }
                                ctx.stroke()
                            }
                            if (root.showS2) drawLine(s2, "#2C8FBF")
                            if (root.showDiff) drawLine(diff, "#2F9E7A")
                            if (root.showS1) drawLine(s1, "#E29A2D")

                            if (chart.hoverX >= left && chart.hoverX <= left + plotW) {
                                var idx = Math.round((chart.hoverX - left) / plotW * (n - 1))
                                idx = Math.max(0, Math.min(n - 1, idx))
                                var hx = left + plotW * idx / (n - 1)
                                ctx.strokeStyle = "#94A6B8"
                                ctx.lineWidth = 1
                                ctx.beginPath()
                                ctx.moveTo(hx, top)
                                ctx.lineTo(hx, top + plotH)
                                ctx.stroke()
                                var dots = []
                                if (root.showS1) dots.push({ v: s1[idx], c: "#E29A2D" })
                                if (root.showS2) dots.push({ v: s2[idx], c: "#2C8FBF" })
                                if (root.showDiff) dots.push({ v: diff[idx], c: "#2F9E7A" })
                                for (var d = 0; d < dots.length; ++d) {
                                    ctx.fillStyle = dots[d].c
                                    ctx.beginPath()
                                    ctx.arc(hx, yFor(dots[d].v), 2.5, 0, Math.PI * 2)
                                    ctx.fill()
                                }
                                var lines = [
                                    "S1  " + root.valueText(s1[idx]) + " nT",
                                    "S2  " + root.valueText(s2[idx]) + " nT",
                                    "Δ   " + root.valueText(diff[idx]) + " nT"
                                ]
                                ctx.font = "bold 11px 'Times New Roman'"
                                var boxW = 142
                                var boxH = 54
                                var boxX = Math.min(Math.max(hx + 8, left), left + plotW - boxW)
                                var boxY = top + 4
                                ctx.fillStyle = "rgba(255,255,255,0.94)"
                                ctx.strokeStyle = "#C9D5E2"
                                ctx.fillRect(boxX, boxY, boxW, boxH)
                                ctx.strokeRect(boxX, boxY, boxW, boxH)
                                ctx.fillStyle = root._text
                                ctx.textAlign = "left"
                                for (var t = 0; t < lines.length; ++t) {
                                    ctx.fillText(lines[t], boxX + 8, boxY + 16 + t * 15)
                                }
                            }
                        }
                    }

                    Timer {
                        id: hoverRefresh
                        interval: 30
                        repeat: false
                        onTriggered: chart.requestPaint()
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                        cursorShape: Qt.CrossCursor
                        onPositionChanged: function(mouse) {
                            chart.hoverX = mouse.x
                            if (!hoverRefresh.running) hoverRefresh.start()
                        }
                        onExited: {
                            chart.hoverX = -1
                            chart.requestPaint()
                        }
                    }
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: root.liveFollow ? qsTr("拖动查看各时刻读数 · 数据更新时自动跟随最新采样")
                                         : qsTr("已暂停：当前显示暂停时的窗口数据")
                    color: root._muted
                    font.pointSize: ScreenTools.smallFontPointSize * 0.85
                    elide: Text.ElideRight
                }
            }

            Card {
                visible: root.module === 2
                SectionTitle {
                    title: qsTr("数据存储与录制")
                    hint: root.sensor.saveFlag ? qsTr("正在写入 CSV") : qsTr("待机")
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: root._margin * 1.2
                    Rectangle {
                        Layout.preferredWidth: 10
                        Layout.preferredHeight: 10
                        radius: 5
                        color: root.sensor.saveFlag ? "#D64545" : "#9AA9B8"
                        SequentialAnimation on opacity {
                            running: root.sensor.saveFlag
                            loops: Animation.Infinite
                            NumberAnimation { from: 1; to: 0.35; duration: 700 }
                            NumberAnimation { from: 0.35; to: 1; duration: 700 }
                        }
                    }
                    QGCLabel {
                        text: root.sensor.saveFlag ? qsTr("录制中") : qsTr("未录制")
                        color: root.sensor.saveFlag ? "#D64545" : root._text
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    QGCLabel {
                        text: root.clock(root.sensor.recordingElapsedSeconds)
                        color: root._navy
                        font.bold: true
                        font.pointSize: ScreenTools.mediumFontPointSize
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: root._margin * 1.2
                    QGCLabel {
                        text: qsTr("采样 %1").arg(root.sensor.sampleCount)
                        color: root._muted
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        text: root.sensor.recordingDirectory || qsTr("存储不可用")
                        color: root._muted
                        font.pointSize: ScreenTools.smallFontPointSize
                        elide: Text.ElideMiddle
                    }
                    QGCButton {
                        text: root.sensor.saveFlag ? qsTr("停止录制") : qsTr("开始录制")
                        primary: !root.sensor.saveFlag
                        onClicked: {
                            if (root.sensor.saveFlag) {
                                stopRecordingDialog.open()
                            } else {
                                root.sensor.saveStart()
                            }
                        }
                    }
                }
                QGCLabel {
                    Layout.fillWidth: true
                    visible: root.sensor.errorString.length > 0
                    text: root.sensor.errorString
                    color: root._error
                    wrapMode: Text.WordWrap
                    font.pointSize: ScreenTools.smallFontPointSize
                }
            }

            Card {
                visible: root.module === 2
                SectionTitle {
                    title: qsTr("已保存数据文件")
                    hint: qsTr("%1 个文件").arg(root.savedFiles.length)
                }
                RowLayout {
                    Layout.fillWidth: true
                    QGCLabel {
                        Layout.fillWidth: true
                        text: qsTr("量测 TXT / CSV 文档")
                        color: root._muted
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    QGCButton {
                        text: qsTr("刷新")
                        onClicked: root.refreshSavedFiles()
                    }
                }
                QGCListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.savedFiles.length > 0
                                             ? Math.min(ScreenTools.defaultFontPixelHeight * 15,
                                                        root.savedFiles.length * ScreenTools.defaultFontPixelHeight * 2.4)
                                             : 0
                    visible: root.savedFiles.length > 0
                    model: root.savedFiles
                    clip: true
                    spacing: root._margin * 0.35
                    delegate: Rectangle {
                        width: ListView.view.width
                        height: fileColumn.implicitHeight + root._margin * 1.2
                        radius: root._radius * 0.6
                        color: "#F7F9FB"
                        border.width: 1
                        border.color: root._border
                        ColumnLayout {
                            id: fileColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: root._margin * 0.7
                            spacing: root._margin * 0.15
                            QGCLabel {
                                Layout.fillWidth: true
                                text: modelData.name
                                color: root._navy
                                font.bold: true
                                elide: Text.ElideMiddle
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: root._margin * 0.5
                                QGCLabel {
                                    text: modelData.modified
                                    color: root._muted
                                    font.pointSize: ScreenTools.smallFontPointSize * 0.82
                                }
                                QGCLabel {
                                    Layout.fillWidth: true
                                    text: modelData.size
                                    color: root._muted
                                    horizontalAlignment: Text.AlignRight
                                    font.pointSize: ScreenTools.smallFontPointSize * 0.82
                                }
                            }
                            QGCLabel {
                                Layout.fillWidth: true
                                text: modelData.directory
                                color: root._muted
                                elide: Text.ElideMiddle
                                font.pointSize: ScreenTools.smallFontPointSize * 0.78
                            }
                        }
                    }
                }
                QGCLabel {
                    Layout.fillWidth: true
                    visible: root.savedFiles.length === 0
                    text: qsTr("暂无已保存的量测文档")
                    color: root._muted
                    font.pointSize: ScreenTools.smallFontPointSize
                }
            }

            Card {
                id: importCard
                visible: root.module === 1
                SectionTitle {
                    title: qsTr("磁图数据导入与生成")
                    hint: generator.busy ? qsTr("处理中") : (generator.magmapFilePath !== "" ? qsTr("已生成") : qsTr("待导入"))
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root._margin
                    QGCLabel {
                        Layout.fillWidth: true
                        text: generator.sourceInfo && generator.sourceInfo.name
                              ? generator.sourceInfo.name
                              : qsTr("将 CSV 拖到这里，或选择文件")
                        color: generator.sourceInfo && generator.sourceInfo.name ? root._text : root._muted
                        font.bold: !!(generator.sourceInfo && generator.sourceInfo.name)
                        elide: Text.ElideMiddle
                    }
                    QGCButton {
                        text: qsTr("选择 CSV")
                        enabled: !generator.busy
                        onClicked: csvDialog.openForLoad()
                    }
                }

                QGCLabel {
                    Layout.fillWidth: true
                    visible: !!(generator.sourceInfo && generator.sourceInfo.name)
                    text: qsTr("%1 · %2 · 修改于 %3")
                        .arg(generator.sourceInfo.size || "—")
                        .arg(generator.sourceInfo.path || "")
                        .arg(generator.sourceInfo.modified || "—")
                    color: root._muted
                    font.pointSize: ScreenTools.smallFontPointSize * 0.85
                    elide: Text.ElideMiddle
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: !!(generator.sourceInfo && generator.sourceInfo.lines && generator.sourceInfo.lines.length > 0)
                    implicitHeight: previewColumn.implicitHeight + root._margin * 2
                    radius: 6
                    color: "#F7F9FB"
                    border.width: 1
                    border.color: root._border
                    ColumnLayout {
                        id: previewColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: root._margin
                        spacing: 2
                        QGCLabel {
                            text: qsTr("数据预览")
                            color: root._muted
                            font.pointSize: ScreenTools.smallFontPointSize * 0.85
                        }
                        Repeater {
                            model: generator.sourceInfo && generator.sourceInfo.lines ? generator.sourceInfo.lines : []
                            QGCLabel {
                                Layout.fillWidth: true
                                text: modelData
                                color: root._text
                                font.family: "Times New Roman"
                                font.pointSize: ScreenTools.smallFontPointSize * 0.85
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root._margin
                    QGCButton {
                        Layout.fillWidth: true
                        text: generator.busy ? qsTr("正在生成磁图…") : qsTr("生成磁图")
                        primary: true
                        enabled: generator.magmapPath !== "" && !generator.busy
                        onClicked: generator.generate()
                    }
                    QGCButton {
                        text: qsTr("取消")
                        visible: generator.busy
                        onClicked: generator.cancelGeneration()
                    }
                }
                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: generator.busy
                    visible: running
                }
                QGCLabel {
                    Layout.fillWidth: true
                    visible: (generator.errorString || generator.progressText) !== ""
                    wrapMode: Text.WordWrap
                    text: generator.errorString || generator.progressText
                    color: generator.errorString !== "" ? root._error : root._muted
                    font.pointSize: ScreenTools.smallFontPointSize
                }
                QGCLabel {
                    Layout.fillWidth: true
                    visible: root.importError !== ""
                    text: root.importError
                    color: root._error
                    font.pointSize: ScreenTools.smallFontPointSize
                }
                Item {
                    id: mapPreview
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? Math.min(width * 0.82, ScreenTools.defaultFontPixelHeight * 24) : 0
                    visible: generator.magmapFilePath !== ""
                    clip: true
                    property var clickInfo: ({})
                    readonly property real leftAxis: ScreenTools.defaultFontPixelWidth * 5.0
                    readonly property real bottomAxis: ScreenTools.defaultFontPixelHeight * 2.0
                    readonly property real rightBar: ScreenTools.defaultFontPixelWidth * 6.0

                    Image {
                        id: mapImage
                        anchors.left: parent.left
                        anchors.leftMargin: mapPreview.leftAxis
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.rightMargin: mapPreview.rightBar
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: mapPreview.bottomAxis
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: false
                        sourceSize.width: 1024
                        sourceSize.height: 1024
                        source: generator.magmapFilePath !== "" ? ("file://" + generator.magmapFilePath) : ""
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.CrossCursor
                            onClicked: (mouse) => {
                                mapPreview.clickInfo = generator.valueAtNormalized(
                                            Math.max(0, Math.min(1, mouse.x / width)),
                                            Math.max(0, Math.min(1, mouse.y / height)))
                            }
                        }
                    }

                    Repeater {
                        model: [0, 0.33, 0.66, 1]
                        Text {
                            x: mapPreview.leftAxis + mapImage.width * modelData - width / 2
                            y: mapImage.y + mapImage.height + 3
                            text: {
                                var v = generator.mapMinLon + (generator.mapMaxLon - generator.mapMinLon) * modelData
                                return isFinite(v) ? v.toFixed(4) : "—"
                            }
                            color: root._muted
                            font.family: "Times New Roman"
                            font.pixelSize: 12
                            rotation: -45
                        }
                    }
                    Repeater {
                        model: [0, 0.33, 0.66, 1]
                        Text {
                            x: 2
                            y: mapImage.y + mapImage.height * modelData - height / 2
                            text: {
                                var v = generator.mapMaxLat - (generator.mapMaxLat - generator.mapMinLat) * modelData
                                return isFinite(v) ? v.toFixed(4) : "—"
                            }
                            color: root._muted
                            font.family: "Times New Roman"
                            font.pixelSize: 12
                            rotation: -45
                        }
                    }

                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: mapImage.top
                        anchors.bottom: mapImage.bottom
                        width: 14
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#FF0000" }
                            GradientStop { position: 0.25; color: "#FFFF00" }
                            GradientStop { position: 0.50; color: "#00FF00" }
                            GradientStop { position: 0.75; color: "#00FFFF" }
                            GradientStop { position: 1.0; color: "#0000FF" }
                        }
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.top: mapImage.top
                        text: isFinite(generator.mapMaxValue) ? generator.mapMaxValue.toFixed(1) : "—"
                        color: root._muted
                        font.family: "Times New Roman"
                        font.pixelSize: 11
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.bottom: mapImage.bottom
                        text: isFinite(generator.mapMinValue) ? generator.mapMinValue.toFixed(1) : "—"
                        color: root._muted
                        font.family: "Times New Roman"
                        font.pixelSize: 11
                    }

                    Text {
                        anchors.left: mapImage.left
                        anchors.top: mapImage.bottom
                        anchors.topMargin: mapPreview.bottomAxis - 12
                        text: {
                            if (!isFinite(generator.mapMinLon) || !isFinite(generator.mapMaxLon)) return qsTr("比例尺：—")
                            var dx = (generator.mapMaxLon - generator.mapMinLon) * 111.0 *
                                     Math.cos(mapPreview.clickInfo.lat ? mapPreview.clickInfo.lat * Math.PI / 180 : 0)
                            var dy = (generator.mapMaxLat - generator.mapMinLat) * 111.0
                            return qsTr("跨度 %1 km × %2 km").arg(Math.abs(dx).toFixed(2)).arg(Math.abs(dy).toFixed(2))
                        }
                        color: root._muted
                        font.family: "Times New Roman"
                        font.pixelSize: 12
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        visible: mapPreview.clickInfo.lon !== undefined
                        text: mapPreview.clickInfo.lon !== undefined
                              ? qsTr("经度 %1  纬度 %2  磁场 %3 nT")
                                .arg(Number(mapPreview.clickInfo.lon).toFixed(6))
                                .arg(Number(mapPreview.clickInfo.lat).toFixed(6))
                                .arg(Number(mapPreview.clickInfo.value).toFixed(2))
                              : ""
                        color: root._text
                        font.family: "Times New Roman"
                        font.pixelSize: 11
                    }
                }

            }
        }
    }

    DropArea {
        parent: importCard
        anchors.fill: parent
        onEntered: (drag) => {
            if (drag.hasUrls) importCard.border.color = root._cyan
        }
        onExited: importCard.border.color = root._border
        onDropped: (drop) => {
            importCard.border.color = root._border
            if (drop.hasUrls && drop.urls.length > 0) {
                root.importPath(drop.urls[0].toString())
            }
        }
    }

    Dialog {
        id: stopRecordingDialog
        parent: Overlay.overlay
        title: qsTr("停止录制")
        modal: true
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: root.sensor.saveStop()
        contentItem: QGCLabel {
            width: ScreenTools.defaultFontPixelWidth * 24
            wrapMode: Text.WordWrap
            text: qsTr("停止后当前 CSV 将完成写入并关闭。确认停止录制？")
        }
    }

    QGCFileDialog {
        id: csvDialog
        title: qsTr("选择测量数据")
        folder: root.sensor.dataDirectory
        nameFilters: [qsTr("测量数据 (*.txt *.csv)")]
        onAcceptedForLoad: (file) => root.importPath(file)
    }
}
