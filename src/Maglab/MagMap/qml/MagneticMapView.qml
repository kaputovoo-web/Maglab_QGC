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
import QtLocation
import QtPositioning

import QGC
import QGroundControl
import QGroundControl.Controls

Item {
    id: root
    anchors.fill: parent

    property var mapControl: null
    property var _attachedMap: null
    property real topInset: 0
    property real bottomOffset: 0
    property bool pipMode: false
    property bool overlayEnabled: false
    property real overlayOpacity: 0.55
    property bool panelOpen: false
    property bool dragEnabled: false
    property var dragCoordinate: QtPositioning.coordinate()
    readonly property real edgeMargin: ScreenTools.defaultFontPixelHeight * 0.75
    readonly property bool compactLayout: root.width < ScreenTools.defaultFontPixelWidth * 58
    readonly property real panelMaximumWidth: ScreenTools.defaultFontPixelWidth * 44

    function locateOverlay() {
        if (!mapControl || !magneticMapController.hasMap) {
            return
        }
        mapControl.center = QtPositioning.coordinate(
                    (magneticMapController.northWest.latitude + magneticMapController.southEast.latitude) / 2,
                    (magneticMapController.northWest.longitude + magneticMapController.southEast.longitude) / 2)
        dragCoordinate = QtPositioning.coordinate()
        mapControl.zoomLevel = 18
    }


    onMapControlChanged: {
        if (_attachedMap) _attachedMap.removeMapItem(magneticRaster)
        _attachedMap = mapControl
        if (_attachedMap) _attachedMap.addMapItem(magneticRaster)
    }
    Component.onDestruction: {
        if (_attachedMap) _attachedMap.removeMapItem(magneticRaster)
    }
    MagneticMapController {
        id: magneticMapController
        onMapChanged: {
            if (root.overlayEnabled) root.locateOverlay()
        }
    }
    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    function mercatorY(latitude) {
        return (1 - Math.log(Math.tan(Math.PI / 4 + latitude * Math.PI / 360)) / Math.PI) / 2
    }

    MapQuickItem {
        id: magneticRaster
        coordinate: root.dragCoordinate.isValid ? root.dragCoordinate : magneticMapController.northWest
        anchorPoint.x: 0
        anchorPoint.y: 0
        zoomLevel: 16
        visible: root.overlayEnabled && magneticMapController.hasMap && !root.pipMode
        opacity: root.overlayOpacity
        // Keep the raster below mission and flown routes using the host version's layer constants.
        z: Math.min(QGroundControl.zOrderWaypointLines, QGroundControl.zOrderTrajectoryLines) - 1

        sourceItem: Item {
            width: magneticMapController.hasMap ? 256 * Math.pow(2, 16)
                   * (magneticMapController.southEast.longitude - magneticMapController.northWest.longitude) / 360 : 0
            height: magneticMapController.hasMap ? 256 * Math.pow(2, 16)
                    * (root.mercatorY(magneticMapController.southEast.latitude)
                       - root.mercatorY(magneticMapController.northWest.latitude)) : 0

            Image {
                anchors.fill: parent
                source: magneticMapController.imageUrl
                fillMode: Image.Stretch
                smooth: true
                mipmap: true
                asynchronous: true
                cache: false
            }

            MouseArea {
                id: dragArea
                anchors.fill: parent
                enabled: root.dragEnabled && root.overlayEnabled
                cursorShape: enabled ? Qt.SizeAllCursor : Qt.ArrowCursor
                property var startCoord
                property var startMapPoint
                onPressed: {
                    if (!root.mapControl) return
                    startMapPoint = root.mapControl.toCoordinate(
                                root.mapControl.mapFromItem(parent, mouse.x, mouse.y), false)
                    startCoord = root.dragCoordinate.isValid
                               ? root.dragCoordinate : magneticMapController.northWest
                }
                onPositionChanged: {
                    if (!pressed || !root.mapControl || !startCoord || !startCoord.isValid) return
                    var current = root.mapControl.toCoordinate(
                                root.mapControl.mapFromItem(parent, mouse.x, mouse.y), false)
                    if (!current || !current.isValid) return
                    root.dragCoordinate = QtPositioning.coordinate(
                                startCoord.latitude + current.latitude - startMapPoint.latitude,
                                startCoord.longitude + current.longitude - startMapPoint.longitude)
                }
            }
        }
    }

    Rectangle {
        id: launcher
        z: QGroundControl.zOrderTopMost + 10
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.edgeMargin
        anchors.bottomMargin: root.bottomOffset + root.edgeMargin
        width: launcherContent.implicitWidth + ScreenTools.defaultFontPixelWidth * 3.2
        height: ScreenTools.defaultFontPixelHeight * 2.35
        radius: ScreenTools.defaultFontPixelHeight * 0.65
        color: Qt.rgba(qgcPal.window.r, qgcPal.window.g, qgcPal.window.b, 0.78)
        border.width: 1
        border.color: magneticMapController.hasMap && root.overlayEnabled
                      ? "#2E86DE"
                      : Qt.rgba(qgcPal.text.r, qgcPal.text.g, qgcPal.text.b, 0.32)
        visible: !root.pipMode

        Row {
            id: launcherContent
            anchors.centerIn: parent
            spacing: ScreenTools.defaultFontPixelWidth * 0.75

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: ScreenTools.defaultFontPixelHeight * 0.55
                height: width
                radius: width / 2
                color: magneticMapController.hasMap && root.overlayEnabled ? "#2E86DE" : "#7F8C8D"
            }

            QGCLabel {
                id: launcherLabel
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Mag Map")
                font.bold: true
                color: magneticMapController.hasMap && root.overlayEnabled ? "#2E86DE" : qgcPal.text
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.panelOpen = !root.panelOpen
        }
    }

    Rectangle {
        id: panel
        z: QGroundControl.zOrderTopMost + 11
        anchors.left: launcher.left
        anchors.bottom: launcher.top
        anchors.bottomMargin: ScreenTools.defaultFontPixelHeight * 0.55
        width: Math.max(0, Math.min(root.width - root.edgeMargin * 2, root.panelMaximumWidth))
        height: Math.max(0, Math.min(panelColumn.implicitHeight + root.edgeMargin * 2,
                                   launcher.y - root.topInset - root.edgeMargin * 2))
        radius: ScreenTools.defaultFontPixelHeight * 0.6
        color: Qt.rgba(qgcPal.window.r, qgcPal.window.g, qgcPal.window.b, 0.88)
        border.width: 1
        border.color: Qt.rgba(qgcPal.text.r, qgcPal.text.g, qgcPal.text.b, 0.28)
        visible: root.panelOpen && !root.pipMode

        Flickable {
            anchors.fill: parent
            anchors.margins: root.edgeMargin
            contentWidth: width
            contentHeight: panelColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: panelColumn
            width: parent.width
            spacing: ScreenTools.defaultFontPixelHeight * 0.45

            RowLayout {
                Layout.fillWidth: true
                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Offline Magnetic Map")
                    font.family: "Times New Roman"
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
                QGCButton {
                    text: qsTr("Done")
                    onClicked: root.panelOpen = false
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                text: magneticMapController.sourceFile.length > 0
                      ? magneticMapController.sourceFile.split(/[\\/]/).pop()
                      : qsTr("Place CSV/TXT data in: %1").arg(magneticMapController.defaultFolder)
                elide: Text.ElideMiddle
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 2
                font.pointSize: ScreenTools.smallFontPointSize
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.compactLayout ? 2 : 4
                columnSpacing: ScreenTools.defaultFontPixelWidth * 0.6
                rowSpacing: ScreenTools.defaultFontPixelHeight * 0.35
                QGCButton {
                    Layout.fillWidth: true
                    text: qsTr("加载数据")
                    enabled: !magneticMapController.busy
                    onClicked: sourceDialog.openForLoad()
                }
                QGCButton {
                    Layout.fillWidth: true
                    text: root.overlayEnabled ? qsTr("Hide") : qsTr("Show")
                    enabled: magneticMapController.hasMap
                    onClicked: root.overlayEnabled = !root.overlayEnabled
                }
                QGCButton {
                    Layout.fillWidth: true
                    text: qsTr("定位")
                    enabled: magneticMapController.hasMap
                    onClicked: {
                        root.dragCoordinate = QtPositioning.coordinate()
                        root.locateOverlay()
                    }
                }
                QGCButton {
                    Layout.fillWidth: true
                    text: root.dragEnabled ? qsTr("停止移动") : qsTr("移动")
                    enabled: magneticMapController.hasMap && root.overlayEnabled
                    onClicked: root.dragEnabled = !root.dragEnabled
                }
                QGCButton {
                    Layout.fillWidth: true
                    text: qsTr("清除")
                    enabled: magneticMapController.hasMap
                    onClicked: {
                        root.overlayEnabled = false
                        root.dragEnabled = false
                        root.dragCoordinate = QtPositioning.coordinate()
                        magneticMapController.clear()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: ScreenTools.defaultFontPixelWidth
                QGCLabel { text: qsTr("Opacity") }
                Slider {
                    Layout.fillWidth: true
                    from: 0.15
                    to: 0.85
                    stepSize: 0.05
                    value: root.overlayOpacity
                    onValueChanged: root.overlayOpacity = value
                }
                QGCLabel { text: Math.round(root.overlayOpacity * 100) + "%" }
            }

            QGCLabel {
                Layout.fillWidth: true
                visible: magneticMapController.hasMap
                text: qsTr("%1 samples  |  value %2 to %3")
                      .arg(magneticMapController.sampleCount)
                      .arg(magneticMapController.minimumValue.toFixed(2))
                      .arg(magneticMapController.maximumValue.toFixed(2))
                font.pointSize: ScreenTools.smallFontPointSize
            }

            QGCLabel {
                Layout.fillWidth: true
                visible: magneticMapController.errorString.length > 0
                text: magneticMapController.errorString
                color: qgcPal.warningText
                wrapMode: Text.WordWrap
            }

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: magneticMapController.busy
                visible: running
            }
        }
        }
    }

    RowLayout {
        id: magneticLegend
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.bottomOffset + ScreenTools.defaultFontPixelHeight * 3.2
        width: Math.min(root.width - 24, 270)
        height: 18
        spacing: 5
        visible: magneticMapController.hasMap && root.overlayEnabled
        z: QGroundControl.zOrderTopMost
        QGCLabel {
            text: magneticMapController.minimumValue.toFixed(1)
            color: "#FFFFFF"
            style: Text.Outline
            styleColor: "#90000000"
            font.pixelSize: 10
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 8
            radius: 4
            border.width: 1
            border.color: Qt.rgba(1,1,1,0.35)
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.00; color: "#0000FF" }
                GradientStop { position: 0.25; color: "#00FFFF" }
                GradientStop { position: 0.50; color: "#00FF00" }
                GradientStop { position: 0.75; color: "#FFFF00" }
                GradientStop { position: 1.00; color: "#FF0000" }
            }
        }
        QGCLabel {
            text: magneticMapController.maximumValue.toFixed(1)
            color: "#FFFFFF"
            style: Text.Outline
            styleColor: "#90000000"
            font.pixelSize: 10
        }
    }

    QGCFileDialog {
        id: sourceDialog
        title: qsTr("Select magnetic survey data")
        folder: MagLabPaths.surveyDirForMode(MagSurveySettings.mode) || MagLabPaths.rootDir()
        nameFilters: [ qsTr("磁测数据 (*.csv *.txt)"), qsTr("所有文件 (*)") ]
        onAcceptedForLoad: (file) => {
            if (magneticMapController.loadFile(file, 256)) {
                root.dragCoordinate = QtPositioning.coordinate()
                root.overlayEnabled = true
                root.locateOverlay()
            }
            close()
        }
    }
}
