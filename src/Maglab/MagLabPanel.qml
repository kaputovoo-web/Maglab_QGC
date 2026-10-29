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

    readonly property color _navy: "#0B2239"
    readonly property color _navySoft: "#153550"
    readonly property color _cyan: "#00A6B8"
    readonly property color _bg: "#F3F6FA"
    readonly property color _border: "#DCE5EE"
    readonly property color _muted: "#607086"
    readonly property color _ok: "#2F9E7A"
    readonly property color _warn: "#D97706"
    readonly property real _margin: ScreenTools.defaultFontPixelWidth * 0.7
    readonly property var sensor: SimpleTest
    readonly property bool _narrow: width < 390

    property int currentTab: 0
    property real _nowMs: Date.now()
    readonly property bool sensorOnline: sensor.lastSampleMs > 0 && (_nowMs - sensor.lastSampleMs) < 3000
    readonly property string moduleName: ["采集与磁图", "磁图库", "相机"][currentTab]

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: root._nowMs = Date.now()
    }

    background: Rectangle { color: root._bg }

    component StatusChip: Rectangle {
        id: chip
        property string label
        property bool ok: true
        implicitWidth: chipRow.implicitWidth + 16
        implicitHeight: chipRow.implicitHeight + 8
        radius: height / 2
        color: chip.ok ? Qt.rgba(0.18, 0.62, 0.48, 0.18) : Qt.rgba(0.85, 0.45, 0.10, 0.18)
        RowLayout {
            id: chipRow
            anchors.centerIn: parent
            spacing: 5
            Rectangle {
                Layout.preferredWidth: 6
                Layout.preferredHeight: 6
                radius: 3
                color: chip.ok ? root._ok : root._warn
            }
            QGCLabel {
                text: chip.label
                color: chip.ok ? "#D8F3E8" : "#FDE7C8"
                font.pointSize: ScreenTools.smallFontPointSize * 0.9
            }
        }
    }

    component NavTab: Rectangle {
        id: tab
        property string label
        property bool selected
        property int tabIndex
        implicitHeight: ScreenTools.defaultFontPixelHeight * 2.0
        radius: 6
        color: tab.selected ? root._navySoft : "transparent"
        border.width: tab.selected ? 1 : 0
        border.color: root._cyan
        QGCLabel {
            anchors.centerIn: parent
            text: tab.label
            color: tab.selected ? "#FFFFFF" : root._muted
            font.bold: tab.selected
            font.pointSize: ScreenTools.smallFontPointSize
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.currentTab = tab.tabIndex
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: navColumn.implicitHeight + root._margin * 1.4
            color: root._navy

            ColumnLayout {
                id: navColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: root._margin
                anchors.rightMargin: root._margin
                spacing: root._margin * 0.45

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root._margin

                    QGCColoredImage {
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        source: "qrc:/maglab/MaglabIcon.svg"
                        color: root._cyan
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        RowLayout {
                            spacing: root._margin * 0.5
                            QGCLabel {
                                text: "MagLab"
                                color: "#FFFFFF"
                                font.bold: true
                                font.pointSize: ScreenTools.largeFontPointSize
                            }
                            QGCLabel {
                                text: qsTr("磁测工作站")
                                color: "#9FB3C8"
                                font.pointSize: ScreenTools.smallFontPointSize
                            }
                        }
                        QGCLabel {
                            text: root.moduleName
                            color: "#7FD8E4"
                            font.pointSize: ScreenTools.smallFontPointSize * 0.9
                        }
                    }

                    QGCButton {
                        text: qsTr("设置")
                        backgroundColor: "transparent"
                        textColor: "#D8E3EE"
                        onClicked: {
                            root.close()
                            mainWindow.showSettingsTool()
                        }
                    }
                    QGCButton {
                        text: qsTr("关闭")
                        backgroundColor: "transparent"
                        textColor: "#FFFFFF"
                        onClicked: root.close()
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: root._margin * 0.8
                    StatusChip {
                        label: qsTr("传感器 %1").arg(root.sensorOnline ? qsTr("在线") : qsTr("离线"))
                        ok: root.sensorOnline
                    }
                    StatusChip {
                        label: qsTr("UDP %1").arg(root.sensor.udpListening ? qsTr("已监听") : qsTr("未监听"))
                        ok: root.sensor.udpListening
                    }
                    StatusChip {
                        label: qsTr("存储 %1").arg(MagLabPaths.storageReady ? qsTr("就绪") : qsTr("不可用"))
                        ok: MagLabPaths.storageReady
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: tabRow.implicitHeight + root._margin
            color: "#FFFFFF"
            border.width: 0

            RowLayout {
                id: tabRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: root._margin
                spacing: root._margin * 0.5

                NavTab { label: qsTr("采集与磁图"); selected: root.currentTab === 0; tabIndex: 0 }
                NavTab { label: qsTr("磁图库"); selected: root.currentTab === 1; tabIndex: 1 }
                NavTab { label: qsTr("相机"); selected: root.currentTab === 2; tabIndex: 2 }
                Item { Layout.fillWidth: true }
                QGCLabel {
                    visible: !root._narrow
                    text: qsTr("UDP 14551")
                    color: root._muted
                    font.pointSize: ScreenTools.smallFontPointSize * 0.85
                }
            }
        }

        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: root._margin
            Layout.rightMargin: root._margin
            Layout.bottomMargin: root._margin
            source: {
                switch (root.currentTab) {
                case 1:  return "qrc:/maglab/MagMapGalleryPage.qml"
                case 2:  return "qrc:/maglab/MediaCapturePage.qml"
                default: return "qrc:/maglab/CollectionPage.qml"
                }
            }
        }
    }
}
