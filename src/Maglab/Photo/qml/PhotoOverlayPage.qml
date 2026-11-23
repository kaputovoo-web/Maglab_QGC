import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtPositioning

import QGC
import QGroundControl
import QGroundControl.Controls

/// Photo / magnetic-map overlay workspace.
/// The page is intentionally split into a compact source/calibration editor
/// and a large interactive preview, instead of a long form that pushes the
/// result off screen.
Rectangle {
    id: root
    color: "#F6F9FC"

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    readonly property var sensor: SimpleTest
    readonly property var videoManager: QGroundControl.videoManager
    readonly property string sessionName: MissionArchiveManager.currentSessionName
    readonly property color _ink: "#102A43"
    readonly property color _muted: "#627D98"
    readonly property color _line: "#D9E2EC"
    readonly property color _teal: "#008C9E"
    readonly property color _soft: "#F1F5F9"
    readonly property real _gap: ScreenTools.defaultFontPixelWidth * 0.8
    readonly property real _pad: ScreenTools.defaultFontPixelWidth * 0.9
    readonly property real _radius: ScreenTools.defaultFontPixelWidth * 0.8
    readonly property bool _narrow: width < 900

    property var photoModel: []
    property var mapModel: []
    property var dataModel: []
    property int photoIndex: -1
    property int mapIndex: -1
    property int dataIndex: -1
    property string importedPhoto: ""
    property string importedMap: ""
    property string importedData: ""
    property string importedBundle: ""
    property string captureStatus: ""
    property string overlayCoordinateText: ""
    property string mapCoordinateText: ""
    property var selectedCoordinate: QtPositioning.coordinate()
    property real surveyWidth: 20
    property real surveyHeight: 20
    property real surveySpacing: 5
    property var surveyGeoPolygon: []
    property var surveyImagePolygon: []
    property var surveyImageRouteLines: []
    readonly property var mapGeo: selectedMap() !== "" ? overlay.mapGeoInfo(selectedMap()) : ({})
    property int previewMode: 0          // 0 overlay, 1 magnetic map
    property string manualMinValue: ""
    property string manualMaxValue: ""

    PhotoOverlayController { id: overlay }
    Handle { id: mapGenerator }

    Component.onCompleted: refreshLists()
    onVisibleChanged: if (visible) refreshLists()
    onPreviewModeChanged: updateSurveyPreview()

    function refreshLists() {
        photoModel = MagLabPaths.photoFiles().filter(function(path) {
            return overlay.hasPhotoSidecar(path)
        })
        mapModel = MagLabPaths.magMapFiles().filter(function(path) {
            return overlay.hasMagSidecar(path)
        })
        dataModel = MagLabPaths.surveyFiles()
        if (photoIndex < 0 && photoModel.length > 0) photoIndex = 0
        if (mapIndex < 0 && mapModel.length > 0) mapIndex = 0
        if (dataIndex < 0 && dataModel.length > 0) dataIndex = 0
    }

    function selectedPhoto() {
        if (importedPhoto.length > 0) return importedPhoto
        return photoIndex >= 0 && photoIndex < photoModel.length ? photoModel[photoIndex] : ""
    }
    function selectedData() {
        if (importedData.length > 0) return importedData
        return dataIndex >= 0 && dataIndex < dataModel.length ? dataModel[dataIndex].path : ""
    }
    function selectedMap() {
        if (importedMap.length > 0) return importedMap
        return mapIndex >= 0 && mapIndex < mapModel.length ? mapModel[mapIndex] : ""
    }

    function generateFromData() {
        if (selectedPhoto() === "") { captureStatus = qsTr("请先选择照片"); return }
        if (selectedData() === "") { captureStatus = qsTr("请先选择磁测数据"); return }
        captureStatus = qsTr("正在从数据生成磁图…")
        mapGenerator.selectSource(selectedData())
        mapGenerator.generate()
    }

    function generateSelected() {
        if (selectedPhoto() === "" || selectedMap() === "") {
            captureStatus = qsTr("请先选择照片和磁图")
            return
        }
        const minValue = parseFloat(manualMinValue)
        const maxValue = parseFloat(manualMaxValue)
        if (isFinite(minValue) && isFinite(maxValue) && maxValue > minValue) {
            overlay.generate(selectedPhoto(), selectedMap(), minValue, maxValue, selectedData())
        } else {
            overlay.generate(selectedPhoto(), selectedMap(), NaN, NaN, selectedData())
        }
    }

    function oneClickGenerate() {
        const photo = selectedPhoto()
        if (photo === "") { captureStatus = qsTr("请先选择照片"); return }
        let map = selectedMap()
        if (map === "") map = overlay.findAssociatedMap(photo)
        if (map !== "") {
            importedMap = map
            captureStatus = qsTr("已匹配磁图，正在生成叠加…")
            generateSelected()
            return
        }
        if (selectedData() !== "") {
            captureStatus = qsTr("未找到同任务磁图，正在从数据生成…")
            generateFromData()
            return
        }
        captureStatus = qsTr("未找到同任务磁图或磁测数据，请手动选择")
    }

    function importBundle(file) {
        const bundle = overlay.loadBundle(file)
        if (!bundle.ok) { captureStatus = bundle.error || qsTr("任务包导入失败"); return }
        importedBundle = file
        importedPhoto = bundle.photo || ""
        importedMap = bundle.map || ""
        importedData = bundle.survey || ""
        captureStatus = qsTr("任务包已导入")
        if (importedMap !== "") {
            if (bundle.hasRange) overlay.generate(importedPhoto, importedMap, bundle.minValue, bundle.maxValue, selectedData())
            else overlay.generate(importedPhoto, importedMap, NaN, NaN, selectedData())
        } else if (importedData !== "") {
            mapGenerator.selectSource(importedData)
            mapGenerator.generate()
        } else {
            captureStatus = qsTr("任务包没有磁图或磁测数据")
        }
    }

    function capturePhoto() {
        if (!videoManager.hasVideo) return
        const stamp = Qt.formatDateTime(new Date(), "HHmmss_zzz")
        const path = MagLabPaths.photoDir() + "/" + sessionName + "_photo_" + stamp + ".jpg"
        const sidecar = overlay.writeSidecar(path, sensor.longitude, sensor.latitude,
                                             sensor.altitude, sensor.roll, sensor.pitch, sensor.yaw)
        if (!sidecar) { captureStatus = qsTr("位姿记录失败，未开始截图"); return }
        captureStatus = qsTr("正在从 RTSP 视频流截图…")
        videoManager.grabImage(path)
    }

    function _rectanglePolygon(center, width, height) {
        if (!center || !center.isValid || !(width > 0) || !(height > 0)) return []
        const north = center.atDistanceAndAzimuth(height / 2, 0)
        const south = center.atDistanceAndAzimuth(height / 2, 180)
        return [
            north.atDistanceAndAzimuth(width / 2, -90), // NW
            north.atDistanceAndAzimuth(width / 2,  90), // NE
            south.atDistanceAndAzimuth(width / 2,  90), // SE
            south.atDistanceAndAzimuth(width / 2, -90)  // SW
        ]
    }

    function updateSurveyPreview() {
        surveyGeoPolygon = _rectanglePolygon(selectedCoordinate, surveyWidth, surveyHeight)
        surveyImagePolygon = []
        if (previewMode === 0 && selectedPhoto() !== "" && surveyGeoPolygon.length >= 3) {
            const pixels = []
            for (let i = 0; i < surveyGeoPolygon.length; ++i) {
                const geo = surveyGeoPolygon[i]
                const pixel = overlay.geoToPixel(selectedPhoto(), geo.longitude, geo.latitude)
                if (pixel.valid) pixels.push(Qt.point(pixel.x, pixel.y))
            }
            if (pixels.length === surveyGeoPolygon.length) {
                surveyImagePolygon = pixels
            }
        }

        surveyImageRouteLines = []
        if (previewMode === 0 && selectedPhoto() !== "" &&
                selectedCoordinate && selectedCoordinate.isValid &&
                surveyWidth > 0 && surveyHeight > 0 && surveySpacing > 0) {
            const lines = []
            const halfW = surveyWidth / 2
            const halfH = surveyHeight / 2
            const lineCount = Math.max(1, Math.ceil(surveyHeight / surveySpacing))
            for (let i = 0; i <= lineCount; ++i) {
                const north = selectedCoordinate.atDistanceAndAzimuth(
                            -halfH + i * surveyHeight / lineCount, 0)
                const west = north.atDistanceAndAzimuth(halfW, -90)
                const east = north.atDistanceAndAzimuth(halfW, 90)
                const p1 = overlay.geoToPixel(selectedPhoto(), west.longitude, west.latitude)
                const p2 = overlay.geoToPixel(selectedPhoto(), east.longitude, east.latitude)
                if (p1.valid && p2.valid) {
                    lines.push([Qt.point(p1.x, p1.y), Qt.point(p2.x, p2.y)])
                }
            }
            surveyImageRouteLines = lines
        }
    }

    function _setClickedCoordinate(geo, text) {
        if (geo && geo.valid) {
            selectedCoordinate = QtPositioning.coordinate(geo.lat, geo.lon)
            updateSurveyPreview()
            return true
        }
        selectedCoordinate = QtPositioning.coordinate()
        surveyGeoPolygon = []
        return false
    }

    function updateOverlayCoordinate(x, y, width, height) {
        if (selectedPhoto() === "") return
        const geo = overlay.pixelToGeo(selectedPhoto(), x, y)
        overlayCoordinateText = geo.valid
                ? qsTr("经度 %1   纬度 %2").arg(geo.lon.toFixed(7)).arg(geo.lat.toFixed(7))
                : qsTr("点击位置不在照片有效范围内")
        _setClickedCoordinate(geo, overlayCoordinateText)
    }
    function updateMapCoordinate(x, y, width, height) {
        if (selectedMap() === "") return
        const geo = overlay.mapPixelToGeo(selectedMap(), x, y)
        mapCoordinateText = geo.valid
                ? qsTr("经度 %1   纬度 %2").arg(geo.lon.toFixed(7)).arg(geo.lat.toFixed(7))
                : qsTr("无法计算该位置经纬度")
        _setClickedCoordinate(geo, mapCoordinateText)
    }

    function requestSurveyPlan() {
        if (!selectedCoordinate || !selectedCoordinate.isValid) {
            captureStatus = qsTr("请先在照片或磁图上点击一个有效位置")
            return
        }
        if (!(surveyWidth > 0) || !(surveyHeight > 0) || !(surveySpacing > 0)) {
            captureStatus = qsTr("扫测宽高和间距必须大于 0")
            return
        }
        updateSurveyPreview()
        if (typeof mainWindow !== "undefined" && mainWindow) {
            mainWindow.createSurveyPlan(selectedCoordinate, surveyWidth, surveyHeight, surveySpacing)
        } else {
            captureStatus = qsTr("无法打开航线规划页面")
        }
    }

    function selectSurveyRegion(x1, y1, x2, y2, width, height) {
        const p1 = previewMode === 0
                ? overlay.pixelToGeo(selectedPhoto(), x1, y1)
                : overlay.mapPixelToGeo(selectedMap(), x1, y1)
        const p2 = previewMode === 0
                ? overlay.pixelToGeo(selectedPhoto(), x2, y2)
                : overlay.mapPixelToGeo(selectedMap(), x2, y2)
        if (!p1.valid || !p2.valid) {
            captureStatus = qsTr("框选区域不在有效经纬度范围内")
            return
        }

        const center = QtPositioning.coordinate((p1.lat + p2.lat) / 2,
                                                (p1.lon + p2.lon) / 2)
        const latSpan = Math.abs(p2.lat - p1.lat)
        const lonSpan = Math.abs(p2.lon - p1.lon)
        const midLatRad = center.latitude * Math.PI / 180
        surveyWidth = Math.max(1, lonSpan * 111320 * Math.cos(midLatRad))
        surveyHeight = Math.max(1, latSpan * 111320)
        selectedCoordinate = center

        const north = center.atDistanceAndAzimuth(surveyHeight / 2, 0)
        const south = center.atDistanceAndAzimuth(surveyHeight / 2, 180)
        surveyGeoPolygon = [
            north.atDistanceAndAzimuth(surveyWidth / 2, -90),
            north.atDistanceAndAzimuth(surveyWidth / 2,  90),
            south.atDistanceAndAzimuth(surveyWidth / 2,  90),
            south.atDistanceAndAzimuth(surveyWidth / 2, -90)
        ]
        requestSurveyPlan()
    }

    Connections {
        target: videoManager
        function onScreenshotComplete(success, filename) {
            if (success) {
                captureStatus = qsTr("照片已保存：%1").arg(filename)
                refreshLists()
                for (var i = 0; i < photoModel.length; ++i) if (photoModel[i] === filename) photoIndex = i
            } else captureStatus = qsTr("截图失败")
        }
    }
    Connections {
        target: overlay
        function onFinished(success) { if (success) refreshLists() }
    }
    Connections {
        target: mapGenerator
        function onGenerationFinished() {
            if (mapGenerator.magmapFilePath === "") return
            importedMap = mapGenerator.magmapFilePath
            captureStatus = qsTr("磁图已生成，正在叠加…")
            overlay.generate(selectedPhoto(), mapGenerator.magmapFilePath,
                             mapGenerator.mapMinValue, mapGenerator.mapMaxValue)
        }
        function onErrorStringChanged() {
            if (mapGenerator.errorString.length > 0) captureStatus = mapGenerator.errorString
        }
    }

    Timer {
        id: previewTimer
        interval: 360
        repeat: false
        onTriggered: {
            if (root.selectedPhoto() !== "" && root.selectedMap() !== "" &&
                    overlay.hasPreview && !overlay.busy) {
                root.generateSelected()
            }
        }
    }

    Connections {
        target: overlay
        function onSettingsChanged() {
            if (overlay.hasPreview && !overlay.busy) previewTimer.restart()
        }
    }

    component Card: Rectangle {
        id: card
        default property alias content: body.data
        property bool fillContent: false
        Layout.fillWidth: true
        implicitHeight: fillContent ? 0 : body.implicitHeight + root._pad * 2
        color: "#FFFFFF"
        radius: root._radius
        border.width: 1
        border.color: root._line
        ColumnLayout {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: root._pad
            height: card.fillContent ? parent.height - root._pad * 2 : implicitHeight
            spacing: root._gap
        }
    }
    component SectionTitle: ColumnLayout {
        property string title
        property string hint: ""
        Layout.fillWidth: true
        spacing: 2
        QGCLabel { text: title; color: root._ink; font.bold: true; font.pointSize: ScreenTools.mediumFontPointSize }
        QGCLabel {
            Layout.fillWidth: true
            visible: hint.length > 0
            text: hint
            color: root._muted
            wrapMode: Text.WordWrap
            font.pointSize: ScreenTools.smallFontPointSize
        }
    }
    component FieldLabel: QGCLabel {
        color: root._muted
        font.pointSize: ScreenTools.smallFontPointSize
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root._pad
        spacing: root._gap

        // Header: one clear action strip.
        Card {
            RowLayout {
                Layout.fillWidth: true
                spacing: root._gap
                Rectangle { width: 8; height: 8; radius: 4; color: videoManager.hasVideo ? "#1E9E6A" : "#B7791F" }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    QGCLabel { text: qsTr("照片与磁图叠加"); color: root._ink; font.bold: true; font.pointSize: ScreenTools.largeFontPointSize }
                    QGCLabel {
                        Layout.fillWidth: true
                        text: captureStatus.length > 0 ? captureStatus : qsTr("导入照片或任务包，自动关联位姿、航磁和磁图，然后生成带经纬度与色棒的叠加结果。")
                        color: root._muted
                        wrapMode: Text.WordWrap
                        font.pointSize: ScreenTools.smallFontPointSize * 0.88
                    }
                }
                QGCButton { text: qsTr("拍摄照片"); primary: true; enabled: videoManager.hasVideo && !overlay.busy; onClicked: capturePhoto() }
                QGCButton { text: qsTr("导入任务包"); onClicked: bundleDialog.openForLoad() }
                QGCButton { text: qsTr("刷新"); onClicked: refreshLists() }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: root._narrow ? 1 : 2
            columnSpacing: root._gap
            rowSpacing: root._gap

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: root._narrow ? -1 : 330
                Layout.maximumWidth: root._narrow ? Number.POSITIVE_INFINITY : 380
                spacing: root._gap

                Card {
                    SectionTitle { title: qsTr("数据来源"); hint: qsTr("自动匹配同任务文件；也可以手动选择。") }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: root._gap
                        rowSpacing: root._gap * 0.45
                        FieldLabel { text: qsTr("照片") }
                        QGCComboBox {
                            Layout.fillWidth: true
                            model: photoModel
                            currentIndex: photoIndex
                            onActivated: (index) => { importedPhoto = ""; photoIndex = index }
                        }
                        FieldLabel { text: qsTr("磁测数据") }
                        QGCComboBox {
                            Layout.fillWidth: true
                            model: dataModel
                            textRole: "name"
                            currentIndex: dataIndex
                            onActivated: (index) => { importedData = ""; dataIndex = index }
                        }
                        FieldLabel { text: qsTr("磁图") }
                        QGCComboBox {
                            Layout.fillWidth: true
                            model: mapModel
                            currentIndex: mapIndex
                            onActivated: (index) => { importedMap = ""; mapIndex = index }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: root._gap
                        QGCButton { Layout.fillWidth: true; text: qsTr("一键匹配并生成"); primary: true; enabled: !overlay.busy && selectedPhoto() !== ""; onClicked: oneClickGenerate() }
                        QGCButton { text: qsTr("导入照片"); onClicked: photoDialog.openForLoad() }
                        QGCButton { text: qsTr("导入位姿"); onClicked: poseDialog.openForLoad() }
                    }
                    QGCButton {
                        Layout.fillWidth: true
                        text: mapGenerator.busy ? qsTr("正在生成磁图…") : qsTr("从磁测数据生成磁图并叠加")
                        enabled: !overlay.busy && !mapGenerator.busy && selectedPhoto() !== "" && selectedData() !== ""
                        onClicked: generateFromData()
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        text: qsTr("照片：%1").arg(MagLabPaths.photoDir())
                        color: root._muted; elide: Text.ElideMiddle
                        font.pointSize: ScreenTools.smallFontPointSize * 0.8
                    }
                }

                Card {
                    SectionTitle { title: qsTr("相机与投影"); hint: qsTr("按实际相机和安装角度校准。") }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: root._gap
                        rowSpacing: root._gap * 0.5
                        FieldLabel { text: qsTr("水平 FOV (°)") }
                        QGCTextField {
                            Layout.fillWidth: true; text: overlay.hfov.toFixed(2)
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onEditingFinished: overlay.hfov = Math.max(1, Math.min(179, parseFloat(text) || 71.9))
                        }
                        FieldLabel { text: qsTr("垂直 FOV (°)") }
                        QGCTextField {
                            Layout.fillWidth: true; text: overlay.vfov.toFixed(2)
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onEditingFinished: overlay.vfov = Math.max(1, Math.min(179, parseFloat(text) || 49.3))
                        }
                        FieldLabel { text: qsTr("离地航高 AGL (m)") }
                        QGCTextField {
                            Layout.fillWidth: true; text: overlay.heightAGL.toFixed(2)
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onEditingFinished: overlay.heightAGL = Math.max(0.1, parseFloat(text) || 15)
                        }
                        QGCButton {
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            text: qsTr("从航磁数据读取 AGL")
                            enabled: selectedData() !== ""
                            onClicked: {
                                const value = overlay.readSurveyAgl(selectedData())
                                if (isFinite(value) && value > 0) {
                                    overlay.heightAGL = value
                                    captureStatus = qsTr("已从航磁数据读取 AGL：%1 m").arg(value.toFixed(2))
                                } else {
                                    captureStatus = qsTr("航磁数据中未找到 AGL 字段，请手动输入")
                                }
                            }
                        }
                        FieldLabel { text: qsTr("相机偏航修正 (°)") }
                        QGCTextField {
                            Layout.fillWidth: true; text: overlay.yawOffset.toFixed(2)
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onEditingFinished: overlay.yawOffset = parseFloat(text) || 0
                        }
                        FieldLabel { text: qsTr("磁图透明度") }
                        Slider {
                            Layout.fillWidth: true; from: 0.1; to: 0.9; stepSize: 0.05
                            value: overlay.transparency
                            onValueChanged: overlay.transparency = value
                        }
                        FieldLabel { text: qsTr("B 最小值 (nT)") }
                        QGCTextField {
                            Layout.fillWidth: true
                            placeholderText: qsTr("自动读取磁图")
                            text: manualMinValue
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onEditingFinished: manualMinValue = text
                        }
                        FieldLabel { text: qsTr("B 最大值 (nT)") }
                        QGCTextField {
                            Layout.fillWidth: true
                            placeholderText: qsTr("自动读取磁图")
                            text: manualMaxValue
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onEditingFinished: manualMaxValue = text
                        }
                    }
                }
            }

            Card {
                fillContent: true
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: root._narrow ? 360 : 600
                Layout.preferredHeight: root._narrow ? 0 : Math.max(600, root.height - 155)
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: root._gap

                    RowLayout {
                        Layout.fillWidth: true
                        QGCButton { text: qsTr("叠加结果"); primary: previewMode === 0; onClicked: previewMode = 0 }
                        QGCButton { text: qsTr("磁图"); primary: previewMode === 1; onClicked: previewMode = 1 }
                        Item { Layout.fillWidth: true }

                        Button {
                            id: resetPreviewButton
                            width: 40
                            height: 40
                            padding: 0
                            scale: hovered ? 1.06 : 1.0
                            hoverEnabled: !ScreenTools.isMobile
                            onClicked: previewView.resetView()
                            ToolTip.visible: hovered && !ScreenTools.isMobile
                            ToolTip.text: qsTr("重置缩放")
                            ToolTip.delay: 500

                            background: Item {
                                Rectangle {
                                    anchors.fill: parent
                                    radius: width / 2
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: "#FFFFFF" }
                                        GradientStop { position: 0.45; color: "#EAF5F7" }
                                        GradientStop { position: 1.0; color: "#B7D5DB" }
                                    }
                                    border.width: 1
                                    border.color: "#9FC3CA"
                                }
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.leftMargin: parent.width * 0.20
                                    anchors.topMargin: parent.height * 0.14
                                    width: parent.width * 0.34
                                    height: parent.height * 0.26
                                    radius: width / 2
                                    color: "#FFFFFF"
                                    opacity: 0.78
                                }
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 3
                                    height: parent.height * 0.25
                                    radius: width / 2
                                    color: "#4C8C98"
                                    opacity: 0.10
                                }
                            }
                            contentItem: QGCColoredImage {
                                source: "/InstrumentValueIcons/reload.svg"
                                color: "#31506B"
                                fillMode: Image.PreserveAspectFit
                                width: 20
                                height: 20
                                anchors.centerIn: parent
                            }
                        }
                        QGCButton {
                            text: overlay.busy ? qsTr("正在生成…") : qsTr("生成预览")
                            primary: true
                            enabled: !overlay.busy && selectedPhoto() !== "" && selectedMap() !== ""
                            onClicked: generateSelected()
                        }
                        QGCButton {
                            text: qsTr("保存叠加图")
                            primary: true
                            enabled: overlay.hasPreview && !overlay.busy
                            onClicked: {
                                const path = overlay.saveOutput()
                                if (path.length > 0) captureStatus = qsTr("已保存叠加图：%1").arg(path)
                            }
                        }
                    }

                    ZoomablePreview {
                        id: previewView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 360
                        source: previewMode === 0
                                ? (overlay.previewPath.length > 0 ? ("file://" + overlay.previewPath) : "")
                                : (selectedMap() !== "" ? ("file://" + selectedMap()) : "")
                        originalSize: previewMode === 0
                                      ? Qt.size(overlay.outputWidth, overlay.outputHeight)
                                      : Qt.size(0, 0)
                        emptyText: previewMode === 0 ? qsTr("生成预览后在此显示") : qsTr("选择磁图后在此预览")
                        coordinateText: previewMode === 0 ? overlayCoordinateText : mapCoordinateText
                        coordinateActionText: root.selectedCoordinate.isValid ? qsTr("规划扫测") : ""
                        secondaryCoordinateActionText: qsTr("框选扫测")
                        surveyGeoPolygon: root.surveyGeoPolygon
                        surveyImagePolygon: root.surveyImagePolygon
                        surveyImageRouteLines: root.surveyImageRouteLines
                        onCoordinateActionClicked: requestSurveyPlan()
                        onSecondaryCoordinateActionClicked: {
                            if (previewMode === 0 && selectedPhoto() === "") {
                                captureStatus = qsTr("请先选择照片")
                            } else if (previewMode === 1 && selectedMap() === "") {
                                captureStatus = qsTr("请先选择磁图")
                            } else {
                                previewView.regionSelectionActive = true
                                captureStatus = qsTr("在预览图上拖动框选扫测区域")
                            }
                        }
                        onRegionSelected: (x1, y1, x2, y2, width, height) =>
                            selectSurveyRegion(x1, y1, x2, y2, width, height)
                        showGeo: previewMode === 0 ? overlay.hasPreview : selectedMap() !== ""
                        geoMinLon: previewMode === 0 ? overlay.geoMinLon : (mapGeo.minLon || 0)
                        geoMaxLon: previewMode === 0 ? overlay.geoMaxLon : (mapGeo.maxLon || 0)
                        geoMinLat: previewMode === 0 ? overlay.geoMinLat : (mapGeo.minLat || 0)
                        geoMaxLat: previewMode === 0 ? overlay.geoMaxLat : (mapGeo.maxLat || 0)
                        mapMinValue: previewMode === 0 ? overlay.mapMinValue : (mapGeo.hasRange ? mapGeo.minValue : 0)
                        mapMaxValue: previewMode === 0 ? overlay.mapMaxValue : (mapGeo.hasRange ? mapGeo.maxValue : 0)
                        metersPerPixel: previewMode === 0 ? overlay.metersPerPixel : (mapGeo.metersPerPixel || 0)
                        onImageClicked: (x, y, width, height) => {
                            if (previewMode === 0) updateOverlayCoordinate(x, y, width, height)
                            else updateMapCoordinate(x, y, width, height)
                        }
                    }

                    QGCLabel {
                        Layout.fillWidth: true
                        text: previewMode === 0
                              ? (overlay.outputPath.length > 0 ? overlay.outputPath
                                 : (overlay.previewPath.length > 0 ? qsTr("预览未保存") : qsTr("尚未生成预览")))
                              : (selectedMap().length > 0 ? selectedMap() : qsTr("尚未选择磁图"))
                        color: root._muted; elide: Text.ElideMiddle
                        font.pointSize: ScreenTools.smallFontPointSize * 0.82
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        visible: overlay.errorString.length > 0
                        text: overlay.errorString; color: "#C53030"; wrapMode: Text.WordWrap
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                }
            }
        }
    }

    QGCFileDialog {
        id: photoDialog
        title: qsTr("导入照片")
        folder: MagLabPaths.photoDir()
        nameFilters: [qsTr("图片 (*.jpg *.jpeg *.png)"), qsTr("所有文件 (*)")]
        onAcceptedForLoad: (file) => { importedPhoto = file; captureStatus = qsTr("已导入照片：%1").arg(file); close() }
    }
    QGCFileDialog {
        id: poseDialog
        title: qsTr("导入位姿文件")
        folder: MagLabPaths.photoDir()
        nameFilters: [qsTr("位姿文件 (*.txt)"), qsTr("所有文件 (*)")]
        onAcceptedForLoad: (file) => {
            if (selectedPhoto() !== "" && overlay.importPoseSidecar(selectedPhoto(), file)) captureStatus = qsTr("已导入位姿文件")
            else captureStatus = qsTr("位姿导入失败：请先选择照片")
            close()
        }
    }
    QGCFileDialog {
        id: bundleDialog
        title: qsTr("导入任务包")
        folder: MagLabPaths.rootDir()
        nameFilters: [qsTr("Maglab 任务包 (*.json)"), qsTr("所有文件 (*)")]
        onAcceptedForLoad: (file) => { importBundle(file); close() }
    }
}
