import QtQuick
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls

/// Simple zoom/pan preview for a photo overlay or magnetic map.
/// Tap coordinates are converted back to original image pixels.
Item {
    id: root
    implicitWidth:  640
    implicitHeight: 420

    property string source:      ""
    property string emptyText:   qsTr("暂无预览")
    property real   zoom:        1.0
    property real   minZoom:     0.1
    property real   maxZoom:     10.0
    property size   originalSize: Qt.size(0, 0)
    property string coordinateText: ""
    property real   markerX:    -1
    property real   markerY:    -1
    property bool   autoFit:    true
    property bool   showGeo:     false
    property double geoMinLon:   0
    property double geoMaxLon:   0
    property double geoMinLat:   0
    property double geoMaxLat:   0
    property double mapMinValue: 0
    property double mapMaxValue: 0
    property double metersPerPixel: 0
    /// Geographic outline of a survey area to show over the image.
    property var    surveyGeoPolygon: []
    /// Survey outline already projected into raw-image pixels (preferred for
    /// a rotated/oblique photo overlay).
    property var    surveyImagePolygon: []
    /// Survey transect lines already projected into raw-image pixels.
    property var    surveyImageRouteLines: []
    /// While true, dragging on the image selects a survey rectangle.
    property bool   regionSelectionActive: false
    property string coordinateActionText: ""
    property string secondaryCoordinateActionText: ""
    property real   _selectionX: -1
    property real   _selectionY: -1
    property real   _selectionW: 0
    property real   _selectionH: 0

    readonly property real plotLeft:   showGeo ? 142 : 4
    readonly property real plotRight:  showGeo ? 168 : 4
    readonly property real plotTop:    showGeo ? 30 : 4
    readonly property real plotBottom: showGeo ? 108 : 4
    readonly property real _availableW: Math.max(1, width - plotLeft - plotRight)
    readonly property real _availableH: Math.max(1, height - plotTop - plotBottom)
    readonly property real _imageAspect: (originalSize.width > 0 && originalSize.height > 0)
                                          ? originalSize.width / originalSize.height : 16.0 / 9.0
    // The blue plotting frame follows the image aspect ratio. This lets the
    // photo fill the frame exactly at reset zoom without letterboxing or
    // stretching; zooming afterwards crops inside the same frame.
    readonly property real plotWidth:  Math.max(1, Math.min(_availableW, _availableH * _imageAspect))
    readonly property real plotHeight: Math.max(1, plotWidth / _imageAspect)
    readonly property real plotX:      plotLeft + (_availableW - plotWidth) / 2
    readonly property real plotY:      plotTop + (_availableH - plotHeight) / 2
    readonly property real fitZoom: (originalSize.width > 0 && originalSize.height > 0 && plotWidth > 0 && plotHeight > 0)
                                     ? (plotWidth / originalSize.width)
                                     : 1.0

    signal imageClicked(real x, real y, real width, real height)
    signal regionSelected(real x1, real y1, real x2, real y2, real width, real height)
    signal coordinateActionClicked()
    signal secondaryCoordinateActionClicked()
    function resetView() {
        zoom = fitZoom
        Qt.callLater(function() {
            flick.contentX = Math.max(0, (previewImage.width - flick.width) / 2)
            flick.contentY = Math.max(0, (previewImage.height - flick.height) / 2)
        })
        markerX = -1
        markerY = -1
    }

    onOriginalSizeChanged: if (autoFit) resetView()
    onWidthChanged: if (autoFit) resetView()
    onHeightChanged: if (autoFit) resetView()

    Rectangle {
        anchors.fill: parent
        color: "#F8FAFC"
        radius: 10
        border.width: 1
        border.color: "#E4EBF2"
        clip: true

        QGCLabel {
            anchors.centerIn: parent
            text: root.emptyText
            visible: previewImage.status !== Image.Ready || previewImage.source.toString() === ""
            color: "#94A3B8"
        }

        Rectangle {
            id: plotBorder
            visible: root.showGeo
            x: root.plotX
            y: root.plotY
            width: root.plotWidth
            height: root.plotHeight
            color: "transparent"
            border.width: 1.2
            border.color: "#17324D"
            z: 1
        }

        // The Flickable is intentionally anchored to this clipping Item,
        // not to the preview card. plotBorder uses x/y/width/height only.
        Item {
            id: imageClip
            x: root.plotX
            y: root.plotY
            width: root.plotWidth
            height: root.plotHeight
            clip: true

            Flickable {
            id: flick
            anchors.fill: parent
            rotation: 0
            contentWidth:  Math.max(width, previewImage.width)
            contentHeight: Math.max(height, previewImage.height)
            clip: true
            interactive: !root.regionSelectionActive
            boundsBehavior: Flickable.StopAtBounds

            Image {
                id: previewImage
                x: Math.max(0, (flick.width - width) / 2)
                y: Math.max(0, (flick.height - height) / 2)
                width:  Math.max(1, root.originalSize.width * root.zoom)
                height: Math.max(1, root.originalSize.height * root.zoom)
                source: root.source
                rotation: 0
                onSourceChanged: {
                    if (root.source.toString() !== "") {
                        root.originalSize = Qt.size(0, 0)
                        root.zoom = 1.0
                        root.markerX = -1
                        root.markerY = -1
                    }
                }
                asynchronous: true
                cache: false
                smooth: true
                fillMode: Image.PreserveAspectFit
                onStatusChanged: {
                    if (status === Image.Ready && root.originalSize.width <= 1) {
                        root.originalSize = Qt.size(sourceSize.width, sourceSize.height)
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onSingleTapped: (eventPoint, button) => {
                        const px = eventPoint.position.x / Math.max(1, previewImage.width)
                        const py = eventPoint.position.y / Math.max(1, previewImage.height)
                        if (px >= 0 && px <= 1 && py >= 0 && py <= 1) {
                            root.markerX = px * root.originalSize.width
                            root.markerY = py * root.originalSize.height
                            root.imageClicked(px * root.originalSize.width,
                                              py * root.originalSize.height,
                                              root.originalSize.width,
                                              root.originalSize.height)
                        }
                    }
                }

                // Crosshair marker at the last clicked image pixel.
                Item {
                    id: clickMarker
                    visible: root.markerX >= 0 && root.markerY >= 0 &&
                             root.originalSize.width > 0 && root.originalSize.height > 0
                    width: 22
                    height: 22
                    x: root.markerX * previewImage.width / Math.max(1, root.originalSize.width) - width / 2
                    y: root.markerY * previewImage.height / Math.max(1, root.originalSize.height) - height / 2

                    Rectangle {
                        anchors.centerIn: parent
                        width: 15
                        height: 15
                        radius: 8
                        color: "transparent"
                        border.width: 2
                        border.color: "#FFFFFF"
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 7
                        height: 7
                        radius: 4
                        color: "#007C8A"
                        border.width: 1
                        border.color: "#FFFFFF"
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        width: 1
                        height: 5
                        color: "#007C8A"
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        width: 1
                        height: 5
                        color: "#007C8A"
                    }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        width: 5
                        height: 1
                        color: "#007C8A"
                    }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        width: 5
                        height: 1
                        color: "#007C8A"
                    }
                }
            }

            // Survey outline in image coordinates. It is a sibling of the
            // Image and follows the same zoom/pan geometry, while remaining
            // clipped to the plotting frame.
            Canvas {
                id: surveyCanvas
                x: previewImage.x
                y: previewImage.y
                width: previewImage.width
                height: previewImage.height
                visible: root.surveyImagePolygon.length >= 3 ||
                         (root.surveyGeoPolygon.length >= 3 &&
                          root.geoMaxLon > root.geoMinLon &&
                          root.geoMaxLat > root.geoMinLat)
                z: 2
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    if (!visible) return
                    ctx.beginPath()
                    if (root.surveyImagePolygon.length >= 3) {
                        for (let i = 0; i < root.surveyImagePolygon.length; ++i) {
                            const point = root.surveyImagePolygon[i]
                            if (i === 0) ctx.moveTo(point.x, point.y); else ctx.lineTo(point.x, point.y)
                        }
                    } else {
                        const lonSpan = root.geoMaxLon - root.geoMinLon
                        const latSpan = root.geoMaxLat - root.geoMinLat
                        for (let i = 0; i < root.surveyGeoPolygon.length; ++i) {
                            const geo = root.surveyGeoPolygon[i]
                            const px = (geo.longitude - root.geoMinLon) / lonSpan * width
                            const py = (root.geoMaxLat - geo.latitude) / latSpan * height
                            if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py)
                        }
                    }
                    ctx.closePath()
                    ctx.fillStyle = "rgba(220, 38, 38, 0.16)"
                    ctx.fill()
                    ctx.strokeStyle = "#DC2626"
                    ctx.lineWidth = 3
                    ctx.stroke()

                    // Projected transect lines (the "comb" pattern).
                    ctx.strokeStyle = "rgba(220, 38, 38, 0.85)"
                    ctx.lineWidth = 2
                    for (let lineIndex = 0;
                         lineIndex < root.surveyImageRouteLines.length; ++lineIndex) {
                        const line = root.surveyImageRouteLines[lineIndex]
                        if (line.length < 2) continue
                        ctx.beginPath()
                        ctx.moveTo(line[0].x, line[0].y)
                        for (let pointIndex = 1; pointIndex < line.length; ++pointIndex) {
                            ctx.lineTo(line[pointIndex].x, line[pointIndex].y)
                        }
                        ctx.stroke()
                    }
                }
                Connections {
                    target: root
                    function onSurveyGeoPolygonChanged() { surveyCanvas.requestPaint() }
                    function onSurveyImagePolygonChanged() { surveyCanvas.requestPaint() }
                    function onSurveyImageRouteLinesChanged() { surveyCanvas.requestPaint() }
                    function onGeoMinLonChanged() { surveyCanvas.requestPaint() }
                    function onGeoMaxLonChanged() { surveyCanvas.requestPaint() }
                    function onGeoMinLatChanged() { surveyCanvas.requestPaint() }
                    function onGeoMaxLatChanged() { surveyCanvas.requestPaint() }
                }
                Connections {
                    target: previewImage
                    function onWidthChanged() { surveyCanvas.requestPaint() }
                    function onHeightChanged() { surveyCanvas.requestPaint() }
                }
            }

            // Drag selection rectangle, in previewImage-local pixels.
            Rectangle {
                x: previewImage.x + root._selectionX
                y: previewImage.y + root._selectionY
                width: root._selectionW
                height: root._selectionH
                visible: root.regionSelectionActive && width > 2 && height > 2
                color: Qt.rgba(0.86, 0.15, 0.15, 0.12)
                border.width: 2
                border.color: "#DC2626"
                z: 8
            }

            MouseArea {
                x: previewImage.x
                y: previewImage.y
                width: previewImage.width
                height: previewImage.height
                enabled: root.regionSelectionActive
                visible: enabled
                z: 9
                cursorShape: Qt.CrossCursor
                onPressed: (mouse) => {
                    root._selectionX = mouse.x
                    root._selectionY = mouse.y
                    root._selectionW = 0
                    root._selectionH = 0
                }
                onPositionChanged: (mouse) => {
                    if (!pressed) return
                    root._selectionW = mouse.x - root._selectionX
                    root._selectionH = mouse.y - root._selectionY
                }
                onReleased: {
                    const x1 = Math.min(root._selectionX, root._selectionX + root._selectionW)
                    const y1 = Math.min(root._selectionY, root._selectionY + root._selectionH)
                    const x2 = Math.max(root._selectionX, root._selectionX + root._selectionW)
                    const y2 = Math.max(root._selectionY, root._selectionY + root._selectionH)
                    root._selectionW = x2 - x1
                    root._selectionH = y2 - y1
                    root._selectionX = x1
                    root._selectionY = y1
                    root.regionSelectionActive = false
                    if (root._selectionW >= 4 && root._selectionH >= 4) {
                        const sx1 = x1 / Math.max(1, previewImage.width) * root.originalSize.width
                        const sy1 = y1 / Math.max(1, previewImage.height) * root.originalSize.height
                        const sx2 = x2 / Math.max(1, previewImage.width) * root.originalSize.width
                        const sy2 = y2 / Math.max(1, previewImage.height) * root.originalSize.height
                        root.regionSelected(sx1, sy1, sx2, sy2,
                                            root.originalSize.width, root.originalSize.height)
                    }
                }
            }
        }
        }

        WheelHandler {
            onWheel: (event) => {
                const oldZoom = root.zoom
                const factor = event.angleDelta.y > 0 ? 1.15 : 0.87
                root.zoom = Math.max(root.minZoom, Math.min(root.maxZoom, root.zoom * factor))
                if (root.zoom !== oldZoom) {
                    // Keep the cursor position roughly stationary while zooming.
                    const px = event.x / Math.max(1, flick.width)
                    const py = event.y / Math.max(1, flick.height)
                    flick.contentX = Math.max(0, px * previewImage.width - event.x)
                    flick.contentY = Math.max(0, py * previewImage.height - event.y)
                }
            }
        }

        // The image is clipped by Flickable to the plotting frame. Wheel zoom
        // keeps the pointer anchor, matching normal scientific plot behavior.
        PinchHandler {
            target: flick
            onActiveChanged: {
                if (!active) {
                    root.zoom = Math.max(root.minZoom, Math.min(root.maxZoom, root.zoom * scale))
                }
            }
        }

        Row {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 8
            spacing: 8

            QGCLabel {
                text: root.coordinateText
                visible: text !== ""
                color: "#334155"
                font.pointSize: ScreenTools.smallFontPointSize * 1.05
                font.family: "Times New Roman"
                font.bold: true
                style: Text.Outline
                styleColor: "#FFFFFF"
            }

            QGCButton {
                text: root.coordinateActionText
                visible: root.coordinateActionText !== ""
                onClicked: root.coordinateActionClicked()
            }

            QGCButton {
                text: root.secondaryCoordinateActionText
                visible: root.secondaryCoordinateActionText !== ""
                onClicked: root.secondaryCoordinateActionClicked()
            }
        }

        // Dynamic geographic chrome. It is drawn in viewport coordinates so
        // axes and labels remain visible while the raster is zoomed/panned.
        Canvas {
            id: geoCanvas
            anchors.fill: parent
            visible: root.showGeo
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                if (!root.showGeo) return

                    const left = root.plotX
                const right = root.plotX + root.plotWidth
                const bottom = root.plotY + root.plotHeight
                const top = root.plotY
                const ticks = 6
                const zoomOut = Math.max(1, root.zoom / Math.max(0.0001, root.fitZoom))
                const lonSpan = root.geoMaxLon - root.geoMinLon
                const latSpan = root.geoMaxLat - root.geoMinLat
                // The visible image fraction follows both zoom and pan, so the
                // tick labels describe the real current viewport.
                const visLeft = Math.max(0, Math.min(1, flick.contentX / Math.max(1, previewImage.width)))
                const visRight = Math.max(0, Math.min(1, (flick.contentX + flick.width) / Math.max(1, previewImage.width)))
                const visTop = Math.max(0, Math.min(1, flick.contentY / Math.max(1, previewImage.height)))
                const visBottom = Math.max(0, Math.min(1, (flick.contentY + flick.height) / Math.max(1, previewImage.height)))
                const lmin = root.geoMinLon + lonSpan * visLeft
                const lmax = root.geoMinLon + lonSpan * visRight
                const bmax = root.geoMaxLat - latSpan * visTop
                const bmin = root.geoMaxLat - latSpan * visBottom

                ctx.font = "bold 18px 'Times New Roman'"
                ctx.fillStyle = "#17324D"
                ctx.strokeStyle = "#17324D"
                ctx.lineWidth = 1.2
                // bottom longitude ticks
                for (let i = 0; i < ticks; ++i) {
                    const t = ticks === 1 ? 0 : i / (ticks - 1)
                    const x = left + t * (right - left)
                    ctx.beginPath(); ctx.moveTo(x, bottom); ctx.lineTo(x, bottom - 5); ctx.stroke()
                    ctx.save()
                    ctx.translate(x, bottom + 10)
                    ctx.rotate(Math.PI / 4)
                    ctx.fillText((lmin + (lmax - lmin) * t).toFixed(6), 0, 0)
                    ctx.restore()
                }
                // left latitude ticks
                for (let i = 0; i < ticks; ++i) {
                    const t = ticks === 1 ? 0 : i / (ticks - 1)
                    const y = top + t * (bottom - top)
                    ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(left + 5, y); ctx.stroke()
                    ctx.textAlign = "right"
                    ctx.fillText((bmax - (bmax - bmin) * t).toFixed(6), left - 8, y + 5)
                    ctx.textAlign = "left"
                }
                ctx.font = "bold 22px 'Times New Roman'"
                ctx.fillText("Longitude", (left + right) / 2 - 52, height - 22)
                ctx.save()
                ctx.translate(18, (top + bottom) / 2 + 35)
                ctx.rotate(-Math.PI / 2)
                ctx.fillText("Latitude", 0, 0)
                ctx.restore()

                // scale bar, bottom-right, without a white plate
                if (root.metersPerPixel > 0) {
                    const mpp = root.metersPerPixel / Math.max(1, root.zoom / Math.max(0.0001, root.fitZoom))
                    let desired = Math.max(0.001, 120 * mpp)
                    const e = Math.pow(10, Math.floor(Math.log(desired) / Math.LN10))
                    const b = desired / e
                    const nice = b < 1.5 ? 1 : b < 3.5 ? 2 : b < 7.5 ? 5 : 10
                    const meters = nice * e
                    const px = meters / mpp
                    const x2 = right - 18
                    const x1 = x2 - px
                    const y = bottom - 32
                    ctx.strokeStyle = "#007C8A"
                    ctx.lineWidth = 3
                    ctx.beginPath(); ctx.moveTo(x1, y); ctx.lineTo(x2, y)
                    ctx.moveTo(x1, y - 8); ctx.lineTo(x1, y)
                    ctx.moveTo(x2, y - 8); ctx.lineTo(x2, y); ctx.stroke()
                    ctx.fillStyle = "#007C8A"
                    ctx.font = "bold 15px 'Times New Roman'"
                    ctx.fillText(meters.toFixed(meters < 10 ? 1 : 0) + " m", x1 + (px - 30) / 2, y - 10)
                }
            }
            Connections {
                target: root
                function onZoomChanged() { geoCanvas.requestPaint() }
                function onGeoMinLonChanged() { geoCanvas.requestPaint() }
                function onGeoMaxLonChanged() { geoCanvas.requestPaint() }
                function onMetersPerPixelChanged() { geoCanvas.requestPaint() }
            }
            Connections {
                target: flick
                function onContentXChanged() { geoCanvas.requestPaint() }
                function onContentYChanged() { geoCanvas.requestPaint() }
            }
        }

        // Fixed colour bar for the magnetic range.
        Rectangle {
            visible: root.showGeo && root.mapMaxValue > root.mapMinValue
            x: root.plotX + root.plotWidth + 22
            y: root.plotY
            width: 20
            height: Math.max(80, root.plotHeight)
            radius: 2
            border.width: 1
            border.color: "#17324D"
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#C00000" }
                GradientStop { position: 0.25; color: "#FF8000" }
                GradientStop { position: 0.50; color: "#FFF000" }
                GradientStop { position: 0.75; color: "#00C8B4" }
                GradientStop { position: 1.0; color: "#0000C8" }
            }
        }
        QGCLabel {
            visible: root.showGeo && root.mapMaxValue > root.mapMinValue
            x: root.plotX + root.plotWidth + 4
            y: root.plotY - 26
            text: "B (nT)"
            color: "#17324D"
            font.family: "Times New Roman"
            font.bold: true
            font.pixelSize: 22
        }
        QGCLabel {
            visible: root.showGeo && root.mapMaxValue > root.mapMinValue
            x: root.plotX + root.plotWidth + 48
            y: root.plotY
            text: root.mapMaxValue.toFixed(1)
            color: "#17324D"
            font.family: "Times New Roman"
            font.bold: true
            font.pixelSize: 18
        }
        QGCLabel {
            visible: root.showGeo && root.mapMaxValue > root.mapMinValue
            x: root.plotX + root.plotWidth + 48
            y: root.plotY + Math.max(80, root.plotHeight) - 22
            text: root.mapMinValue.toFixed(1)
            color: "#17324D"
            font.family: "Times New Roman"
            font.bold: true
            font.pixelSize: 18
        }

        // Fixed north arrow at the top-left of the plotting frame.
        Item {
            visible: root.showGeo
            x: root.plotX + 10
            y: root.plotY + 10
            width: 40
            height: 46
            z: 3

            Canvas {
                anchors.fill: parent
                onPaint: {
                    const ctx = getContext("2d")
                    const cx = width / 2
                    const cy = height / 2 + 2
                    const r = 11
                    ctx.clearRect(0, 0, width, height)
                    ctx.beginPath()
                    ctx.arc(cx, cy, r + 4, 0, Math.PI * 2)
                    ctx.fillStyle = "rgba(255,255,255,0.72)"
                    ctx.fill()
                    ctx.beginPath()
                    ctx.moveTo(cx, cy - r)
                    ctx.lineTo(cx + 6, cy)
                    ctx.lineTo(cx - 6, cy)
                    ctx.closePath()
                    ctx.fillStyle = "#17324D"
                    ctx.fill()
                    ctx.beginPath()
                    ctx.moveTo(cx, cy + r)
                    ctx.lineTo(cx + 6, cy)
                    ctx.lineTo(cx - 6, cy)
                    ctx.closePath()
                    ctx.fillStyle = "#FFFFFF"
                    ctx.fill()
                    ctx.strokeStyle = "#17324D"
                    ctx.lineWidth = 1
                    ctx.stroke()
                    ctx.fillStyle = "#17324D"
                    ctx.font = "bold 12px 'Times New Roman'"
                    ctx.textAlign = "center"
                    ctx.fillText("N", cx, cy - r - 5)
                }
            }
        }
    }
}
