import QtQuick
import QtQuick.Controls
import QtLocation
import QtPositioning

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightMap
import QGroundControl.PlanView

Item {
    id: _root
    property var map
    property bool interactive: true
    signal clicked(int sequenceNumber)

    property var  _obj: object
    property bool _cur: object.isCurrentItem

    // Rebuild direction arrow at end of path (one arrow, pointing along spiral)
    function _rebuildArrows() {
        _arrowMgr.destroyObjects()
        var pts = object.path
        if (!_root.map || !pts || pts.length < 2) return
        // Arrow from second-to-last point to last point
        var n = pts.length
        var arrow = arrowComponent.createObject(_root.map, {
            "fromCoord": pts[n - 2],
            "toCoord":   pts[n - 1],
            "arrowColor": "white",
            "arrowPosition": 2,
            "visible": _cur,
            "opacity": _root.opacity
        })
        if (arrow) {
            _arrowMgr.addObject(arrow, _root.map)
        }
    }

    Component.onCompleted: {
        objMgr.createObjects([pathLine, centerPt, exitPt, sizeLabel], _root.map, true)
        objMgr.createObject(arcBoundaryMarkers, _root.map, false)
        _rebuildArrows()
    }
    Component.onDestruction: {
        objMgr.destroyObjects()
        _arrowMgr.destroyObjects()
    }

    QGCDynamicObjectManager { id: objMgr }
    QGCDynamicObjectManager { id: _arrowMgr }

    Connections {
        target: object
        function onPathChanged() { _rebuildArrows() }
    }

    // Interactive polygon with vertex-drag handles
    QGCMapPolygonVisuals {
        mapControl:       _root.map
        mapPolygon:       object.surveyAreaPolygon
        interactive:      _cur && _root.interactive
        borderWidth:      1
        borderColor:      "black"
        interiorColor:    QGroundControl.globalPalette.surveyPolygonInterior
        altColor:         QGroundControl.globalPalette.surveyPolygonTerrainCollision
        interiorOpacity:  0.4 * _root.opacity
    }

    // ---- Path polyline ----
    Component {
        id: pathLine
        MapPolyline {
            line.width: 3
            line.color: _cur ? QGroundControl.globalPalette.mapMissionTrajectory : Qt.rgba(1,1,1,0.4)
            z: QGroundControl.zOrderWaypointLines
            path: object.path
            opacity: _root.opacity
        }
    }

    // Exact boundaries of the piecewise-semicircular model. These markers make
    // the radius update at every pi interval visible in Plan View.
    Component {
        id: arcBoundaryMarkers
        MapItemView {
            model: object.arcBoundaryPath
            delegate: MapQuickItem {
                z: QGroundControl.zOrderMapItems + 1
                coordinate: modelData
                visible: _cur
                anchorPoint.x: boundaryMarker.width / 2
                anchorPoint.y: boundaryMarker.height / 2
                sourceItem: Rectangle {
                    id: boundaryMarker
                    width: ScreenTools.defaultFontPixelHeight * 1.15
                    height: width
                    radius: width / 2
                    color: index === 0 ? "#4CAF50" : "#FF9800"
                    border.color: "white"
                    border.width: 1
                    QGCLabel {
                        anchors.centerIn: parent
                        text: index === 0 ? "S" : "π" + index
                        color: "white"
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                }
            }
        }
    }

    // ---- Center ----
    Component {
        id: centerPt
        MapQuickItem {
            z: QGroundControl.zOrderMapItems
            coordinate: object.coordinate
            visible: object.coordinate.isValid
            sourceItem: MissionItemIndexLabel {
                index: object.sequenceNumber; label: "C"; checked: _cur
                onClicked: if (_root.interactive) _root.clicked(object.sequenceNumber)
            }
            MouseArea {
                anchors.fill: parent; enabled: _root.interactive && _cur
                onPositionChanged: (mouse) => {
                    if (!pressed) return
                    if (!enabled || !_root.map) return
                    var position = mapToItem(_root.map, mouse.x, mouse.y)
                    var c = _root.map.toCoordinate(position, false)
                    if (c && c.isValid) object.coordinate = c
                }
            }
        }
    }

    // ---- Area-box size label (tracks a polygon corner, shows the box span) ----
    Component {
        id: sizeLabel
        MapQuickItem {
            z: QGroundControl.zOrderMapItems
            visible: _cur && _root.interactive && object.surveyAreaPolygon.count > 2
            coordinate: {
                var p = object.surveyAreaPolygon.path
                return (p && p.length > 0) ? p[0] : object.coordinate
            }
            anchorPoint.x: 0
            anchorPoint.y: sizeRect.height
            sourceItem: Rectangle {
                id: sizeRect
                color:  Qt.rgba(0, 0, 0, 0.6)
                radius: ScreenTools.defaultFontPixelWidth / 4
                width:  sizeTxt.width  + ScreenTools.defaultFontPixelWidth
                height: sizeTxt.height + ScreenTools.defaultFontPixelHeight * 0.3
                QGCLabel {
                    id: sizeTxt
                    anchors.centerIn: parent
                    color: "white"
                    font.pointSize: ScreenTools.smallFontPointSize
                    text: {
                        var p = object.surveyAreaPolygon.path
                        var c = object.coordinate
                        if (!c.isValid || !p || p.length < 1) return ""
                        var r = c.distanceTo(p[0])
                        return qsTr("范围 ") + Math.round(2 * r) + " m"
                    }
                }
            }
        }
    }

    // ---- Exit ----
    Component {
        id: exitPt
        MapQuickItem {
            z: QGroundControl.zOrderMapItems
            coordinate: object.exitCoordinate
            visible: object.exitCoordinate.isValid && _root.interactive
            sourceItem: MissionItemIndexLabel {
                index: object.lastSequenceNumber; label: "E"; checked: _cur
                onClicked: if (_root.interactive) _root.clicked(object.lastSequenceNumber)
            }
        }
    }

    // ---- Arrow template (created dynamically by _rebuildArrows) ----
    Component {
        id: arrowComponent
        MapLineArrow {
            z: QGroundControl.zOrderWaypointLines + 1
        }
    }
}
