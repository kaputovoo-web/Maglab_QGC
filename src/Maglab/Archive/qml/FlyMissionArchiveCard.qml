import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGC
import QGroundControl
import QGroundControl.Controls

/// Fly View mission timeline: no card, no full-screen background.
/// A thin teal progress line with a small moving aircraft and a soft sheen.
Item {
    id: root

    property var activeVehicle
    property var missionController
    property var record: MissionArchiveManager.latestRecord || ({})
    property real bottomInset: 0

    readonly property real progressValue: Math.max(0, Math.min(100, MissionArchiveManager.executionProgress))
    readonly property string statusText: (record && record.status) ? String(record.status) : ""
    readonly property bool complete: progressValue >= 99.9
    readonly property bool failed: statusText === "error" || statusText === "failed" || statusText === "cancelled"
    readonly property bool paused: !failed && !complete &&
                                   (statusText === "interrupted" || statusText === "waiting")
    readonly property bool active: MissionArchiveManager.executionActive && !paused && !complete && !failed
    readonly property color statusColor: failed ? "#E05252"
                                       : complete ? "#3DDC84"
                                       : paused ? "#F0A63A" : "#35D6C0"
    readonly property string displayTaskName:
        (record && record.executionName) ? record.executionName : qsTr("Mission")

    width: Math.min(560, Math.max(260, parent ? parent.width * 0.45 : 480),
                    parent ? parent.width - 24 : 480)
    height: 52
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? parent.height - height - bottomInset - 28 : 0
    z: QGroundControl.zOrderTopMost

    // A very light text shadow is provided by the outline style below; the
    // component itself deliberately has no Rectangle background.
    Text {
        id: titleLabel
        anchors.left: parent.left
        anchors.top: parent.top
        width: Math.max(0, progressTrack.width - percentLabel.width - 8)
        text: root.displayTaskName
        color: "#F2F8FA"
        style: Text.Outline
        styleColor: "#80000000"
        elide: Text.ElideRight
        font.pixelSize: 11
        font.bold: true
    }

    Text {
        id: percentLabel
        anchors.right: parent.right
        anchors.top: parent.top
        text: Math.round(root.progressValue) + "%"
        color: root.statusColor
        style: Text.Outline
        styleColor: "#80000000"
        font.pixelSize: 12
        font.bold: true
    }

    Rectangle {
        id: progressTrack
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 26
        height: 5
        radius: height / 2
        color: Qt.rgba(0.45, 0.55, 0.65, 0.42)

        Rectangle {
            id: progressFill
            width: progressTrack.width * root.progressValue / 100
            height: parent.height
            radius: parent.radius
            clip: true
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#35D6C0" }
                GradientStop { position: 1.0; color: "#39A9FF" }
            }
            Behavior on width {
                NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
            }

            // Light flowing highlight, constrained to the completed portion.
            Rectangle {
                id: sheen
                width: 72
                height: parent.height
                opacity: 0.32
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.5; color: "#BFFFFFFF" }
                    GradientStop { position: 1.0; color: "transparent" }
                }
                SequentialAnimation on x {
                    running: root.active && root.progressValue > 0.5 && root.progressValue < 99.5
                    loops: Animation.Infinite
                    NumberAnimation {
                        from: -sheen.width
                        to: progressFill.width
                        duration: 1800
                        easing.type: Easing.InOutSine
                    }
                }
            }
        }
    }

    QGCColoredImage {
        id: aircraftIcon
        width: 20
        height: 20
        source: "/qmlimages/Quad.svg"
        color: root.statusColor
        x: Math.max(0, Math.min(progressTrack.width - width,
                                progressTrack.width * root.progressValue / 100 - width / 2))
        y: progressTrack.y - height - 1
        Behavior on x {
            NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
        }

        SequentialAnimation on opacity {
            running: root.active
            loops: Animation.Infinite
            NumberAnimation { from: 0.75; to: 1.0; duration: 1000; easing.type: Easing.InOutSine }
            NumberAnimation { from: 1.0; to: 0.75; duration: 1000; easing.type: Easing.InOutSine }
        }
    }

    Text {
        id: timeLabel
        anchors.left: parent.left
        anchors.top: progressTrack.bottom
        anchors.topMargin: 5
        text: root.paused ? qsTr("已暂停")
                          : root.complete ? qsTr("已完成 · %1").arg(root.record.duration || "")
                                          : qsTr("已执行 %1").arg(root.record.duration || "--:--:--")
        color: "#D6E3EA"
        style: Text.Outline
        styleColor: "#80000000"
        font.pixelSize: 10
    }

    Text {
        anchors.right: parent.right
        anchors.top: progressTrack.bottom
        anchors.topMargin: 5
        text: root.record && root.record.totalItems !== undefined
              ? qsTr("%1 项任务").arg(root.record.totalItems) : ""
        color: "#AFC3CE"
        style: Text.Outline
        styleColor: "#80000000"
        font.pixelSize: 10
    }
}
