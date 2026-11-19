/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGC
import QGroundControl
import QGroundControl.Controls

Rectangle {
    id: root

    anchors.fill: parent
    color: qgcPal.window

    readonly property color deepBlue: "#123f78"
    readonly property color accentBlue: "#2f78c4"
    readonly property real margin: ScreenTools.defaultFontPixelWidth

    function statusText(status) {
        status = String(status || "")
        if (status === "waiting")     return qsTr("WAITING")
        if (status === "running")     return qsTr("RUNNING")
        if (status === "completed")   return qsTr("COMPLETED")
        if (status === "interrupted") return qsTr("INTERRUPTED")
        if (status === "cancelled")   return qsTr("CANCELLED")
        return status.toUpperCase()
    }

    QGCPalette {
        id: qgcPal
        colorGroupEnabled: true
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: root.margin

        Rectangle {
            Layout.fillWidth: true
            height: headerColumn.implicitHeight + root.margin * 1.6
            radius: ScreenTools.defaultFontPixelWidth * 0.8
            color: Qt.rgba(0.70, 0.82, 0.94, 0.34)
            border.width: 1
            border.color: Qt.rgba(0.18, 0.47, 0.77, 0.30)

            ColumnLayout {
                id: headerColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: root.margin * 0.8
                spacing: ScreenTools.defaultFontPixelHeight * 0.25

                QGCLabel {
                    text: qsTr("TASK NAME TIME")
                    color: root.deepBlue
                    font.bold: true
                    font.pointSize: ScreenTools.largeFontPointSize
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Records observed armed mission-mode activity for the active vehicle. Local edited plans are not assumed to match the vehicle mission. Disconnect, disarm or mode exit marks the observation interrupted; reaching the final index does not prove mission completion.")
                    wrapMode: Text.WordWrap
                    color: Qt.rgba(0.07, 0.25, 0.47, 0.76)
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Completed name format: yyyy-MM-dd-HHmmss-HHmmss")
                    color: root.accentBlue
                    font.bold: true
                }
            }
        }

        QGCListView {
            id: executionList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: root.margin * 0.75
            model: MissionArchiveManager.executionRecords

            delegate: Rectangle {
                id: recordCard

                width: executionList.width
                height: recordColumn.implicitHeight + root.margin * 1.6
                radius: ScreenTools.defaultFontPixelWidth * 0.8
                color: Qt.rgba(0.76, 0.86, 0.96, 0.30)
                border.width: 1
                border.color: Qt.rgba(0.12, 0.28, 0.46, 0.22)

                property var taskRecord: modelData

                ColumnLayout {
                    id: recordColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: root.margin * 0.8
                    spacing: ScreenTools.defaultFontPixelHeight * 0.45

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: root.margin

                        Image {
                            width: ScreenTools.defaultFontPixelHeight * 2.8
                            height: width
                            source: recordCard.taskRecord.icon
                            fillMode: Image.PreserveAspectFit
                            opacity: 0.76
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            QGCLabel {
                                Layout.fillWidth: true
                                text: recordCard.taskRecord.executionName
                                color: root.deepBlue
                                font.bold: true
                                font.pointSize: ScreenTools.largeFontPointSize
                                elide: Text.ElideRight
                            }

                            QGCLabel {
                                Layout.fillWidth: true
                                text: recordCard.taskRecord.planName + "  ·  " + recordCard.taskRecord.routeType
                                color: Qt.rgba(0.07, 0.25, 0.47, 0.68)
                                elide: Text.ElideRight
                            }
                        }

                        Rectangle {
                            height: statusLabel.implicitHeight + root.margin * 0.55
                            width: statusLabel.implicitWidth + root.margin * 1.1
                            radius: height / 2
                            color: recordCard.taskRecord.isRunning
                                   ? Qt.rgba(0.12, 0.62, 0.42, 0.18)
                                   : Qt.rgba(0.18, 0.47, 0.77, 0.14)

                            QGCLabel {
                                id: statusLabel
                                anchors.centerIn: parent
                                text: root.statusText(recordCard.taskRecord.status)
                                color: recordCard.taskRecord.isRunning ? "#14764f" : root.deepBlue
                                font.bold: true
                                font.pointSize: ScreenTools.smallFontPointSize
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Qt.rgba(0.18, 0.47, 0.77, 0.18)
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: root.width < ScreenTools.defaultFontPixelWidth * 70 ? 1 : 2
                        columnSpacing: root.margin * 1.5
                        rowSpacing: root.margin * 0.45

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            QGCLabel {
                                text: qsTr("FIRST WAYPOINT / START")
                                color: Qt.rgba(0.07, 0.25, 0.47, 0.64)
                                font.pointSize: ScreenTools.smallFontPointSize
                            }

                            QGCLabel {
                                Layout.fillWidth: true
                                text: recordCard.taskRecord.startDateTime
                                color: root.deepBlue
                                font.bold: true
                                font.pointSize: ScreenTools.largeFontPointSize
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            QGCLabel {
                                text: qsTr("LAST WAYPOINT / END")
                                color: Qt.rgba(0.07, 0.25, 0.47, 0.64)
                                font.pointSize: ScreenTools.smallFontPointSize
                            }

                            QGCLabel {
                                Layout.fillWidth: true
                                text: recordCard.taskRecord.endDateTime
                                color: root.deepBlue
                                font.bold: true
                                font.pointSize: ScreenTools.largeFontPointSize
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        QGCLabel {
                            text: qsTr("Duration: %1").arg(recordCard.taskRecord.duration)
                            color: Qt.rgba(0.07, 0.25, 0.47, 0.72)
                            font.bold: true
                        }

                        QGCLabel {
                            Layout.fillWidth: true
                            text: Math.round(Number(recordCard.taskRecord.progress)) + "%"
                            horizontalAlignment: Text.AlignRight
                            color: root.deepBlue
                            font.bold: true
                            font.pointSize: ScreenTools.largeFontPointSize
                        }
                    }
                }
            }

            QGCLabel {
                anchors.centerIn: parent
                visible: executionList.count === 0
                text: qsTr("No mission execution records yet")
                color: Qt.rgba(0.07, 0.25, 0.47, 0.66)
                font.bold: true
            }
        }
    }
}
