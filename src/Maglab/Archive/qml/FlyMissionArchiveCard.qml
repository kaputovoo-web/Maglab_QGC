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

Item {
    id: root

    property var activeVehicle
    property var missionController
    property var record: MissionArchiveManager.latestRecord || ({})

    // Event ownership lives in the C++ observer, not in this transient card.
    readonly property real progressValue: MissionArchiveManager.executionProgress
    readonly property string displayTaskName:
        (record && record.name) ? record.name : qsTr("Mission Progress")

    // ---- 视觉：右下角常驻进度条 ----
    width:  ScreenTools.defaultFontPixelWidth * 38
    height: ScreenTools.defaultFontPixelHeight * 4.0

    Rectangle {
        anchors.fill: parent
        radius: ScreenTools.defaultFontPixelHeight * 0.7
        color: Qt.rgba(0.05, 0.11, 0.18, 0.72)
        border.width: 1
        border.color: Qt.rgba(1.0, 1.0, 1.0, 0.14)

        RowLayout {
            anchors.fill: parent
            anchors.margins: ScreenTools.defaultFontPixelWidth * 0.9
            spacing: ScreenTools.defaultFontPixelWidth * 0.7

            QGCLabel {
                Layout.maximumWidth: ScreenTools.defaultFontPixelWidth * 9
                text: root.displayTaskName
                elide: Text.ElideRight
                color: Qt.rgba(1.0, 1.0, 1.0, 0.92)
                font.pointSize: ScreenTools.smallFontPointSize
            }

            Rectangle {
                id: progressTrack
                Layout.fillWidth: true
                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 0.6
                Layout.alignment: Qt.AlignVCenter
                radius: height / 2
                color: Qt.rgba(1.0, 1.0, 1.0, 0.18)
                clip: true

                Rectangle {
                    id: progressFill
                    height: parent.height
                    width: parent.width * root.progressValue / 100.0
                    radius: parent.radius
                    color: "#36ad7d"

                    Behavior on width {
                        NumberAnimation { duration: 240; easing.type: Easing.OutCubic }
                    }
                }
            }

            QGCLabel {
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 4.2
                text: Math.round(root.progressValue) + "%"
                horizontalAlignment: Text.AlignRight
                color: "#ffffff"
                font.bold: true
                font.pointSize: ScreenTools.defaultFontPointSize
            }
        }
    }
}
