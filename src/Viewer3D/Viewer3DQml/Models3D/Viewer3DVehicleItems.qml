import QGroundControl
import QGroundControl.Controls
import QGroundControl.Viewer3D
import QtQuick
import QtQml.Models
import QtQuick3D

Node {
    id: vehicle3DBody

    property var _altitudeBias: _viewer3DSetting.altitudeBias.rawValue
    property var _backendQml: null
    property Node _camera: null
    property var _missionController: (_planMasterController) ? (_planMasterController.missionController) : (null)
    property var _planMasterController: null
    property bool _rtlActive: false
    property var _vehicle: null
    property double referenceElevation: NaN
    property bool localTerrainMode: false
    property var localTerrain: null
    readonly property double localHomeGround: {
        if (!localTerrainMode || !localTerrain || !_vehicle
            || _vehicle.vehicleLinkManager.communicationLost || !_vehicle.homePosition.isValid)
            return NaN;
        if (localTerrain.elevationState !== Viewer3DTerrainGeometry.Ready)
            return NaN;
        return localTerrain.elevationAt(_vehicle.homePosition);
    }
    property var _viewer3DSetting: QGroundControl.settingsManager.viewer3DSettings
    property alias waypointConeModel: _waypointConeModel
    property alias waypointInstancing: waypointInstancing
    signal taskHeightSelected(real sceneHeight)

    Connections {
        target: waypointInstancing
        function onSelectedIndexChanged() {
            const index = waypointInstancing.selectedIndex;
            const height = index >= 0 && index < missionWaypointListModel.count
                           ? missionWaypointListModel.get(index).z + vehicle3DBody._altitudeBias : NaN;
            vehicle3DBody.taskHeightSelected(height);
        }
    }

    // Resolve scene distances in meters, never user-selected display units.
    function missionSceneAltitude(item, exitAltitude = false) {
        if (!item || !_missionController)
            return NaN;
        const altitude = exitAltitude ? item.amslExitAlt : item.amslEntryAlt;
        const origin = Number.isFinite(referenceElevation) ? referenceElevation
                       : _missionController.plannedHomePosition.altitude;
        return Number.isFinite(altitude) && Number.isFinite(origin) ? altitude - origin : NaN;
    }

    // Coalesce altitude/coordinate bursts into one rebuild, without polling.
    Timer {
        id: missionRefresh
        interval: 0
        onTriggered: {
            vehicle3DBody.addMissionItemsToListModel();
            vehicle3DBody.addSegmentToMissionPathModel();
        }
    }

    Instantiator {
        model: vehicle3DBody._missionController ? vehicle3DBody._missionController.visualItems : null
        delegate: Connections {
            required property var object
            target: object
            ignoreUnknownSignals: true
            function onAmslEntryAltChanged() { missionRefresh.restart(); }
            function onAmslExitAltChanged() { missionRefresh.restart(); }
            function onCoordinateChanged() { missionRefresh.restart(); }
            function onSequenceNumberChanged() { missionRefresh.restart(); }
            function onCommandChanged() { missionRefresh.restart(); }
        }
        onObjectAdded: missionRefresh.restart()
        onObjectRemoved: missionRefresh.restart()
    }

    on_MissionControllerChanged: missionRefresh.restart()
    on_VehicleChanged: missionRefresh.restart()

    function addMissionItemsToListModel() {
        taskHeightSelected(NaN);
        missionWaypointListModel.clear();
        waypointInstancing.selectedIndex = -1;
        waypointInstancing.clear();
        if (!_missionController || localTerrainMode)
            return;
        var _geo2EnuCopy = geo2Enu;
        var launchItemCoordinate = null;
        var _missionItemPrevious = null;
        _rtlActive = false;

        for (var i = 0; i < _missionController.visualItems.count; i++) {
            var _missionItem = _missionController.visualItems.get(i);
            if (isLaunchItem(_missionItem)) {
                launchItemCoordinate = _missionItem.coordinate;
                continue;
            }
            if (isItemAcceptable(_missionItem)) {
                if (isReturnToLaunchItem(_missionItem)) {
                    _rtlActive = true;
                    if (!_missionItemPrevious || !_vehicle || !_vehicle.homePosition.isValid)
                        continue;
                    _geo2EnuCopy.coordinate = _vehicle.homePosition;
                    _geo2EnuCopy.coordinate.altitude = missionSceneAltitude(_missionItemPrevious);
                } else {
                    _geo2EnuCopy.coordinate = _missionItem.coordinate;
                    _geo2EnuCopy.coordinate.altitude = missionSceneAltitude(_missionItem);
                }
                var name = getItemName(_missionItem);
                var lx = _geo2EnuCopy.localCoordinate.x;
                var ly = _geo2EnuCopy.localCoordinate.y;
                var lz = _geo2EnuCopy.coordinate.altitude;
                if (!Number.isFinite(lz))
                    continue;
                missionWaypointListModel.append({
                    "x": lx,
                    "y": ly,
                    "z": lz,
                    "itemName": name,
                    "index": _missionItem.sequenceNumber
                });
                waypointInstancing.addEntry(Qt.vector3d(lx * 10, ly * 10, (lz + _altitudeBias) * 10), Qt.vector3d(0.1, 0.1, 0.1), Quaternion.fromEulerAngles(Qt.vector3d(90, 0, 0)), waypointColor(name));
                if (name === "L" || name === "W") {
                    _missionItemPrevious = _missionItem;
                }
            }
        }
    }

    function addSegmentToMissionPathModel() {
        pathInstancing.clear();
        if (!_missionController || localTerrainMode)
            return;
        var _geo2EnuCopy = geo2Enu;

        var _missionItem;
        var _missionItemPrevious = null;
        var launchItemCoordinate;

        for (var i = 0; i < _missionController.visualItems.count; i++) {
            _missionItem = _missionController.visualItems.get(i);
            if (isLaunchItem(_missionItem)) {
                launchItemCoordinate = _missionItem.coordinate;
                continue;
            }
            if (_missionItem.isTakeoffItem) {
                _missionItemPrevious = _missionItem;
                continue;
            }
            if (_missionItemPrevious === null) {
                continue;
            }
            if (getItemName(_missionItem) === "L" || getItemName(_missionItem) === "W") {
                _geo2EnuCopy.coordinate = _missionItemPrevious.coordinate;
                _geo2EnuCopy.coordinate.altitude = missionSceneAltitude(_missionItemPrevious, true);
                if (!Number.isFinite(_geo2EnuCopy.coordinate.altitude))
                    continue;
                var p1 = Qt.vector3d(_geo2EnuCopy.localCoordinate.x * 10, _geo2EnuCopy.localCoordinate.y * 10, (_geo2EnuCopy.coordinate.altitude + _altitudeBias) * 10);

                if (isReturnToLaunchItem(_missionItem)) {
                    if (!_vehicle || !_vehicle.homePosition.isValid)
                        continue;
                    _geo2EnuCopy.coordinate = _vehicle.homePosition;
                    _geo2EnuCopy.coordinate.altitude = missionSceneAltitude(_missionItemPrevious);
                } else {
                    _geo2EnuCopy.coordinate = _missionItem.coordinate;
                    _geo2EnuCopy.coordinate.altitude = missionSceneAltitude(_missionItem);
                }
                if (!Number.isFinite(_geo2EnuCopy.coordinate.altitude))
                    continue;
                var p2 = Qt.vector3d(_geo2EnuCopy.localCoordinate.x * 10, _geo2EnuCopy.localCoordinate.y * 10, (_geo2EnuCopy.coordinate.altitude + _altitudeBias) * 10);

                var color = isReturnToLaunchItem(_missionItem) ? "red" : "orange";
                pathInstancing.addLineSegment(p1, p2, 8, color);
                _missionItemPrevious = _missionItem;
            }
        }
    }

    // Stable internal identifiers: localization must not change mission logic.
    function getItemName(missionItem) {
        if (isLaunchItem(missionItem) || isReturnToLaunchItem(missionItem))
            return "L";
        if (missionItem.isTakeoffItem)
            return "T";
        if (missionItem.specifiesCoordinate) {
            switch (missionItem.command) {
            case 16: return "W";
            case 22: return "T";
            case 195:
            case 201: return "R";
            }
        }
        return "null";
    }

    function isItemAcceptable(missionItem) {
        const acceptableCmdIds = [16 // Waypoint
            , 20 // Return To Launch
            , 22 //Takeoff
            , 195 // ROI
            , 201 // ROI DEPRECATED
            ,]; // based on MavCmdInfoCommon.json file
        return acceptableCmdIds.includes(missionItem.command);
    }

    onLocalTerrainModeChanged: missionRefresh.restart()

    onReferenceElevationChanged: {
        addMissionItemsToListModel();
        addSegmentToMissionPathModel();
    }

    function isLaunchItem(missionItem) {
        return missionItem.sequenceNumber === 0 && !missionItem.isSimpleItem;
    }

    function isReturnToLaunchItem(missionItem) {
        return missionItem.command === 20;
    }

    function waypointColor(itemName) {
        if (itemName === "T")
            return "green";
        if (itemName === "R")
            return "red";
        if (itemName === "L")
            return "orange";
        return "black";
    }

    Viewer3DGeoCoordinateType {
        id: geo2Enu

        gpsRef: _backendQml.gpsRef
    }

    ListModel {
        id: missionWaypointListModel
    }

    Component.onDestruction: {
        // This is necessary insurance to prevent crashes after destruction when "keepSceneAlive" setting is disabled.
        missionWaypointListModel.clear()
    }

    DroneModelDjiF450 {
        id: droneDji3DModel

        objectName: "viewer3DDroneModel"

        altitudeBias: _altitudeBias
        referenceElevation: vehicle3DBody.referenceElevation
        localTerrainMode: vehicle3DBody.localTerrainMode
        localHomeGround: vehicle3DBody.localHomeGround
        gpsRef: _backendQml.gpsRef
        modelScale: Qt.vector3d(0.05, 0.05, 0.05)
        vehicle: _vehicle
    }

    Viewer3DInstancing {
        id: waypointInstancing

    }

    Model {
        id: _waypointConeModel

        instancing: waypointInstancing
        instancingLodMax: 5000
        opacity: 0.8
        pickable: true
        source: "#Cone"

        materials: DefaultMaterial {
            diffuseColor: "white"
        }
    }

    Repeater3D {
        model: missionWaypointListModel

        delegate: Node {
            position: Qt.vector3d(model.x * 10, model.y * 10, (model.z + _altitudeBias) * 10)

            // Billboard: copy the camera's orientation so the label plane stays
            // parallel to the screen and upright. LookAtNode is unsuitable here
            // because it assumes a Y-up world while this scene is Z-up.
            Node {
                position.x: -6
                position.z: 30
                rotation: vehicle3DBody._camera ? vehicle3DBody._camera.sceneRotation : Qt.quaternion(1, 0, 0, 0)

                // Plain Text (not QGCLabel): QGCLabel sets font.pointSize, and
                // combining that with the fixed pixelSize needed here makes Qt
                // warn "Both point size and pixel size set".
                Text {
                    color: "black"
                    font.family: ScreenTools.normalFontFamily
                    font.pixelSize: 20
                    text: (model.itemName === "W") ? String(model.index) : model.itemName
                }
            }
        }
    }

    Viewer3DInstancing {
        id: pathInstancing

    }

    Model {
        instancing: pathInstancing
        instancingLodMax: 8000
        source: "#Cylinder"

        materials: DefaultMaterial {
            diffuseColor: "white"
        }
    }

    Connections {
        function onPlannedHomePositionChanged() { missionRefresh.restart(); }
        function onVisualItemsReset() {
            addMissionItemsToListModel();
            addSegmentToMissionPathModel();
        }

        enabled: target !== null
        target: _missionController
    }

    Connections {
        function onGpsRefChanged() {
            addMissionItemsToListModel();
            addSegmentToMissionPathModel();
        }

        enabled: target !== null
        target: _backendQml
    }

    Connections {
        function onHomePositionChanged() {
            if (_rtlActive) {
                addMissionItemsToListModel();
                addSegmentToMissionPathModel();
            }
        }

        enabled: target !== null
        target: _vehicle
    }
}
