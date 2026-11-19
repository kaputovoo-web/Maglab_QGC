import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGC
import QGroundControl
import QGroundControl.Controls

Popup {
    id: root
    parent: Overlay.overlay
    width: parent ? Math.max(240, Math.min(parent.width - 32, ScreenTools.defaultFontPixelWidth * 46)) : 0
    height: parent ? Math.max(240, parent.height - 32) : 0
    x: parent ? parent.width - width - 16 : 0
    y: parent ? 16 : 0
    modal: true
    focus: true
    closePolicy: (controller && controller.busy) ? Popup.NoAutoClose : Popup.CloseOnEscape
    padding: ScreenTools.defaultFontPixelWidth * 0.8

    property var controller
    property var planMasterController
    property bool _readRequested: false
    property int _probeAttempts: 0

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    readonly property color _navy: "#0B2239"
    readonly property color _cyan: "#00A6B8"
    readonly property color _muted: "#607086"
    readonly property color _border: "#DCE5EE"
    readonly property color _danger: "#D64545"

    function readCurrentMission() {
        if (!planMasterController) {
            return
        }
        if (planMasterController.dirtyForSave) {
            QGroundControl.showMessageDialog(root,
                qsTr("读取飞控当前任务"),
                qsTr("读取会覆盖当前未保存的计划修改，但不会上传任何任务。是否继续？"),
                Dialog.Yes | Dialog.Cancel,
                function() {
                    planMasterController.loadFromVehicle()
                    root._readRequested = true
                    root._probeAttempts = 0
                    probeTimer.start()
                })
            return
        }
        planMasterController.loadFromVehicle()
        _readRequested = true
        _probeAttempts = 0
        probeTimer.start()
    }

    function changeUpdateMode(mode) {
        if (!controller) return
        controller.updateMode = mode
        if (_readRequested || mode === 0) {
            controller.prepare()
        }
    }

    onOpened: {
        if (!controller) {
            return
        }
        previewDebounce.stop()
        probeTimer.stop()
        controller.reset()
        _probeAttempts = 0
        // Keep the "vehicle mission has been read" state across the
        // close/edit/reopen workflow. The plan is edited in Plan View while
        // this modal panel is closed; reopening must not require another
        // destructive vehicle download.
        if (_readRequested || controller.updateMode === 0) {
            controller.prepare()
        }
    }

    onClosed: {
        probeTimer.stop()
        previewDebounce.stop()
        // Do not reset the controller or _readRequested here. Keeping the
        // read plan allows editing on the map and reopening for a fresh
        // preview without downloading the vehicle mission again.
    }

    Timer {
        id: probeTimer
        interval: 500
        repeat: true
        running: root.opened && root._readRequested && root.controller &&
                 !root.controller.ready && !root.controller.busy
        onTriggered: {
            if (!root.planMasterController || root.planMasterController.syncInProgress) {
                if (++root._probeAttempts >= 20) {
                    stop()
                }
                return
            }
            stop()
            root.controller.prepare()
        }
    }

    // Once the vehicle mission has been read, keep the preview in sync while
    // the operator edits the plan. This prevents the confirm button from
    // remaining disabled after a waypoint insert/append.
    Timer {
        id: previewDebounce
        interval: 320
        repeat: false
        onTriggered: {
            if (root.opened && root.controller && !root.controller.busy &&
                    (root._readRequested || root.controller.updateMode === 0)) {
                root.controller.prepare()
            }
        }
    }

    Connections {
        target: root.planMasterController

        function onDirtyForUploadChanged() {
            if (root.opened && root.controller && root.planMasterController &&
                    root.planMasterController.dirtyForUpload &&
                    (root._readRequested || root.controller.updateMode === 0)) {
                previewDebounce.restart()
            }
        }
    }

    background: Rectangle {
        color: "#FFFFFF"
        radius: ScreenTools.defaultFontPixelWidth * 0.7
        border.width: 1
        border.color: root._border
    }

    contentItem: Flickable {
        id: viewport
        contentWidth: width
        contentHeight: content.implicitHeight
        clip: true

        ColumnLayout {
            id: content
            width: viewport.width
            spacing: ScreenTools.defaultFontPixelWidth * 0.6

            RowLayout {
                Layout.fillWidth: true
                spacing: content.spacing
                Rectangle {
                    Layout.preferredWidth: 4
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.7
                    radius: 2
                    color: root._cyan
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    QGCLabel {
                        text: qsTr("飞行中航线替换")
                        color: root._navy
                        font.bold: true
                        font.pointSize: ScreenTools.largeFontPointSize
                    }
                    QGCLabel {
                        text: qsTr("进入只预览，确认后才更新飞控任务")
                        color: root._muted
                        font.pointSize: ScreenTools.smallFontPointSize * 0.9
                    }
                }
                QGCButton {
                    text: qsTr("关闭")
                    enabled: !controller || !controller.busy
                    onClicked: root.close()
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: root._border }

            QGCLabel {
                text: qsTr("更新方式")
                color: root._navy
                font.bold: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: content.spacing
                QGCButton {
                    Layout.fillWidth: true
                    text: qsTr("全量替换")
                    primary: controller && controller.updateMode === 0
                    enabled: !!controller
                    onClicked: root.changeUpdateMode(0)
                }
                QGCButton {
                    Layout.fillWidth: true
                    text: qsTr("部分更新")
                    primary: controller && controller.updateMode === 1
                    enabled: controller && !controller.planOnly
                    onClicked: root.changeUpdateMode(1)
                }
            }
            QGCLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                visible: controller && controller.updateMode === 1
                text: qsTr("仅发送 current 之后的变更范围：替换已存在任务项，并在末尾逐项追加新任务项；不改 Home、不改已执行前缀、不设置 current。")
                color: "#2C8FBF"
                font.pointSize: ScreenTools.smallFontPointSize
            }
            QGCLabel {
                Layout.fillWidth: true
                visible: !(controller && controller.updateMode === 1)
                wrapMode: Text.WordWrap
                text: qsTr("全量替换会保留飞控 Home，重新上传整条航线，并从 Home 后的第一个航点重新执行；不会重新上传已执行前缀。")
                color: root._muted
                font.pointSize: ScreenTools.smallFontPointSize
            }

            Rectangle {
                Layout.fillWidth: true
                visible: !(controller && controller.planOnly)
                implicitHeight: readColumn.implicitHeight + content.spacing * 2
                radius: ScreenTools.defaultFontPixelWidth * 0.5
                color: "#F7F9FB"
                border.width: 1
                border.color: root._border
                ColumnLayout {
                    id: readColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: content.spacing
                    spacing: content.spacing * 0.6
                    QGCLabel {
                        Layout.fillWidth: true
                        text: qsTr("先读取飞控当前任务，再修改 current 之后的航点。读取后可以关闭面板，在 Plan View 中插入或追加航点，重新打开面板即可预览；已读取状态不会因关闭面板而丢失。")
                        color: root._muted
                        wrapMode: Text.WordWrap
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    QGCButton {
                        Layout.fillWidth: true
                        text: qsTr("读取飞控当前任务")
                        enabled: !controller || !controller.busy
                        onClicked: root.readCurrentMission()
                    }
                    QGCButton {
                        Layout.fillWidth: true
                        text: qsTr("关闭并编辑计划")
                        enabled: !controller || !controller.busy
                        onClicked: root.close()
                    }
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                visible: !!(controller && controller.planOnly)
                wrapMode: Text.WordWrap
                color: "#2C8FBF"
                font.pointSize: ScreenTools.smallFontPointSize
                text: qsTr("当前编辑计划作为独立新航线后缀；部分更新需要先读取飞控当前任务，读取会覆盖当前计划中的未保存修改。")
            }
            QGCButton {
                Layout.fillWidth: true
                visible: !!(controller && controller.planOnly)
                text: qsTr("读取飞控当前任务以启用部分更新")
                enabled: controller && !controller.busy
                onClicked: {
                    controller.planOnly = false
                    root.readCurrentMission()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: summaryGrid.implicitHeight + content.spacing * 2
                radius: ScreenTools.defaultFontPixelWidth * 0.5
                color: "#F7F9FB"
                border.width: 1
                border.color: root._border
                GridLayout {
                    id: summaryGrid
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: content.spacing
                    columns: 2
                    columnSpacing: content.spacing
                    rowSpacing: content.spacing * 0.5
                    QGCLabel { text: qsTr("当前模式") }
                    QGCLabel { Layout.fillWidth: true; text: controller && controller.preview.mode ? controller.preview.mode : "—" }
                    QGCLabel { text: qsTr("当前任务序号") }
                    QGCLabel { Layout.fillWidth: true; text: controller && controller.preview.currentIndex !== undefined ? controller.preview.currentIndex : "—" }
                    QGCLabel { text: qsTr("保留范围") }
                    QGCLabel { Layout.fillWidth: true; text: controller && controller.preview.prefixEnd !== undefined ? ("0 — " + controller.preview.prefixEnd) : "—" }
                    QGCLabel { text: qsTr("新航线起点") }
                    QGCLabel { Layout.fillWidth: true; text: controller && controller.preview.newStart !== undefined ? controller.preview.newStart : "—" }
                    QGCLabel { text: qsTr("新任务总项数") }
                    QGCLabel { Layout.fillWidth: true; text: controller && controller.preview.newCount !== undefined ? controller.preview.newCount : "—" }
                    QGCLabel { text: qsTr("上传后当前项") }
                    QGCLabel { Layout.fillWidth: true; text: controller && controller.preview.newCurrent !== undefined ? controller.preview.newCurrent : "—" }
                    QGCLabel { text: qsTr("部分更新范围"); visible: !!(controller && controller.preview.partial) }
                    QGCLabel {
                        Layout.fillWidth: true
                        visible: !!(controller && controller.preview.partial)
                        text: controller && controller.preview.partial
                              ? (controller.preview.updateStart + " — " + controller.preview.updateEnd)
                              : "—"
                    }
                    QGCLabel { text: qsTr("替换/新增数量"); visible: !!(controller && controller.preview.partial) }
                    QGCLabel {
                        Layout.fillWidth: true
                        visible: !!(controller && controller.preview.partial)
                        text: controller && controller.preview.partial
                              ? (controller.preview.replaceCount + " / " + controller.preview.appendCount)
                              : "—"
                    }
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                visible: !!(controller && controller.preview.complexBoundary)
                wrapMode: Text.WordWrap
                color: "#D97706"
                font.pointSize: ScreenTools.smallFontPointSize
                text: qsTr("当前处于复杂任务内部：将保留完整的 %1，完成后自动进入新航线。")
                    .arg(controller && controller.preview.complexName ? controller.preview.complexName : qsTr("复杂任务"))
            }

            QGCLabel {
                Layout.fillWidth: true
                visible: !!(controller && controller.ready && !controller.hasChanges)
                wrapMode: Text.WordWrap
                color: root._muted
                text: qsTr("当前计划与飞控任务没有差异。请在 current 之后插入或追加航点，或修改现有航点后再试。")
            }
            QGCLabel {
                Layout.fillWidth: true
                visible: controller && !controller.busy && controller.errorString === "" &&
                         (!root._readRequested || !controller.ready || !controller.hasChanges)
                wrapMode: Text.WordWrap
                color: root._muted
                font.pointSize: ScreenTools.smallFontPointSize * 0.9
                text: !root._readRequested
                      ? qsTr("上传按钮暂不可用：请先点击“读取飞控当前任务”，读取后即可预览和上传。")
                      : (!controller.ready
                         ? qsTr("正在等待任务预览；请确认飞控已连接，并点击“重新预览”。")
                         : qsTr("上传按钮暂不可用：当前计划没有变化。请在 current 之后插入或追加航点。"))
            }

            QGCLabel {
                Layout.fillWidth: true
                visible: controller && controller.errorString !== ""
                wrapMode: Text.WordWrap
                color: root._danger
                text: controller ? controller.errorString : ""
            }
            QGCLabel {
                Layout.fillWidth: true
                visible: controller && controller.statusText !== ""
                wrapMode: Text.WordWrap
                color: "#2F9E7A"
                text: controller ? controller.statusText : ""
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: content.spacing
                QGCButton {
                    Layout.fillWidth: true
                    text: qsTr("预览 / 刷新")
                    enabled: controller && !controller.busy && (root._readRequested || controller.updateMode === 0)
                    onClicked: controller.prepare()
                }
                QGCButton {
                    Layout.fillWidth: true
                    text: controller && controller.updateMode === 1
                          ? qsTr("确认部分更新")
                          : qsTr("确认上传替换")
                    primary: true
                    enabled: controller && controller.ready && controller.hasChanges && !controller.busy
                    onClicked: {
                        var partial = controller.updateMode === 1
                        var message = partial
                            ? qsTr("将通过 MAVLink 部分任务写入修改序号 %1..%2，并追加 %3 个新任务项。\n\n不会修改 Home 和已执行前缀，不会发送 DO_SET_MISSION_CURRENT，飞控保持当前执行序号 %4。")
                                .arg(controller.preview.updateStart)
                                .arg(controller.preview.updateEnd)
                                .arg(controller.preview.appendCount)
                                .arg(controller.preview.currentIndex)
                            : qsTr("将保留飞控 Home，全量上传整条新航线，并把飞控当前航点设置为 %1，重新执行 Home 后的航点。\n\n不会暂停或恢复飞行模式，请确认飞控不在 VTOL 转换阶段。")
                                .arg(controller.preview.newCurrent)
                        QGroundControl.showMessageDialog(root,
                            partial ? qsTr("部分更新任务") : qsTr("上传替换任务"),
                            message,
                            Dialog.Yes | Dialog.Cancel,
                            function() { controller.uploadReplacement() })
                    }
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: root._muted
                font.pointSize: ScreenTools.smallFontPointSize * 0.85
                text: controller && controller.updateMode === 1
                      ? qsTr("部分更新不发送 DO_SET_MISSION_CURRENT；任一任务项失败都会停止追加，已写入的任务项保持生效。")
                      : qsTr("上传失败时不会设置当前航点；上传成功但当前航点确认失败时，任务已写入飞控，需要人工判断。")
            }
        }
    }
}
