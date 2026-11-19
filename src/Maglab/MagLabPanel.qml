import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGC
import QGroundControl
import QGroundControl.Controls

Popup {
    id: root
    parent: Overlay.overlay
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape

    readonly property color _navy: "#102A43"
    readonly property color _cyan: "#00A6B8"
    readonly property color _bg: "#F3F6FA"
    readonly property color _border: "#D9E2EC"
    readonly property color _muted: "#627D98"
    readonly property color _ok: "#2F9E7A"
    readonly property color _warn: "#D97706"
    readonly property real _margin: ScreenTools.defaultFontPixelWidth * 0.7
    readonly property var sensor: SimpleTest
    readonly property bool _narrow: width < 700

    signal magneticCurveVisibilityRequested(bool visible)
    property bool magneticCurveEnabled: false
    property int currentTab: 0
    property real _nowMs: Date.now()
    readonly property bool sensorOnline: sensor.lastSampleMs > 0 && (_nowMs - sensor.lastSampleMs) < 3000
    readonly property var moduleNames: [qsTr("实时数据"), qsTr("磁干扰补偿"), qsTr("磁图显示"), qsTr("数据存储"), qsTr("照片叠加")]
    readonly property string moduleName: moduleNames[currentTab] || ""

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: root._nowMs = Date.now()
    }

    function applyModule() {
        const page = pageLoader.item
        if (!page) return
        if (root.currentTab === 2) page.module = 1
        else if (root.currentTab === 3) page.module = 2
        if (page.scrollOffset !== undefined) page.scrollOffset = 0
    }

    onCurrentTabChanged: Qt.callLater(root.applyModule)
    background: Rectangle { color: root._bg }

    component DockButton: Rectangle {
        id: dockButton
        property int tabIndex
        property string label
        property string glyph
        readonly property bool selected: root.currentTab === tabIndex
        implicitWidth: 40
        implicitHeight: 40
        radius: 20
        color: selected ? "#0B6E7A" : (dockMouse.containsMouse ? "#EAF5F7" : "transparent")
        border.width: selected ? 1 : 0
        border.color: "#7FD8E4"
        Behavior on color { ColorAnimation { duration: 140 } }
        scale: dockMouse.pressed ? 0.93 : 1.0
        Behavior on scale { NumberAnimation { duration: 100 } }
        QGCLabel {
            anchors.centerIn: parent
            text: dockButton.glyph
            color: dockButton.selected ? "#FFFFFF" : root._navy
            font.pixelSize: 20
            font.bold: true
        }
        MouseArea {
            id: dockMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.currentTab = dockButton.tabIndex
        }
        ToolTip.visible: dockMouse.containsMouse
        ToolTip.text: dockButton.label
    }

    component StatusDot: RowLayout {
        property string label
        property bool ok: true
        spacing: 4
        Rectangle {
            Layout.preferredWidth: 6
            Layout.preferredHeight: 6
            radius: 3
            color: ok ? root._ok : root._warn
        }
        QGCLabel {
            text: label
            color: root._muted
            font.pointSize: ScreenTools.smallFontPointSize * 0.82
        }
    }

    contentItem: Item {
        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            spacing: 0

            // The old dark navigation bar is intentionally gone. This single
            // floating capsule is the primary navigation for all four modules.
            Rectangle {
                id: navigation
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                Layout.topMargin: 12
                implicitHeight: 56
                radius: 28
                color: Qt.rgba(1.0, 1.0, 1.0, 0.92)
                border.width: 1
                border.color: "#E6EEF4"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    Image {
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        source: "qrc:/maglab/heu.svg"
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }
                    ColumnLayout {
                        Layout.preferredWidth: root._narrow ? 74 : 120
                        spacing: 0
                        QGCLabel {
                            text: "MagLab"
                            color: root._navy
                            font.bold: true
                            font.pointSize: ScreenTools.mediumFontPointSize
                        }
                        QGCLabel {
                            text: root.moduleName
                            color: root._cyan
                            font.pointSize: ScreenTools.smallFontPointSize * 0.78
                            elide: Text.ElideRight
                        }
                    }

                    Item { Layout.preferredWidth: 4 }

                    Row {
                        spacing: 2
                        DockButton { tabIndex: 0; label: qsTr("实时数据"); glyph: "◉" }
                        DockButton { tabIndex: 1; label: qsTr("磁干扰补偿"); glyph: "◎" }
                        DockButton { tabIndex: 2; label: qsTr("磁图显示"); glyph: "▦" }
                        DockButton { tabIndex: 3; label: qsTr("数据存储"); glyph: "▤" }
                        DockButton { tabIndex: 4; label: qsTr("照片叠加"); glyph: "▧" }
                    }

                    Item { Layout.fillWidth: true }

                    Flow {
                        Layout.maximumWidth: root._narrow ? 0 : 320
                        visible: !root._narrow
                        spacing: 10
                        StatusDot { label: qsTr("传感器"); ok: root.sensorOnline }
                        StatusDot { label: qsTr("UDP"); ok: root.sensor.udpListening }
                        StatusDot { label: qsTr("存储"); ok: MagLabPaths.storageReady }
                    }

                    QGCLabel {
                        visible: !root._narrow
                        text: qsTr("曲线")
                        color: root._muted
                        font.pointSize: ScreenTools.smallFontPointSize * 0.82
                    }
                    Switch {
                        visible: !root._narrow
                        checked: root.magneticCurveEnabled
                        onToggled: {
                            root.magneticCurveEnabled = checked
                            root.magneticCurveVisibilityRequested(checked)
                        }
                    }
                    QGCButton {
                        text: qsTr("设置")
                        backgroundColor: "transparent"
                        textColor: root._navy
                        onClicked: {
                            root.close()
                            mainWindow.showSettingsTool()
                        }
                    }
                    QGCButton {
                        text: qsTr("关闭")
                        backgroundColor: "transparent"
                        textColor: root._navy
                        onClicked: root.close()
                    }
                }
            }

            Loader {
                id: pageLoader
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: root._margin
                Layout.rightMargin: root._margin
                Layout.topMargin: 6
                source: {
                    if (root.currentTab === 0) return "qrc:/maglab/RealtimePage.qml"
                    if (root.currentTab === 1) return "qrc:/maglab/CompensationPage.qml"
                    if (root.currentTab === 4) return "qrc:/maglab/PhotoOverlayPage.qml"
                    return "qrc:/maglab/CollectionPage.qml"
                }
                onLoaded: {
                    root.applyModule()
                    if (item) item.visible = true
                }
            }

            Image {
                Layout.preferredWidth: Math.min(220, parent.width * 0.72)
                Layout.preferredHeight: width / 6.8
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 10
                source: "qrc:/maglab/xunxun.png"
                fillMode: Image.PreserveAspectFit
                opacity: 0.52
                smooth: true
            }
        }
    }
}
