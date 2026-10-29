import QtQuick              2.15
import QtQuick.Controls     2.15
import QtQuick.Layouts      1.2
import QtCharts             2.15

import QGroundControl               1.0
import QGroundControl.Palette       1.0
import QGroundControl.FactSystem    1.0
import QGroundControl.FactControls  1.0
import QGroundControl.Controls      1.0
import QGroundControl.ScreenTools   1.0
import QGroundControl.Controllers   1.0
import SimpleTest 1.0

// 注意：AnalyzePage 把 pageComponent 暴露为 pageLoader.sourceComponent，
// 页面内容必须写在下面的 Component 里。原来直接写 pageComponent: pageComponent
// 是自引用，Loader 取不到任何东西，内容会挂到 AnalyzePage 外层并压住标题区。
AnalyzePage {
    id: mavlinkConsolePage

    // 采样率 20 Hz；横轴为墙钟时间（hh:mm:ss），窗口 5s/10s/20s 可切换
    property int samplingRate: 20
    property int timeWindow:   10
    property int currentChart: 0
    property var dataWindows: []
    property var dataWindows1: []
    property var dataWindows2: []
    property int flushTick: 0

    function setTimeWindow(seconds) {
        timeWindow = seconds
        // 时间窗缩小后 JS 侧窗口也要一起裁，否则纵轴仍按旧极值缩放
        trimWindow(dataWindows)
        trimWindow(dataWindows1)
        trimWindow(dataWindows2)
        var now = Date.now()
        updateXAxis(now)
        trimSeries(series)
        trimSeries(series1)
        trimSeries(series2)
    }

    function maxPoints() {
        return timeWindow * samplingRate
    }

    function trimWindow(arr) {
        while (arr.length > maxPoints()) {
            arr.shift()
        }
    }

    function trimSeries(s) {
        while (s.count > maxPoints()) {
            s.remove(0)
        }
    }

    function updateXAxis(now) {
        var lo = now - timeWindow * 1000
        var loDate = new Date(lo)
        var nowDate = new Date(now)
        axisX.min = loDate;  axisX.max = nowDate
        axisX1.min = loDate; axisX1.max = nowDate
        axisX2.min = loDate; axisX2.max = nowDate
    }

    function updateYAxis(axis, arr) {
        var mm = safeMinMax(arr)
        var margin = (mm.max - mm.min) * 0.1
        if (margin < 1) margin = 1
        axis.min = mm.min - margin
        axis.max = mm.max + margin
    }

    function safeMinMax(arr) {
        if (!arr || arr.length === 0) return {min: 0, max: 0}
        var valid = []
        for (var i = 0; i < arr.length; i++) {
            var v = Number(arr[i])
            if (!isNaN(v) && isFinite(v)) valid.push(v)
        }
        if (valid.length === 0) return {min: 0, max: 0}
        return {min: Math.min.apply(null, valid), max: Math.max.apply(null, valid)}
    }

    Component {
        id: pageComponent

        // 右侧控制按钮
        ColumnLayout {
            id: rightColumn
            width: parent.width / 7
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            spacing: 6

            // autoExclusive：checkable+checked 绑定在用户点击后会被破坏，
            // 没有 ButtonGroup 时会出现两个按钮同时高亮、与 currentChart 不一致。
            ButtonGroup {
                id: chartGroup
                exclusive: true
            }

            QGCButton {
                id: button1
                text: "光泵一"
                Layout.fillWidth: true
                Layout.fillHeight: true
                checkable: true
                ButtonGroup.group: chartGroup
                checked: currentChart === 0
                autoExclusive: true
                onClicked: currentChart = 0
                pointSize: 16
                backRadius: 6
            }
            QGCButton {
                id: button2
                text: "光泵二"
                Layout.fillWidth: true
                Layout.fillHeight: true
                checkable: true
                ButtonGroup.group: chartGroup
                checked: currentChart === 1
                autoExclusive: true
                onClicked: currentChart = 1
                pointSize: 16
                backRadius: 6
            }
            QGCButton {
                id: button3
                text: "差分"
                Layout.fillWidth: true
                Layout.fillHeight: true
                checkable: true
                ButtonGroup.group: chartGroup
                checked: currentChart === 2
                autoExclusive: true
                onClicked: currentChart = 2
                pointSize: 16
                backRadius: 6
            }
            QGCButton {
                id: button4
                text: SimpleTest.saveFlag ? "停止保存" : "开始保存"
                Layout.fillWidth: true
                Layout.fillHeight: true
                checkable: true
                checked: SimpleTest.saveFlag
                onClicked: SimpleTest.saveFlag ? SimpleTest.saveStop() : SimpleTest.saveStart()
                pointSize: 16
                backRadius: 6
                primary: SimpleTest.saveFlag
            }

            // 时间窗口切换
            QGCLabel {
                Layout.fillWidth: true
                text: "时间窗口"
                horizontalAlignment: Text.AlignHCenter
                color: "white"
                font.pointSize: 11
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                QGCButton {
                    text: "5s"
                    Layout.fillWidth: true
                    checkable: true
                    checked: timeWindow === 5
                    onClicked: setTimeWindow(5)
                    pointSize: 12
                    backRadius: 5
                }
                QGCButton {
                    text: "10s"
                    Layout.fillWidth: true
                    checkable: true
                    checked: timeWindow === 10
                    onClicked: setTimeWindow(10)
                    pointSize: 12
                    backRadius: 5
                }
                QGCButton {
                    text: "20s"
                    Layout.fillWidth: true
                    checkable: true
                    checked: timeWindow === 20
                    onClicked: setTimeWindow(20)
                    pointSize: 12
                    backRadius: 5
                }
            }
        }

        ChartView {
            id: chart1
            width: parent.width * 6 / 7
            height: parent.height
            anchors.top: parent.top
            anchors.left: parent.left
            visible: currentChart === 0
            antialiasing: false
            title: "光泵一"
            titleFont.pointSize: 16
            titleFont.family: "Times New Roman"

            DateTimeAxis {
                id: axisX
                min: new Date(Date.now() - timeWindow * 1000)
                max: new Date(Date.now())
                format: "hh:mm:ss"
                tickCount: 5
                titleText: "Time"
                titleFont.pointSize: 14
                titleFont.family: "Times New Roman"
                labelsFont.pointSize: 12
                labelsFont.family: "Times New Roman"
                gridVisible: true
                gridLineColor: "#e5e9f0"
            }
            ValueAxis {
                id: axisY
                min: -10
                max: 10
                titleText: "B (nT)"
                titleFont.pointSize: 14
                titleFont.family: "Times New Roman"
                labelsFont.pointSize: 12
                labelsFont.family: "Times New Roman"
                gridVisible: true
                gridLineColor: "#e5e9f0"
            }
            LineSeries {
                id: series
                name: "光泵一数据"
                axisX: axisX
                axisY: axisY
                color: "#2f78c4"
                width: 2
            }
            legend.font.pointSize: 14
        }

        ChartView {
            id: chart2
            width: parent.width * 6 / 7
            height: parent.height
            anchors.top: parent.top
            anchors.left: parent.left
            visible: currentChart === 1
            antialiasing: false
            title: "光泵二"
            titleFont.pointSize: 16
            titleFont.family: "Times New Roman"

            DateTimeAxis {
                id: axisX1
                min: new Date(Date.now() - timeWindow * 1000)
                max: new Date(Date.now())
                format: "hh:mm:ss"
                tickCount: 5
                titleText: "Time"
                titleFont.pointSize: 14
                titleFont.family: "Times New Roman"
                labelsFont.pointSize: 12
                labelsFont.family: "Times New Roman"
                gridVisible: true
                gridLineColor: "#e5e9f0"
            }
            ValueAxis {
                id: axisY1
                min: -10
                max: 10
                titleText: "B (nT)"
                titleFont.pointSize: 14
                titleFont.family: "Times New Roman"
                labelsFont.pointSize: 12
                labelsFont.family: "Times New Roman"
                gridVisible: true
                gridLineColor: "#e5e9f0"
            }
            LineSeries {
                id: series1
                name: "光泵二数据"
                axisX: axisX1
                axisY: axisY1
                color: "#e8890c"
                width: 2
            }
            legend.font.pointSize: 14
        }

        ChartView {
            id: chart3
            width: parent.width * 6 / 7
            height: parent.height
            anchors.top: parent.top
            anchors.left: parent.left
            visible: currentChart === 2
            antialiasing: false
            title: "差分"
            titleFont.pointSize: 16
            titleFont.family: "Times New Roman"

            DateTimeAxis {
                id: axisX2
                min: new Date(Date.now() - timeWindow * 1000)
                max: new Date(Date.now())
                format: "hh:mm:ss"
                tickCount: 5
                titleText: "Time"
                titleFont.pointSize: 14
                titleFont.family: "Times New Roman"
                labelsFont.pointSize: 12
                labelsFont.family: "Times New Roman"
                gridVisible: true
                gridLineColor: "#e5e9f0"
            }
            ValueAxis {
                id: axisY2
                min: -10
                max: 10
                titleText: "B (nT)"
                titleFont.pointSize: 14
                titleFont.family: "Times New Roman"
                labelsFont.pointSize: 12
                labelsFont.family: "Times New Roman"
                gridVisible: true
                gridLineColor: "#e5e9f0"
            }
            LineSeries {
                id: series2
                name: "差分数据"
                axisX: axisX2
                axisY: axisY2
                color: "#2e9d6e"
                width: 2
            }
            legend.font.pointSize: 14
        }

        // 采样暂存：UDP 每帧只做数组 push，图表更新交给下面的 5 Hz 定时器。
        // 原来每帧都要 append 3 个 series、重算 3 次窗口极值、写 12 次坐标轴属性
        // （20 Hz 下约 240 次/秒的图表重排），是界面卡顿的主因。
        property var pendingTime:  []
        property var pendingV1:    []
        property var pendingV2:    []
        property var pendingDiff:  []

        function appendWindow(arr, value) {
            arr.push(value)
            var excess = arr.length - maxPoints()
            if (excess > 0) arr.splice(0, excess)
        }

        function flushCharts() {
            if (pendingTime.length === 0) return

            var n     = pendingTime.length
            var first = pendingTime[0]
            var last  = pendingTime[n - 1]

            // 只更新当前可见的那一个图，隐藏的图不再白算
            if (currentChart === 0) {
                series.append(first, pendingV1[0])
                for (var i = 1; i < n; i++) series.append(pendingTime[i], pendingV1[i])
                for (var a = 0; a < n; a++) appendWindow(dataWindows, pendingV1[a])
                trimSeries(series)
            } else if (currentChart === 1) {
                series1.append(first, pendingV2[0])
                for (var j = 1; j < n; j++) series1.append(pendingTime[j], pendingV2[j])
                for (var b = 0; b < n; b++) appendWindow(dataWindows1, pendingV2[b])
                trimSeries(series1)
            } else if (currentChart === 2) {
                series2.append(first, pendingDiff[0])
                for (var k = 1; k < n; k++) series2.append(pendingTime[k], pendingDiff[k])
                for (var c = 0; c < n; c++) appendWindow(dataWindows2, pendingDiff[c])
                trimSeries(series2)
            }

            pendingTime.length = 0
            pendingV1.length   = 0
            pendingV2.length   = 0
            pendingDiff.length = 0

            updateXAxis(last)

            // 纵轴每约 1 秒重算一次即可，避免曲线随每帧缩放"呼吸"
            if ((flushTick % 5) === 0) {
                if (currentChart === 0) updateYAxis(axisY,  dataWindows)
                if (currentChart === 1) updateYAxis(axisY1, dataWindows1)
                if (currentChart === 2) updateYAxis(axisY2, dataWindows2)
            }
            flushTick++
        }

        Timer {
            interval: 200        // 5 Hz
            running: true
            repeat: true
            onTriggered: flushCharts()
        }

        Connections {
            target: SimpleTest
            function onScalar1_changed() {
                // 这一层只做最便宜的缓存，任何坐标轴/图表操作都推迟到 flushCharts()
                var now = Date.now()
                var v1 = SimpleTest.scalar1
                var v2 = SimpleTest.scalar2
                pendingTime.push(now)
                pendingV1.push(v1)
                pendingV2.push(v2)
                pendingDiff.push(v2 - v1)
            }
        }
    }
}