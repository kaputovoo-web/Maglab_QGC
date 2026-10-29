import QtQuick
import QtQuick.Controls
import QtLocation
import QtPositioning

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightMap
import QGroundControl.PlanView

/// Map visual for the free-hand Sketch item. Hosts the drag-to-draw capture layer
/// (so no changes to the core PlanView are needed) plus the drawn route/area.
Item {
    id: _root
    property var  map
    property bool interactive: true
    signal clicked(int sequenceNumber)

    property bool _cur: object.isCurrentItem

    Component.onCompleted: {
        objMgr.createObjects([areaPoly, routeLine], _root.map, true)
        objMgr.createObject(markersView, _root.map, false)
        _captureObj = captureComp.createObject(_root.map)
    }
    Component.onDestruction: {
        objMgr.destroyObjects()
        if (_captureObj) { _captureObj.destroy(); _captureObj = null }
    }
    property var _captureObj: null
    QGCDynamicObjectManager { id: objMgr }

    // Survey-area fill (closed polygon) shown in SurveyArea mode.
    Component {
        id: areaPoly
        MapPolygon {
            z: QGroundControl.zOrderMapItems - 1
            visible: object.drawMode === 0 && object.path.length >= 3
            color: Qt.rgba(1.0, 0.45, 0.26, 0.20)
            border.color: "#FF7043"
            border.width: 2
            path: object.path
        }
    }

    // Drawn route polyline (both modes).
    Component {
        id: routeLine
        MapPolyline {
            z: QGroundControl.zOrderWaypointLines
            line.width: 3
            line.color: object.drawing ? "#29B6F6" : "#FF7043"
            path: object.path
            opacity: _root.opacity
        }
    }

    // Numbered, selectable, draggable waypoint markers (Waypoints mode).
    Component {
        id: markersView
        MapItemView {
            model: object.path
            delegate: MapQuickItem {
                z: QGroundControl.zOrderMapItems + 1
                coordinate: modelData
                visible: object.drawMode === 1 && _root._cur
                anchorPoint.x: mk.width / 2
                anchorPoint.y: mk.height / 2
                sourceItem: Rectangle {
                    id: mk
                    width:  ScreenTools.defaultFontPixelHeight * 1.4
                    height: width
                    radius: width / 2
                    color: index === object.selectedIndex ? "#29B6F6" : "#FF7043"
                    border.color: "white"; border.width: 1
                    QGCLabel {
                        anchors.centerIn: parent
                        text: index + 1
                        color: "white"
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: !object.drawArmed && _root._cur
                        onClicked: object.selectedIndex = index
                        onPositionChanged: {
                            if (pressed && _root.map) {
                                var p = mapToItem(_root.map, mouseX, mouseY)
                                var c = _root.map.toCoordinate(Qt.point(p.x, p.y), false)
                                if (c && c.isValid) { object.movePoint(index, c) }
                            }
                        }
                    }
                }
            }
        }
    }

    // Full-map drag capture: active only while armed and this item is current.
    Component {
        id: captureComp
        MouseArea {
            anchors.fill: parent
            enabled: object.drawArmed && _root._cur
            visible: enabled
            preventStealing: true
            z: 1000
            hoverEnabled: false
            onPressed: {
                object.beginStroke()
                object.addPoint(_root.map.toCoordinate(Qt.point(mouse.x, mouse.y), false))
            }
            onPositionChanged: {
                if (pressed) { object.addPoint(_root.map.toCoordinate(Qt.point(mouse.x, mouse.y), false)) }
            }
            onReleased: { object.endStroke() }
        }
    }
}
