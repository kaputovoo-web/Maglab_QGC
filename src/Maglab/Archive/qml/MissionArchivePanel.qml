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
import QtQuick.Dialogs
import QtQuick.Layouts

import QGC
import QGroundControl
import QGroundControl.Controls

Item {
    id: root

    property var planMasterController
    onPlanMasterControllerChanged: {
        if (planMasterController) MissionArchiveManager.attachPlanController(planMasterController)
    }
    property int selectedIndex: -1
    signal archiveLoaded()

    readonly property color deepBlue: "#123f78"
    readonly property color accentBlue: "#2f78c4"
    readonly property real margin: ScreenTools.defaultFontPixelWidth * 0.75

    function statusText(status) {
        if (status === "waiting")     return qsTr("Awaiting first waypoint")
        if (status === "running")     return qsTr("In progress")
        if (status === "completed")   return qsTr("Completed")
        if (status === "ended")       return qsTr("Ended")
        if (status === "interrupted") return qsTr("Interrupted")
        return qsTr("Saved")
    }

    function iconForChoice(choice, currentIcon) {
        switch (choice) {
        case 1: return "/qmlimages/PlanCreator/LLMPlanCreator.png"
        case 2: return "/qmlimages/PlanCreator/SketchPlanCreator.png"
        case 3: return "/qmlimages/PlanCreator/SurveyPlanCreator.png"
        case 4: return "/qmlimages/MapAddMission.svg"
        case 5: return "/qmlimages/MapDrawShape.svg"
        default: return currentIcon
        }
    }

    QGCPalette {
        id: qgcPal
        colorGroupEnabled: true
    }

    function archiveIndexForId(archiveId) {
        var currentEntries = MissionArchiveManager.entries
        for (var entryIndex = 0; entryIndex < currentEntries.length; entryIndex++) {
            if (currentEntries[entryIndex].id === archiveId) {
                return entryIndex
            }
        }
        return -1
    }

    function confirmDelete(archiveId, archiveName) {
        mainWindow.showMessageDialog(
                    qsTr("Delete archived mission"),
                    qsTr("Delete '%1' and its cached plan? This cannot be undone.").arg(archiveName),
                    Dialog.Yes | Dialog.Cancel,
                    function() {
                        var currentIndex = root.archiveIndexForId(archiveId)
                        if (currentIndex >= 0) {
                            MissionArchiveManager.removeEntry(currentIndex)
                            root.selectedIndex = -1
                        }
                    })
    }

    Rectangle {
        anchors.fill: parent
        radius: ScreenTools.defaultFontPixelWidth
        color: Qt.rgba(0.97, 0.98, 1.0, 0.90)
        border.width: 1
        border.color: Qt.rgba(0.18, 0.47, 0.77, 0.32)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.margin
        spacing: root.margin

        ColumnLayout {
            Layout.fillWidth: true
            spacing: root.margin * 0.65

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                QGCLabel {
                    text: qsTr("Mission Archive")
                    color: root.deepBlue
                    font.bold: true
                    font.pointSize: ScreenTools.largeFontPointSize
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: root.planMasterController ? qsTr("Plans are cached automatically and named by creation time") : qsTr("Browse and manage archived plans. Open this panel from Plan View to save or load a plan.")
                    color: Qt.rgba(0.07, 0.25, 0.47, 0.70)
                    font.pointSize: ScreenTools.smallFontPointSize
                    wrapMode: Text.WordWrap
                }
            }

            QGCButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: qsTr("Save now")
                enabled: !!root.planMasterController
                pointSize: ScreenTools.smallFontPointSize
                onClicked: MissionArchiveManager.saveNow()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Qt.rgba(0.18, 0.47, 0.77, 0.22)
        }

        QGCListView {
            id: archiveList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: root.margin
            model: MissionArchiveManager.entries
            property var panelRoot: root

            delegate: Rectangle {
                id: archiveCard
                width: archiveList.width
                height: cardColumn.implicitHeight + root.margin * 2
                radius: ScreenTools.defaultFontPixelWidth * 0.8
                color: root.selectedIndex === index
                       ? Qt.rgba(0.88, 0.94, 1.0, 0.96)
                       : Qt.rgba(1.0, 1.0, 1.0, 0.78)
                border.width: root.selectedIndex === index ? 2 : 1
                border.color: root.selectedIndex === index
                              ? root.accentBlue
                              : Qt.rgba(0.12, 0.28, 0.46, 0.18)

                property var archiveEntry: modelData

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.selectedIndex = root.selectedIndex === index ? -1 : index
                }

                ColumnLayout {
                    id: cardColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: root.margin
                    spacing: root.margin * 0.55

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: root.margin

                        Rectangle {
                            width: ScreenTools.defaultFontPixelHeight * 3.1
                            height: width
                            radius: width / 2
                            color: Qt.rgba(0.18, 0.47, 0.77, 0.12)

                            Image {
                                anchors.centerIn: parent
                                width: parent.width * 0.66
                                height: width
                                source: archiveCard.archiveEntry.icon
                                fillMode: Image.PreserveAspectFit
                                opacity: 0.74
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            QGCLabel {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: archiveCard.archiveEntry.name
                                wrapMode: Text.WrapAnywhere
                                maximumLineCount: 2
                                font.bold: true
                                color: Qt.rgba(0.05, 0.16, 0.28, 0.82)
                                opacity: 0.84
                            }

                            QGCLabel {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: archiveCard.archiveEntry.type
                                color: Qt.rgba(0.07, 0.25, 0.47, 0.66)
                                font.pointSize: ScreenTools.smallFontPointSize
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Rectangle {
                            Layout.minimumWidth: 0
                            radius: height / 2
                            height: statusLabel.implicitHeight + root.margin * 0.55
                            Layout.preferredWidth: Math.min(statusLabel.implicitWidth + root.margin * 1.2,
                                                            archiveCard.width - root.margin * 2)
                            color: archiveCard.archiveEntry.status === "running"
                                   ? Qt.rgba(0.12, 0.62, 0.42, 0.16)
                                   : Qt.rgba(0.18, 0.47, 0.77, 0.12)

                            QGCLabel {
                                id: statusLabel
                                anchors.centerIn: parent
                                text: root.statusText(archiveCard.archiveEntry.status)
                                color: archiveCard.archiveEntry.status === "running" ? "#14764f" : root.deepBlue
                                font.bold: true
                                font.pointSize: ScreenTools.smallFontPointSize
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        QGCButton {
                            Layout.minimumWidth: 0
                            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 7.5
                            text: archiveCard.archiveEntry.status === "running"
                                  ? qsTr("Running")
                                  : qsTr("Delete")
                            pointSize: ScreenTools.smallFontPointSize
                            enabled: archiveCard.archiveEntry.status !== "running"
                            onClicked: root.confirmDelete(archiveCard.archiveEntry.id,
                                                          archiveCard.archiveEntry.name)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        QGCLabel {
                            Layout.fillWidth: true
                            text: qsTr("Saved")
                            color: Qt.rgba(0.12, 0.25, 0.38, 0.62)
                            font.pointSize: ScreenTools.smallFontPointSize
                        }

                        QGCLabel {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: archiveCard.archiveEntry.savedDate + "  " + archiveCard.archiveEntry.savedTime
                            horizontalAlignment: Text.AlignLeft
                            color: root.deepBlue
                            font.bold: true
                            font.pointSize: ScreenTools.smallFontPointSize
                            wrapMode: Text.WrapAnywhere
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: archiveCard.archiveEntry.hasExecution || archiveCard.archiveEntry.isWaiting
                        spacing: 0

                        QGCLabel {
                            Layout.fillWidth: true
                            text: archiveCard.archiveEntry.flightDate
                            color: Qt.rgba(0.12, 0.25, 0.38, 0.62)
                            font.pointSize: ScreenTools.smallFontPointSize
                        }

                        QGCLabel {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            text: archiveCard.archiveEntry.isWaiting
                                  ? qsTr("Waiting for first waypoint")
                                  : archiveCard.archiveEntry.startTime + "  →  " +
                                    (archiveCard.archiveEntry.isRunning ? qsTr("Running") : archiveCard.archiveEntry.endTime)
                            horizontalAlignment: Text.AlignLeft
                            color: root.deepBlue
                            font.bold: true
                            font.pointSize: ScreenTools.smallFontPointSize
                            wrapMode: Text.WrapAnywhere
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root.selectedIndex === index
                        spacing: root.margin * 0.55

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Qt.rgba(0.18, 0.47, 0.77, 0.18)
                        }

                        QGCTextField {
                            id: nameEditor
                            Layout.fillWidth: true
                            // 不能直接绑定 archiveEntry.name：用户一输入绑定就被破坏，
                            // 而 delegate 会被复用，这里之后可能显示另一条任务的旧名字，
                            // Apply 就会把名字写到错的任务上。改成本地草稿 + index 变化时重新同步。
                            property string draft: archiveCard.archiveEntry.name

                            text: draft
                            placeholderText: qsTr("Mission name")

                            // 行被复用（滚动/模型变化）时把草稿恢复成该条任务的真实名字；
                            // 正在输入时不干预。不用 Connections + onIndexChanged：
                            // delegate 上没有那个信号，QML 会报 "no signal matches"。
                            //
                            // 也不要加 restoreMode —— 本文件 import 的是 QtQuick 2.12，
                            // 而 Binding.restoreMode 是 2.14 才有的，加了会导致整个
                            // 组件加载失败，进而 PlanView / 主窗口起不来。
                            // 这里也不需要它：值表达式不引用 nameEditor.draft，
                            // 写入目标不会让绑定失效，它会自己保留。
                            Binding {
                                target:   nameEditor
                                property: "draft"
                                value:    archiveCard.archiveEntry.name
                                when:     !nameEditor.activeFocus
                            }
                        }

                        QGCComboBox {
                            id: iconSelector
                            Layout.fillWidth: true
                            model: [
                                qsTr("Keep current icon"),
                                qsTr("LLM / Spiral"),
                                qsTr("Sketch"),
                                qsTr("Survey"),
                                qsTr("Waypoints"),
                                qsTr("Shape")
                            ]
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            QGCButton {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: qsTr("Open")
                                enabled: !!archiveList.panelRoot.planMasterController
                                pointSize: ScreenTools.smallFontPointSize
                                onClicked: {
                                    if (MissionArchiveManager.loadEntry(index, archiveList.panelRoot.planMasterController)) {
                                        archiveList.panelRoot.archiveLoaded()
                                    }
                                }
                            }

                            QGCButton {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: qsTr("Apply")
                                pointSize: ScreenTools.smallFontPointSize
                                onClicked: {
                                    MissionArchiveManager.updateEntry(
                                                index,
                                                nameEditor.draft,
                                                iconSelector.currentIndex > 0
                                                    ? archiveList.panelRoot.iconForChoice(iconSelector.currentIndex,
                                                                         archiveCard.archiveEntry.icon)
                                                    : "")
                                    archiveList.panelRoot.selectedIndex = -1
                                }
                            }
                        }

                        QGCButton {
                            Layout.fillWidth: true
                            text: archiveCard.archiveEntry.status === "running"
                                  ? qsTr("Cannot delete a running mission")
                                  : qsTr("Delete this mission")
                            pointSize: ScreenTools.smallFontPointSize
                            enabled: archiveCard.archiveEntry.status !== "running"
                            onClicked: root.confirmDelete(archiveCard.archiveEntry.id,
                                                          archiveCard.archiveEntry.name)
                        }
                    }
                }
            }

            visible: archiveList.count > 0
        }

        QGCLabel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: archiveList.count === 0
            text: qsTr("No archived mission yet\nCreate or edit a plan to start automatic caching")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: Qt.rgba(0.07, 0.25, 0.47, 0.65)
        }
    }
}
