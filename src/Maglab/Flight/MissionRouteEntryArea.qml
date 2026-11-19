import QtQuick
import QtQuick.Controls

import QGC
import QGroundControl
import QGroundControl.Controls

/// Invisible gesture layer over the Fly View mission line.
/// Desktop: double-click. Touch: long-press. The hit test is limited to the
/// displayed mission polyline so normal map gestures keep working elsewhere.
Item {
    id: root
    anchors.fill: parent
    visible: QGroundControl.corePlugin.maglabModulesEnabled

    property var map
    property var missionController
    readonly property real hitRadiusPx: Math.max(10, ScreenTools.defaultFontPixelWidth * 0.9)

    function pointSegmentDistance(p, a, b) {
        var vx = b.x - a.x
        var vy = b.y - a.y
        var wx = p.x - a.x
        var wy = p.y - a.y
        var len2 = vx * vx + vy * vy
        var t = len2 > 0 ? (wx * vx + wy * vy) / len2 : 0
        t = Math.max(0, Math.min(1, t))
        var dx = p.x - (a.x + t * vx)
        var dy = p.y - (a.y + t * vy)
        return Math.sqrt(dx * dx + dy * dy)
    }

    function hitsRoute(position) {
        if (!map || !missionController) {
            return false
        }
        var segments = missionController.simpleFlightPathSegments
        if (!segments) {
            return false
        }
        for (var i = 0; i < segments.count; ++i) {
            var segment = segments.get(i)
            if (!segment || !segment.coordinate1 || !segment.coordinate2 ||
                !segment.coordinate1.isValid || !segment.coordinate2.isValid) {
                continue
            }
            var a = map.fromCoordinate(segment.coordinate1, false)
            var b = map.fromCoordinate(segment.coordinate2, false)
            if (pointSegmentDistance(position, a, b) <= hitRadiusPx) {
                return true
            }
        }
        return false
    }

    function openEditor() {
        if (typeof mainWindow !== "undefined" && mainWindow) {
            mainWindow.showPlanViewForInFlightReplace()
        }
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.DragThreshold
        onDoubleTapped: (eventPoint, button) => {
            if (root.hitsRoute(eventPoint.position)) {
                root.openEditor()
            }
        }
        onLongPressed: (eventPoint, button) => {
            if (root.hitsRoute(eventPoint.position)) {
                root.openEditor()
            }
        }
    }
}
