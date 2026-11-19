import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGC
import QGroundControl
import QGroundControl.Controls

Rectangle {
    id: root
    color: qgcPal.window

    QGCPalette { id: qgcPal; colorGroupEnabled: true }
    readonly property var config: MagSurveySettings
    readonly property var sensor: SimpleTest

    readonly property color _ink: "#102A43"
    readonly property color _muted: "#627D98"
    readonly property color _line: "#D9E2EC"
    readonly property color _soft: "#F5F8FB"
    readonly property color _teal: "#008C9E"
    readonly property color _tealSoft: "#E5F7F8"
    readonly property color _green: "#1E9E6A"
    readonly property color _amber: "#B7791F"
    readonly property color _red: "#C53030"
    readonly property real _gap: ScreenTools.defaultFontPixelWidth * 0.9
    readonly property real _pad: ScreenTools.defaultFontPixelWidth * 1.1
    readonly property real _radius: ScreenTools.defaultFontPixelWidth * 0.9
    readonly property bool wide: width >= 850

    property int draftMode: 0
    property int draftTransport: 0
    property string draftLocalAddress: "0.0.0.0"
    property int draftLocalPort: 14551
    property string draftRemoteAddress: "127.0.0.1"
    property int draftRemotePort: 14551
    property int draftInterval: 1000
    property bool draftHeartbeat: true
    property bool draftAutoCollect: true
    property bool draftAutoSave: false
    property string applyStatus: ""
    property string applyError: ""

    readonly property bool dirty: draftMode !== config.mode ||
                                  draftTransport !== config.transport ||
                                  draftLocalAddress !== config.localAddress ||
                                  draftLocalPort !== config.localPort ||
                                  draftRemoteAddress !== config.remoteAddress ||
                                  draftRemotePort !== config.remotePort ||
                                  draftInterval !== config.requestIntervalMs ||
                                  draftHeartbeat !== config.heartbeatEnabled ||
                                  draftAutoCollect !== config.autoCollect ||
                                  draftAutoSave !== config.autoSave

    // Scalar now supports an optional F8 heartbeat as well.
    readonly property bool heartbeatRelevant: true
    property bool showAI: false
    readonly property bool sensorOnline: sensor.lastSampleMs > 0 &&
                                         (Date.now() - sensor.lastSampleMs) < 3000
    readonly property bool hasError: sensor.errorString.length > 0

    Component.onCompleted: syncDraft()

    function syncDraft() {
        draftMode = config.mode
        draftTransport = config.transport
        draftLocalAddress = config.localAddress
        draftLocalPort = config.localPort
        draftRemoteAddress = config.remoteAddress
        draftRemotePort = config.remotePort
        draftInterval = config.requestIntervalMs
        draftHeartbeat = config.heartbeatEnabled
        draftAutoCollect = config.autoCollect
        draftAutoSave = config.autoSave
        applyStatus = ""
        applyError = ""
    }

    function selectMode(mode) {
        if (draftMode === mode) return
        draftMode = mode
        // Match the C++ defaults while preserving explicitly edited ports.
        if (mode === 1) {
            if (Number(draftLocalPort) === 14551) draftLocalPort = 13551
            if (Number(draftRemotePort) === 14551) draftRemotePort = 13551
        } else {
            if (Number(draftLocalPort) === 13551) draftLocalPort = 14551
            if (Number(draftRemotePort) === 13551) draftRemotePort = 14551
        }
    }

    function applyDraft() {
        if (sensor.saveFlag) {
            applyError = qsTr("正在录制数据，请先停止录制后再应用通信配置。")
            applyStatus = ""
            return
        }
        const localPort = Math.max(1, parseInt(draftLocalPort) || 1)
        const remotePort = Math.max(1, parseInt(draftRemotePort) || 1)
        const interval = Math.max(100, parseInt(draftInterval) || 1000)
        config.applyConfiguration(draftMode, draftTransport,
                                  draftLocalAddress.trim(), localPort,
                                  draftRemoteAddress.trim(), remotePort,
                                  interval, draftHeartbeat,
                                  draftAutoCollect, draftAutoSave)
        if (!draftAutoCollect) sensor.stopCollection()
        syncDraft()
        applyStatus = qsTr("配置已保存并应用")
    }

    function statusColor(ok, warning) {
        if (warning) return _amber
        return ok ? _green : _muted
    }

    function formatNumber(value, digits) {
        return isFinite(value) ? Number(value).toFixed(digits) : "—"
    }

    component Card: Rectangle {
        id: card
        default property alias content: cardColumn.data
        Layout.fillWidth: true
        implicitHeight: cardColumn.implicitHeight + root._pad * 2
        color: "#FFFFFF"
        radius: root._radius
        border.width: 1
        border.color: root._line
        ColumnLayout {
            id: cardColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: root._pad
            spacing: root._gap
        }
    }

    component SectionTitle: ColumnLayout {
        property string title
        property string subtitle: ""
        Layout.fillWidth: true
        spacing: root._gap * 0.22
        QGCLabel {
            text: title
            color: root._ink
            font.bold: true
            font.pointSize: ScreenTools.mediumFontPointSize
        }
        QGCLabel {
            Layout.fillWidth: true
            visible: subtitle.length > 0
            text: subtitle
            color: root._muted
            wrapMode: Text.WordWrap
            font.pointSize: ScreenTools.smallFontPointSize
        }
    }

    component Field: ColumnLayout {
        property string label
        property string hint: ""
        default property alias field: fieldHost.data
        Layout.fillWidth: true
        spacing: root._gap * 0.25
        QGCLabel {
            text: label
            color: root._ink
            font.pointSize: ScreenTools.smallFontPointSize
        }
        Item {
            id: fieldHost
            Layout.fillWidth: true
            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.9
        }
        QGCLabel {
            Layout.fillWidth: true
            visible: hint.length > 0
            text: hint
            color: root._muted
            wrapMode: Text.WordWrap
            font.pointSize: ScreenTools.smallFontPointSize * 0.86
        }
    }

    component Pill: Rectangle {
        property string label
        property string value
        property color tone: root._muted
        Layout.preferredHeight: pillColumn.implicitHeight + root._gap * 0.55
        Layout.preferredWidth: pillColumn.implicitWidth + root._gap * 1.2
        radius: height / 2
        color: Qt.rgba(tone.r, tone.g, tone.b, 0.10)
        border.width: 1
        border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.22)
        ColumnLayout {
            id: pillColumn
            anchors.centerIn: parent
            spacing: 0
            QGCLabel {
                text: label
                color: root._muted
                font.pointSize: ScreenTools.smallFontPointSize * 0.78
            }
            QGCLabel {
                text: value
                color: tone
                font.bold: true
                font.pointSize: ScreenTools.smallFontPointSize * 0.9
            }
        }
    }

    component ModeCard: Rectangle {
        id: modeCard
        property bool selected: false
        property string title
        property string subtitle
        signal clicked()
        Layout.fillWidth: true
        Layout.preferredHeight: modeColumn.implicitHeight + root._gap * 1.4
        radius: root._radius
        color: selected ? root._tealSoft : root._soft
        border.width: selected ? 2 : 1
        border.color: selected ? root._teal : root._line
        ColumnLayout {
            id: modeColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: root._gap
            spacing: root._gap * 0.35
            RowLayout {
                Layout.fillWidth: true
                spacing: root._gap * 0.5
                Rectangle {
                    Layout.preferredWidth: ScreenTools.defaultFontPixelHeight * 1.1
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.1
                    radius: width / 2
                    color: "transparent"
                    border.width: 2
                    border.color: modeCard.selected ? root._teal : root._line
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.48
                        height: width
                        radius: width / 2
                        color: root._teal
                        visible: modeCard.selected
                    }
                }
                QGCLabel {
                    text: modeCard.title
                    color: root._ink
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
                Item { Layout.fillWidth: true }
                QGCLabel {
                    text: modeCard.selected ? qsTr("当前选择") : qsTr("选择")
                    color: modeCard.selected ? root._teal : root._muted
                    font.bold: modeCard.selected
                    font.pointSize: ScreenTools.smallFontPointSize * 0.85
                }
            }
            QGCLabel {
                Layout.fillWidth: true
                text: modeCard.subtitle
                color: root._muted
                wrapMode: Text.WordWrap
                font.pointSize: ScreenTools.smallFontPointSize
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: modeCard.clicked()
        }
    }

    component TransportSelector: Rectangle {
        id: selector
        property int currentIndex: 0
        signal selected(int index)
        readonly property var labels: [qsTr("UDP"), qsTr("TCP Client"), qsTr("TCP Server")]
        Layout.fillWidth: true
        implicitHeight: ScreenTools.defaultFontPixelHeight * 2.25
        radius: height / 2
        color: root._soft
        border.width: 1
        border.color: root._line

        Rectangle {
            id: thumb
            width: selector.width / selector.labels.length
            height: selector.height - 4
            y: 2
            x: 2 + selector.currentIndex * width
            radius: height / 2
            color: root._teal
            Behavior on x { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }
        Row {
            anchors.fill: parent
            Repeater {
                model: selector.labels
                Item {
                    width: selector.width / selector.labels.length
                    height: selector.height
                    QGCLabel {
                        anchors.centerIn: parent
                        text: modelData
                        color: index === selector.currentIndex ? "#FFFFFF" : root._ink
                        font.bold: index === selector.currentIndex
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: selector.selected(index)
                    }
                }
            }
        }
    }

    component SmallSwitch: RowLayout {
        id: smallSwitch
        property string label
        property bool checked: false
        property bool switchEnabled: true
        signal toggled(bool checked)
        spacing: root._gap * 0.45
        QGCLabel {
            Layout.fillWidth: true
            text: parent.label
            color: parent.switchEnabled ? root._ink : root._muted
            font.pointSize: ScreenTools.smallFontPointSize
        }
        Switch {
            id: switchControl
            checked: parent.checked
            enabled: parent.switchEnabled
            onToggled: parent.toggled(checked)
        }
        Connections {
            target: smallSwitch
            function onCheckedChanged() {
                switchControl.checked = smallSwitch.checked
            }
        }
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.implicitHeight + root._pad * 2
        clip: true
        flickableDirection: Flickable.VerticalFlick

        ColumnLayout {
            id: content
            x: root._pad
            y: root._pad
            width: scroller.width - root._pad * 2
            spacing: root._gap

            // Header
            Card {
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: headerRow.implicitHeight + root._gap * 1.2
                    radius: root._radius * 0.9
                    color: root._tealSoft
                    border.width: 1
                    border.color: root._line
                    RowLayout {
                        id: headerRow
                        anchors.fill: parent
                        anchors.margins: root._gap
                        spacing: root._gap
                        Image {
                            Layout.preferredWidth: ScreenTools.defaultFontPixelHeight * 3.2
                            Layout.preferredHeight: Layout.preferredWidth
                            source: "qrc:/maglab/heu.svg"
                            fillMode: Image.PreserveAspectFit
                            opacity: 0.95
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: root._gap * 0.2
                            QGCLabel {
                                text: qsTr("航磁设置")
                                color: root._ink
                                font.bold: true
                                font.pointSize: ScreenTools.largeFontPointSize
                            }
                            QGCLabel {
                                Layout.fillWidth: true
                                text: qsTr("磁测设备、通信、数据记录与 AI 辅助配置")
                                color: root._muted
                                font.pointSize: ScreenTools.smallFontPointSize
                            }
                        }
                        Pill {
                            label: qsTr("传感器")
                            value: root.sensorOnline ? qsTr("在线") : qsTr("离线")
                            tone: root.sensorOnline ? root._green : root._muted
                        }
                        Pill {
                            label: qsTr("链路")
                            value: root.draftTransport === 0 ? "UDP"
                                   : root.draftTransport === 1 ? "TCP Client" : "TCP Server"
                            tone: root._teal
                        }
                    }
                }
                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("标量与矢量使用独立的协议解析和存储目录；修改参数后点击下方“应用并保存”。")
                    color: root._muted
                    wrapMode: Text.WordWrap
                    font.pointSize: ScreenTools.smallFontPointSize
                }
            }

            // Mode selection
            Card {
                SectionTitle {
                    title: qsTr("航磁设备")
                    subtitle: qsTr("选择当前接入的探杆协议。矢量模式保留双磁通门 XYZ，标量模式沿用原有 S1/S2 协议。")
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: root.wide ? 2 : 1
                    columnSpacing: root._gap
                    rowSpacing: root._gap
                    ModeCard {
                        title: qsTr("标量 Scalar")
                        subtitle: qsTr("光泵 S1 / S2 标量读数，24 字段 UU 协议；默认 UDP 14551。")
                        selected: root.draftMode === 0
                        onClicked: root.selectMode(0)
                    }
                    ModeCard {
                        title: qsTr("矢量 Vector")
                        subtitle: qsTr("双磁通门 FG1/FG2 XYZ 与姿态，25 字段 UU 协议；默认 UDP 13551，需要持续 F8 心跳。")
                        selected: root.draftMode === 1
                        onClicked: root.selectMode(1)
                    }
                }
            }

            // Communication
            Card {
                SectionTitle {
                    title: qsTr("通信配置")
                    subtitle: qsTr("参考 QGC 通信连接：支持 UDP、TCP 客户端和 TCP 服务端；矢量及可选标量心跳按下方频率发送 F8。")
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: root._gap * 0.5
                    QGCLabel {
                        text: qsTr("通信方式")
                        color: root._ink
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    TransportSelector {
                        Layout.fillWidth: true
                        currentIndex: root.draftTransport
                        onSelected: (index) => root.draftTransport = index
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.wide ? 2 : 1
                    columnSpacing: root._gap
                    rowSpacing: root._gap
                    Field {
                        visible: root.draftTransport !== 1
                        label: qsTr("本地监听 IP")
                        hint: root.draftTransport === 2
                              ? qsTr("TCP 服务端绑定地址；0.0.0.0 表示所有网卡")
                              : root.draftTransport === 1
                                ? qsTr("TCP 客户端可选本地绑定地址")
                                : qsTr("UDP 通常使用 0.0.0.0 监听所有网卡")
                        QGCTextField {
                            anchors.fill: parent
                            text: root.draftLocalAddress
                            placeholderText: "0.0.0.0"
                            onEditingFinished: root.draftLocalAddress = text.trim()
                        }
                    }
                    Field {
                        visible: root.draftTransport !== 1
                        label: qsTr("本地端口")
                        hint: root.draftMode === 1 ? qsTr("矢量默认 13551") : qsTr("标量默认 14551")
                        QGCTextField {
                            anchors.fill: parent
                            text: root.draftLocalPort
                            inputMethodHints: Qt.ImhDigitsOnly
                            onEditingFinished: root.draftLocalPort = Math.max(1, parseInt(text) || 1)
                        }
                    }
                    Field {
                        visible: root.draftTransport !== 2
                        label: qsTr("远程服务器 IP")
                        hint: qsTr("TCP 客户端/探杆转发器地址；模拟器本机测试使用 127.0.0.1")
                        QGCTextField {
                            anchors.fill: parent
                            text: root.draftRemoteAddress
                            placeholderText: "127.0.0.1"
                            onEditingFinished: root.draftRemoteAddress = text.trim()
                        }
                    }
                    Field {
                        visible: root.draftTransport !== 2
                        label: qsTr("远程端口")
                        hint: qsTr("TCP 客户端/UDP 目标端口；TCP 服务端使用连接的对端地址")
                        QGCTextField {
                            anchors.fill: parent
                            text: root.draftRemotePort
                            inputMethodHints: Qt.ImhDigitsOnly
                            onEditingFinished: root.draftRemotePort = Math.max(1, parseInt(text) || 1)
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: heartbeatColumn.implicitHeight + root._gap * 1.1
                    radius: root._radius * 0.75
                    color: root._soft
                    ColumnLayout {
                        id: heartbeatColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: root._gap
                        spacing: root._gap * 0.25
                        SmallSwitch {
                            Layout.fillWidth: true
                            label: qsTr("心跳请求")
                            checked: root.draftHeartbeat
                            switchEnabled: root.heartbeatRelevant
                            onToggled: (value) => root.draftHeartbeat = value
                        }
                        QGCLabel {
                            Layout.fillWidth: true
                            text: root.heartbeatRelevant
                                  ? qsTr("矢量模式必须持续发送 F8；标量模式也可以按需开启 F8 心跳。")
                                  : qsTr("当前已关闭心跳；矢量模式通常需要开启，标量模式按设备协议决定。")
                            color: root._muted
                            wrapMode: Text.WordWrap
                            font.pointSize: ScreenTools.smallFontPointSize * 0.86
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: root._gap * 0.4
                            QGCLabel {
                                text: qsTr("请求频率 (ms)")
                                color: root._ink
                                font.pointSize: ScreenTools.smallFontPointSize
                            }
                            QGCTextField {
                                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 10
                                text: root.draftInterval
                                enabled: root.heartbeatRelevant
                                inputMethodHints: Qt.ImhDigitsOnly
                                onEditingFinished: root.draftInterval = Math.max(100, parseInt(text) || 1000)
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }

            // Data configuration
            Card {
                SectionTitle {
                    title: qsTr("数据配置")
                    subtitle: qsTr("协议字段和记录策略。矢量数据会保留原始 XYZ，并计算两路幅值差用于磁图。")
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: root._gap * 0.45
                    Rectangle {
                        height: tagLabel.implicitHeight + root._gap * 0.55
                        width: tagLabel.implicitWidth + root._gap * 1.2
                        radius: height / 2
                        color: root.draftMode === 1 ? root._tealSoft : "#EEF4F8"
                        QGCLabel {
                            id: tagLabel
                            anchors.centerIn: parent
                            text: root.draftMode === 1 ? qsTr("FG1 / FG2 双磁通门") : qsTr("S1 / S2 标量")
                            color: root._teal
                            font.pointSize: ScreenTools.smallFontPointSize * 0.86
                        }
                    }
                    Rectangle {
                        height: tagLabel2.implicitHeight + root._gap * 0.55
                        width: tagLabel2.implicitWidth + root._gap * 1.2
                        radius: height / 2
                        color: "#EEF4F8"
                        QGCLabel {
                            id: tagLabel2
                            anchors.centerIn: parent
                            text: root.draftMode === 1 ? qsTr("|B1| − |B2| 磁图值") : qsTr("S1 − S2 磁图值")
                            color: root._muted
                            font.pointSize: ScreenTools.smallFontPointSize * 0.86
                        }
                    }
                    Rectangle {
                        height: tagLabel3.implicitHeight + root._gap * 0.55
                        width: tagLabel3.implicitWidth + root._gap * 1.2
                        radius: height / 2
                        color: "#EEF4F8"
                        QGCLabel {
                            id: tagLabel3
                            anchors.centerIn: parent
                            text: root.draftMode === 1 ? qsTr("GPS + 姿态 + 激光高度") : qsTr("GPS + 姿态")
                            color: root._muted
                            font.pointSize: ScreenTools.smallFontPointSize * 0.86
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: liveColumn.implicitHeight + root._gap * 1.1
                    radius: root._radius * 0.75
                    color: root._soft
                    ColumnLayout {
                        id: liveColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: root._gap
                        spacing: root._gap * 0.25
                        QGCLabel {
                            text: qsTr("实时数据")
                            color: root._ink
                            font.bold: true
                            font.pointSize: ScreenTools.smallFontPointSize
                        }
                        QGCLabel {
                            Layout.fillWidth: true
                            text: root.draftMode === 1
                                  ? qsTr("FG1 X %1  Y %2  Z %3  ·  FG2 X %4  Y %5  Z %6")
                                    .arg(root.formatNumber(sensor.fg1x, 1))
                                    .arg(root.formatNumber(sensor.fg1y, 1))
                                    .arg(root.formatNumber(sensor.fg1z, 1))
                                    .arg(root.formatNumber(sensor.fg2x, 1))
                                    .arg(root.formatNumber(sensor.fg2y, 1))
                                    .arg(root.formatNumber(sensor.fg2z, 1))
                                  : qsTr("S1 %1 nT  ·  S2 %2 nT  ·  Δ %3 nT")
                                    .arg(root.formatNumber(sensor.scalar1, 3))
                                    .arg(root.formatNumber(sensor.scalar2, 3))
                                    .arg(root.formatNumber(sensor.scalar1 - sensor.scalar2, 3))
                            color: root._muted
                            wrapMode: Text.WordWrap
                            font.pointSize: ScreenTools.smallFontPointSize * 0.86
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.wide ? 2 : 1
                    columnSpacing: root._gap
                    rowSpacing: root._gap * 0.5
                    SmallSwitch {
                        Layout.fillWidth: true
                        label: qsTr("自动采集")
                        checked: root.draftAutoCollect
                        onToggled: (value) => {
                            root.draftAutoCollect = value
                            if (!value) root.draftAutoSave = false
                        }
                    }
                    SmallSwitch {
                        Layout.fillWidth: true
                        label: qsTr("自动保存")
                        checked: root.draftAutoSave
                        switchEnabled: root.draftAutoCollect
                        onToggled: (value) => root.draftAutoSave = value
                    }
                }
            }

            // Storage
            Card {
                SectionTitle {
                    title: qsTr("数据存储")
                    subtitle: qsTr("Android 使用应用私有目录，不需要外部存储权限。")
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: root._gap
                    rowSpacing: root._gap * 0.35
                    QGCLabel {
                        text: qsTr("采集目录")
                        color: root._muted
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        text: root.sensor.dataDirectory || qsTr("存储不可用")
                        color: root._ink
                        elide: Text.ElideMiddle
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    QGCLabel {
                        text: qsTr("录制状态")
                        color: root._muted
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        text: root.sensor.saveFlag ? qsTr("正在写入 CSV") : qsTr("待机")
                        color: root.sensor.saveFlag ? root._red : root._green
                        font.bold: true
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                }
            }

            // Diagnostics
            Card {
                SectionTitle {
                    title: qsTr("设备诊断")
                    subtitle: qsTr("只读状态，用于确认监听、心跳和数据回传是否正常。")
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: root.wide ? 4 : 2
                    columnSpacing: root._gap * 0.5
                    rowSpacing: root._gap * 0.5
                    Pill {
                        label: qsTr("连接")
                        value: root.sensor.connectionState
                        tone: root.sensor.udpListening ? root._green : root._amber
                    }
                    Pill {
                        label: qsTr("数据包")
                        value: String(root.sensor.packetCount)
                        tone: root.sensor.packetCount > 0 ? root._teal : root._muted
                    }
                    Pill {
                        label: qsTr("采样")
                        value: String(root.sensor.sampleCount)
                        tone: root.sensor.sampleCount > 0 ? root._teal : root._muted
                    }
                    Pill {
                        label: qsTr("接收频率")
                        value: root.formatNumber(root.sensor.sampleRateHz, 1) + " Hz"
                        tone: root.sensor.sampleRateHz > 0 ? root._green : root._muted
                    }
                    Pill {
                        label: qsTr("心跳")
                        value: String(root.sensor.heartbeatCount)
                        tone: root.sensor.heartbeatCount > 0 ? root._green : root._amber
                    }
                }
                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("最近数据：%1  ·  最近心跳：%2")
                          .arg(root.sensor.lastSampleMs > 0 ? Qt.formatDateTime(new Date(root.sensor.lastSampleMs), "HH:mm:ss") : "—")
                          .arg(root.sensor.lastHeartbeatMs > 0 ? Qt.formatDateTime(new Date(root.sensor.lastHeartbeatMs), "HH:mm:ss") : "—")
                    color: root._muted
                    font.pointSize: ScreenTools.smallFontPointSize * 0.86
                }
                QGCLabel {
                    Layout.fillWidth: true
                    visible: !root.sensor.udpListening
                    text: qsTr("通信异常：当前没有可用的监听/连接，请检查端口、网段或远程 IP。")
                    color: root._red
                    wrapMode: Text.WordWrap
                    font.pointSize: ScreenTools.smallFontPointSize
                }
                QGCLabel {
                    Layout.fillWidth: true
                    visible: root.hasError
                    text: root.sensor.errorString
                    color: root._red
                    wrapMode: Text.WordWrap
                    font.pointSize: ScreenTools.smallFontPointSize
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: root._gap * 0.5
                    QGCButton {
                        text: qsTr("立即连接")
                        enabled: !root.sensor.saveFlag
                        onClicked: root.sensor.startCollection()
                    }
                    QGCButton {
                        text: qsTr("断开采集")
                        enabled: root.sensor.udpListening && !root.sensor.saveFlag
                        onClicked: root.sensor.stopCollection()
                    }
                    Item { Layout.fillWidth: true }
                }
                QGCLabel {
                    Layout.fillWidth: true
                    visible: !root.draftHeartbeat
                    text: qsTr("警告：关闭心跳后不会发送 F8；若探杆依赖请求回传，将没有数据。")
                    color: root._amber
                    wrapMode: Text.WordWrap
                    font.pointSize: ScreenTools.smallFontPointSize
                }
            }

            // AI / voice settings are part of the same MagLab settings page.
            Card {
                SectionTitle {
                    title: qsTr("AI 与语音设置")
                    subtitle: qsTr("LLM 航线规划、语音识别和系统提示词；与航磁采集设置统一管理。")
                }
                RowLayout {
                    Layout.fillWidth: true
                    QGCLabel {
                        Layout.fillWidth: true
                        text: qsTr("AI 规划 / STT / 系统提示词")
                        color: root._ink
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    QGCButton {
                        text: root.showAI ? qsTr("收起") : qsTr("展开 AI 设置")
                        primary: root.showAI
                        onClicked: root.showAI = !root.showAI
                    }
                }
                Loader {
                    id: llmSettingsLoader
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.showAI ? ScreenTools.defaultFontPixelHeight * 44 : 0
                    clip: true
                    active: root.showAI
                    visible: root.showAI
                    source: active ? "qrc:/maglab/LLMSettingsPage.qml" : ""
                }
            }

            // Apply bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: applyRow.implicitHeight + root._gap * 1.2
                radius: root._radius
                color: "#FFFFFF"
                border.width: 1
                border.color: root.dirty ? root._teal : root._line
                RowLayout {
                    id: applyRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: root._gap
                    spacing: root._gap
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: root._gap * 0.15
                        QGCLabel {
                            text: root.applyError.length > 0 ? root.applyError
                                  : (root.applyStatus.length > 0 ? root.applyStatus
                                  : (root.dirty ? qsTr("有未应用的配置修改") : qsTr("配置已同步")))
                            color: root.applyError.length > 0 ? root._red
                                 : (root.dirty ? root._amber : root._green)
                            font.bold: root.dirty || root.applyError.length > 0
                            font.pointSize: ScreenTools.smallFontPointSize
                        }
                        QGCLabel {
                            text: qsTr("应用会持久化设置，并按新参数重新绑定采集端口。")
                            color: root._muted
                            font.pointSize: ScreenTools.smallFontPointSize * 0.82
                        }
                    }
                    QGCButton {
                        text: qsTr("恢复已保存")
                        enabled: root.dirty
                        onClicked: root.syncDraft()
                    }
                    QGCButton {
                        text: qsTr("应用并保存")
                        primary: true
                        enabled: root.dirty && !root.sensor.saveFlag
                        onClicked: root.applyDraft()
                    }
                }
            }
        }
    }
}
