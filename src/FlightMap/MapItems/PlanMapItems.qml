import QtQuick
import QtLocation
import QtPositioning

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightMap
import QGroundControl.PlanView

// Adds visual items associated with the Flight Plan to the map.
// Currently only used by Fly View even though it's called PlanMapItems!
Item {
    id: _root

    property var    map                     ///< Map control to show items on
    property bool   largeMapView            ///< true: map takes up entire view, false: map is in small window
    property var    planMasterController    ///< Reference to PlanMasterController for vehicle
    property var    vehicle                 ///< Vehicle associated with these items

    property var    _map:                       map
    property var    _vehicle:                   vehicle
    property var    _missionController:         planMasterController.missionController
    property var    _geoFenceController:        planMasterController.geoFenceController
    property var    _rallyPointController:      planMasterController.rallyPointController
    property var    _guidedController:          globals.guidedControllerFlyView
    property var    _missionLineViewComponent

    property string fmode: vehicle.flightMode

    // Group the uploaded half-turn LOITER_TURNS items back into a single
    // display path.  The vehicle stores only simple mission items, so this
    // reconstruction is what keeps a spiral from looking like a pile of
    // independent loiter circles in Fly View.
    property int _visualItemsCount: _missionController.visualItems ? _missionController.visualItems.count : 0
    property var _spiralPaths: {
        _visualItemsCount // dependency: force a rebuild when the mission list changes
        return _buildSpiralPaths()
    }

    function _isHalfTurnLoiter(item) {
        return item && item.isSimpleItem && item.command === 18 &&
               isFinite(item.loiterTurns) && Math.abs(item.loiterTurns - 0.5) < 0.01
    }

    function _spiralTangentPoint(center, radius, nextCenter, nextRadius) {
        if (!center || !nextCenter || !center.isValid || !nextCenter.isValid) {
            return center && center.isValid
                    ? center.atDistanceAndAzimuth(radius, 0)
                    : QtPositioning.coordinate()
        }
        var distance = center.distanceTo(nextCenter)
        if (distance < 0.01) {
            return center.atDistanceAndAzimuth(radius, 0)
        }
        // Adjacent spiral arcs are internally tangent. Because the next
        // circle is larger and contains the current one, the shared tangent
        // point is on the far side of the current centre (away from the next
        // centre), i.e. the next-centre bearing plus 180 degrees.
        return center.atDistanceAndAzimuth(radius, center.azimuthTo(nextCenter) + 180)
    }

    function _spiralArcPath(item, startPoint, endPoint, clockwise) {
        var center = item.coordinate
        var radius = Math.abs(item.loiterRadius)
        if (!center || !center.isValid || !(radius > 0)) {
            return []
        }

        var startAngle = startPoint && startPoint.isValid
                ? center.azimuthTo(startPoint)
                : 0
        var endAngle = endPoint && endPoint.isValid
                ? center.azimuthTo(endPoint)
                : startAngle + (clockwise ? 180 : -180)
        var sweep = endAngle - startAngle
        if (clockwise) {
            while (sweep <= 0) sweep += 360
            while (sweep > 360) sweep -= 360
        } else {
            while (sweep >= 0) sweep -= 360
            while (sweep < -360) sweep += 360
        }

        // A half-circle is the expected segment. If the tangent calculation
        // produced a tiny angular error, still draw a full semicircle rather
        // than introducing a visible straight connector.
        if (Math.abs(Math.abs(sweep) - 180) > 25) {
            sweep = clockwise ? 180 : -180
        }

        var path = []
        var steps = Math.max(12, Math.ceil(Math.abs(sweep) / 7.5))
        for (var step = 0; step <= steps; ++step) {
            var angle = startAngle + sweep * step / steps
            path.push(center.atDistanceAndAzimuth(radius, angle))
        }
        // Use the exact shared tangent point for the last sample. This removes
        // sub-metre gaps that otherwise appear as a straight segment between
        // consecutive loiters.
        if (endPoint && endPoint.isValid) {
            path[path.length - 1] = endPoint
        }
        return path
    }

    function _buildSpiralPaths() {
        var items = _missionController.visualItems
        if (!items) {
            return []
        }

        var paths = []
        var current = []
        var entryCoordinate = null

        function appendCurrent() {
            if (current.length < 2) {
                current = []
                return
            }

            var path = []
            var previous = entryCoordinate
            for (var i = 0; i < current.length; ++i) {
                var item = current[i]
                var center = item.coordinate
                var radius = Math.abs(item.loiterRadius)
                if (!center || !center.isValid || !(radius > 0)) {
                    continue
                }

                var startPoint = previous
                if (i === 0) {
                    // First segment: show the approach from the previous item
                    // to the circle edge. Subsequent segments start exactly at
                    // the previous segment's tangent-end point.
                    var entryAngle = previous && previous.isValid
                            ? center.azimuthTo(previous) : 0
                    startPoint = center.atDistanceAndAzimuth(radius, entryAngle)
                }

                var endPoint = null
                if (i + 1 < current.length) {
                    var next = current[i + 1]
                    endPoint = _spiralTangentPoint(center, radius, next.coordinate, Math.abs(next.loiterRadius))
                } else {
                    var startAngle = center.azimuthTo(startPoint)
                    endPoint = center.atDistanceAndAzimuth(radius,
                            startAngle + (item.loiterRadius >= 0 ? 180 : -180))
                }

                var arc = _spiralArcPath(item, startPoint, endPoint, item.loiterRadius >= 0)
                for (var j = 0; j < arc.length; ++j) {
                    path.push(arc[j])
                }
                previous = path[path.length - 1]
            }
            if (path.length > 1) {
                paths.push(path)
            }
            current = []
        }

        for (var i = 0; i < items.count; ++i) {
            var item = items.get(i)
            if (_isHalfTurnLoiter(item)) {
                if (current.length === 0) {
                    entryCoordinate = i > 0 ? items.get(i - 1).coordinate : null
                }
                current.push(item)
            } else {
                appendCurrent()
                entryCoordinate = item.coordinate
            }
        }
        appendCurrent()
        return paths
    }

    // Add the mission item visuals to the map
    Repeater {
        model: largeMapView ? _missionController.visualItems : 0

        delegate: MissionItemMapVisual {
            map:        _map
            vehicle:    _vehicle
            onClicked:  _guidedController.confirmAction(_guidedController.actionSetWaypoint, Math.max(object.sequenceNumber, 1))
        }
    }

    Component.onCompleted: {
        _missionLineViewComponent = missionLineViewComponent.createObject(map)
        if (_missionLineViewComponent.status === Component.Error)
            console.log(_missionLineViewComponent.errorString())
        map.addMapItemGroup(_missionLineViewComponent)
    }

    Component.onDestruction: {
        if (_missionLineViewComponent) {
            // Must remove MapItemGroup before destruction, otherwise we crash on quit
            map.removeMapItemGroup(_missionLineViewComponent)
            _missionLineViewComponent.destroy()
        }
    }

    Component {
        id: missionLineViewComponent

        MapItemGroup {
            MissionLineView {
                model: _missionController.simpleFlightPathSegments
            }

            MapItemView {
                model: _missionController.directionArrows

                delegate: MapLineArrow {
                    fromCoord:      object ? object.coordinate1 : undefined
                    toCoord:        object ? object.coordinate2 : undefined
                    arrowPosition:  3
                    z:              QGroundControl.zOrderWaypointLines + 1
                }
            }

            MapItemView {
                model: _root._spiralPaths

                delegate: MapPolyline {
                    path:       modelData
                    line.width: 3
                    line.color: QGroundControl.globalPalette.mapMissionTrajectory
                    z:          QGroundControl.zOrderWaypointLines
                }
            }
        }
    }
}
