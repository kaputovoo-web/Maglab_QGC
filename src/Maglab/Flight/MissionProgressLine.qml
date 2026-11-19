import QtQuick
import QtQuick.Controls

import QGC
import QGroundControl
import QGroundControl.Controls

/// Second row of the transparent Fly View HUD. No card/background.
Item {
    id: root

    property var missionController
    readonly property var record: MissionArchiveManager.latestRecord || ({})
    readonly property real archiveProgress: MissionArchiveManager.executionProgress
    readonly property real directProgress: {
        if (!missionController) return 0
        var items = missionController.visualItems
        if (!items || items.count < 2) return 0
        var current = missionController.currentMissionIndex
        if (current < 0) return 0
        var first = -1
        var last = -1
        for (var i = 0; i < items.count; ++i) {
            var item = items.get(i)
            if (!item || item.sequenceNumber <= 0) continue
            if (first < 0) first = item.sequenceNumber
            last = item.lastSequenceNumber
        }
        if (first < 0 || last < first) return 0
        return Math.max(0, Math.min(100, 100 * (current - first) / Math.max(1, last - first)))
    }
    readonly property real progressValue: Math.max(
        0, Math.min(100,
            MissionArchiveManager.executionActive
                ? Math.max(archiveProgress, directProgress)
                : directProgress))
    readonly property string statusText: (record && record.status) ? String(record.status)
                                       : (missionController && missionController.currentMissionIndex >= 0 ? "running" : "")
    readonly property bool complete: progressValue >= 99.9
    readonly property bool failed: statusText === "error" || statusText === "failed" || statusText === "cancelled"
    readonly property bool paused: !failed && !complete && (statusText === "interrupted" || statusText === "waiting")
    readonly property bool active: !paused && !complete && !failed &&
                                   (MissionArchiveManager.executionActive || directProgress > 0)
    readonly property color statusColor: failed ? "#E05252"
                                       : complete ? "#3DDC84"
                                       : paused ? "#F0A63A" : "#35D6C0"

    height: 26

    Text {
        id: timeText
        anchors.left: parent.left
        anchors.top: parent.top
        text: root.statusText === "" ? "" :
              root.paused ? qsTr("已暂停") :
              root.complete ? qsTr("已完成") :
              qsTr("已执行 %1").arg(root.record.duration || "--:--:--")
        color: "#EAF4F7"
        style: Text.Outline
        styleColor: "#90000000"
        font.pixelSize: 10
    }

    Text {
        anchors.right: parent.right
        anchors.top: parent.top
        text: Math.round(root.progressValue) + "%"
        color: root.statusColor
        style: Text.Outline
        styleColor: "#90000000"
        font.pixelSize: 11
        font.bold: true
    }

    Rectangle {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 4
        radius: 2
        color: Qt.rgba(0.55, 0.64, 0.72, 0.35)

        Rectangle {
            id: fill
            width: track.width * root.progressValue / 100
            height: parent.height
            radius: parent.radius
            clip: true
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#35D6C0" }
                GradientStop { position: 1.0; color: "#39A9FF" }
            }
            Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

            Rectangle {
                id: sheen
                width: 60
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
                        to: fill.width
                        duration: 1800
                        easing.type: Easing.InOutSine
                    }
                }
            }
        }
    }

    QGCColoredImage {
        width: 18
        height: 18
        source: "/qmlimages/Quad.svg"
        color: root.statusColor
        x: Math.max(0, Math.min(track.width - width,
                                track.width * root.progressValue / 100 - width / 2))
        y: track.y - height + 1
        Behavior on x { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

        SequentialAnimation on opacity {
            running: root.active
            loops: Animation.Infinite
            NumberAnimation { from: 0.75; to: 1.0; duration: 1000; easing.type: Easing.InOutSine }
            NumberAnimation { from: 1.0; to: 0.75; duration: 1000; easing.type: Easing.InOutSine }
        }
    }
}
