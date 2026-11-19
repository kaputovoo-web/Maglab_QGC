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
    readonly property real fitZoom: (originalSize.width > 0 && originalSize.height > 0 && width > 0 && height > 0)
                                     ? Math.min(width / originalSize.width, height / originalSize.height)
                                     : 1.0

    signal imageClicked(real x, real y, real width, real height)
    function resetView() {
        zoom = fitZoom
        flick.contentX = 0
        flick.contentY = 0
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

        Flickable {
            id: flick
            anchors.fill: parent
            contentWidth:  Math.max(width, previewImage.width)
            contentHeight: Math.max(height, previewImage.height)
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Image {
                id: previewImage
                x: Math.max(0, (flick.width - width) / 2)
                y: Math.max(0, (flick.height - height) / 2)
                width:  Math.max(1, root.originalSize.width * root.zoom)
                height: Math.max(1, root.originalSize.height * root.zoom)
                source: root.source
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

        PinchHandler {
            target: flick
            onActiveChanged: {
                if (!active) {
                    root.zoom = Math.max(root.minZoom, Math.min(root.maxZoom, root.zoom * scale))
                }
            }
        }

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            width: 92
            height: 32
            radius: 8
            color: Qt.rgba(1, 1, 1, 0.82)
            border.width: 1
            border.color: "#DCE6EF"

            QGCLabel {
                anchors.centerIn: parent
                text: qsTr("重置缩放")
                color: "#475569"
                font.pointSize: ScreenTools.smallFontPointSize * 0.82
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.resetView()
            }
        }

        QGCLabel {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 8
            text: root.coordinateText
            visible: text !== ""
            color: "#334155"
            font.pointSize: ScreenTools.smallFontPointSize * 0.82
            font.family: "Times New Roman"
            font.bold: true
            style: Text.Outline
            styleColor: "#FFFFFF"
        }
    }
}
