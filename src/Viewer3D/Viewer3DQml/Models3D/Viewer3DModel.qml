import QtQuick
import QtPositioning
import QtQuick3D

import QGroundControl
import QGroundControl.Controls

View3D {
    id: topView

    objectName: "viewer3DView"

    property real topOverlayInset: 0

    readonly property real _viewDistance: 50000
    readonly property var _gpsRef: QGCViewer3DManager.gpsRef
    readonly property bool _mapLoaded: QGCViewer3DManager.mapProvider ? QGCViewer3DManager.mapProvider.mapLoaded : false

    // Not enabled until the DEM and vehicle vertical datums are verified.
    readonly property double _referenceElevation: NaN
    property real _selectedTaskHeight: NaN
    property bool _cutTerrain: false
    readonly property bool _terrainPreview: QGroundControl.settingsManager.viewer3DSettings.terrainPreview.rawValue

    QGCCheckBox {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: ScreenTools.defaultFontPixelWidth
        visible: !topView._terrainPreview && Number.isFinite(topView._selectedTaskHeight)
        text: qsTr("Cut terrain above selected task height (visual only)")
        checked: topView._cutTerrain
        onClicked: topView._cutTerrain = checked
    }

    QGCCheckBox {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: ScreenTools.defaultFontPixelWidth
        anchors.topMargin: topView.topOverlayInset + ScreenTools.defaultFontPixelWidth
        text: qsTr("Simple 3D terrain view")
        checked: topView._terrainPreview
        onClicked: QGroundControl.settingsManager.viewer3DSettings.terrainPreview.rawValue = checked
    }

    QGCLabel {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: ScreenTools.defaultFontPixelWidth
        anchors.topMargin: topView.topOverlayInset + ScreenTools.defaultFontPixelWidth
        width: Math.min(implicitWidth, parent.width * 0.55)
        wrapMode: Text.WordWrap
        text: mapGeometryLoader.item ? mapGeometryLoader.item.elevationStatusText : qsTr("Waiting for map data")
    }

    // Exposed so FlyView can position the scale bar alongside the other widgets
    readonly property Viewer3DCameraController cameraController: _cameraController

    camera: standAloneScene.cameraOne

    environment: SceneEnvironment {
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
        backgroundMode: SceneEnvironment.Color
        clearColor: _skyColor

        fog: Fog {
            color: _skyColor
            depthCurve: 1.0
            depthEnabled: true
            depthFar: _viewDistance
            depthNear: 1000
            enabled: true
        }
    }

    QGCPalette { id: qgcPal }

    readonly property color _skyColor: qgcPal.window
    importScene: CameraLightModel {
        id: standAloneScene

        cameraHeading: cameraController.heading
        cameraTilt: cameraController.tilt
        cameraZoom: cameraController.distance
        orbitCenter: cameraController.orbitCenter
        viewDistance: _viewDistance
    }

    Viewer3DCameraController {
        id: _cameraController

        fieldOfView: standAloneScene.cameraOne.fieldOfView
        viewportSize: Qt.size(topView.width, topView.height)
    }

    function _resetCameraForContent() {
        if (_mapLoaded) {
            cameraController.reset();
        } else {
            // Without an OSM map there is no terrain; a top-down view shows
            // nothing useful. Look at the vehicle from the side, slightly above.
            cameraController.lookAt(Qt.vector3d(0, 0, 0), 0, 75, 400);
        }
    }

    Component.onCompleted: {
        vehicle3DLoader.active = true;
        mapGeometryLoader.active = true;
        _resetCameraForContent();
    }
    on_GpsRefChanged: {
        _selectedTaskHeight = NaN;
        _resetCameraForContent();
    }
    on_MapLoadedChanged: {
        _resetCameraForContent();
    }

    Component {
        id: buildingsGeometryComponent

        Node {
            id: terrainScene

            function reloadTerrain() {
                if (!_terrainTextureManager.textureGeometryDone)
                    return;
                terrainGeometryManager.sectorCount = topView._terrainPreview ? 32 : _terrainTextureManager.tileCount.width;
                terrainGeometryManager.stackCount = topView._terrainPreview ? 32 : _terrainTextureManager.tileCount.height;
                terrainGeometryManager.roiMin = _terrainTextureManager.roiMinCoordinate;
                terrainGeometryManager.roiMax = _terrainTextureManager.roiMaxCoordinate;
                if (topView._terrainPreview)
                    terrainGeometryManager.requestTerrainPreview();
                else
                    terrainGeometryManager.clearElevationData();
            }

            Connections {
                target: topView
                function on_TerrainPreviewChanged() { terrainScene.reloadTerrain(); }
                function on_GpsRefChanged() { terrainScene.reloadTerrain(); }
            }

            Model {
                // Scene uses ENU (Z up) and the existing ten-units-per-meter scale.
                source: "#Rectangle"
                visible: !topView._terrainPreview && Number.isFinite(topView._selectedTaskHeight)
                z: visible ? topView._selectedTaskHeight * 10 : 0
                scale: Qt.vector3d(100, 100, 1)
                materials: PrincipledMaterial {
                    baseColor: "#4080d0"
                    opacity: 0.25
                    alphaMode: PrincipledMaterial.Blend
                    cullMode: Material.NoCulling
                    lighting: PrincipledMaterial.NoLighting
                }
            }

            readonly property var terrain: terrainGeometryManager
            readonly property string elevationStatusText: {
                switch (terrainGeometryManager.elevationState) {
                case Viewer3DTerrainGeometry.Loading:
                    return qsTr("Loading terrain elevation…");
                case Viewer3DTerrainGeometry.Ready:
                    return qsTr("Terrain loaded independently. Vehicle uses AMSL changes anchored at Home — approximate datum alignment, not for obstacle clearance.");
                case Viewer3DTerrainGeometry.Failed:
                    return qsTr("Elevation unavailable — showing a flat map");
                default:
                    return qsTr("Flat satellite map — elevation datum not calibrated");
                }
            }

            Model {
                id: cityMapModel

                scale: Qt.vector3d(10, 10, 10)
                visible: !topView._terrainPreview

                geometry: CityMapGeometry {
                    id: cityMapGeometry

                    mapProvider: QGCViewer3DManager.mapProvider
                    terrain: topView._terrainPreview ? terrainGeometryManager : null
                    modelName: "city_map"
                }
                materials: [
                    PrincipledMaterial {
                        baseColor: "gray"
                        indexOfRefraction: 4.0
                        metalness: 0.1
                        opacity: 1.0
                        roughness: 0.5
                        specularAmount: 1.0
                    }
                ]
            }

            Model {
                id: pointModel

                scale: Qt.vector3d(10, 10, 10)
                visible: true

                geometry: Viewer3DTerrainGeometry {
                    id: terrainGeometryManager

                    refCoordinate: _gpsRef
                }
                materials: CustomMaterial {
                    property bool cutTerrain: !topView._terrainPreview && topView._cutTerrain && Number.isFinite(topView._selectedTaskHeight)
                    property real taskHeight: Number.isFinite(topView._selectedTaskHeight) ? topView._selectedTaskHeight : 0
                    property bool showTaskHeight: !topView._terrainPreview && Number.isFinite(topView._selectedTaskHeight)
                    property TextureInput someTextureMap: TextureInput {
                        texture: Texture {
                            textureData: _terrainTextureManager
                        }
                    }

                    fragmentShader: "/qml/QGroundControl/Viewer3D/ShaderFragment/earthMaterial.frag"
                    vertexShader: "/qml/QGroundControl/Viewer3D/ShaderVertex/earthMaterial.vert"
                }
            }

            Viewer3DTerrainTexture {
                id: _terrainTextureManager

                readonly property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle

                // Home may be unavailable before arming. Use the valid vehicle
                // reference instead of leaving the map empty in that case.
                fallbackCenter: _activeVehicle && _activeVehicle.homePosition.isValid
                                ? _activeVehicle.homePosition : _gpsRef
                mapProvider: QGCViewer3DManager.mapProvider

                onTextureGeometryDoneChanged: {
                    if (textureGeometryDone === true) {
                        terrainScene.reloadTerrain();
                    }
                }
            }
        }
    }

    Loader3D {
        id: mapGeometryLoader

        active: false
        sourceComponent: buildingsGeometryComponent
    }

    Component {
        id: vehicle3DComponent

        Repeater3D {
            model: QGroundControl.multiVehicleManager.vehicles

            delegate: Viewer3DVehicleItems {
                _backendQml: QGCViewer3DManager
                _camera: standAloneScene.cameraOne
                _planMasterController: masterController
                _vehicle: object
                referenceElevation: topView._referenceElevation
                localTerrainMode: topView._terrainPreview
                localTerrain: mapGeometryLoader.item ? mapGeometryLoader.item.terrain : null
                onTaskHeightSelected: function(sceneHeight) {
                    topView._selectedTaskHeight = sceneHeight;
                }

                PlanMasterController {
                    id: masterController

                    Component.onCompleted: startStaticActiveVehicle(object)
                }
            }
        }
    }

    Loader3D {
        id: vehicle3DLoader

        active: false
        sourceComponent: vehicle3DComponent
    }

    Viewer3DCameraControls {
        anchors.fill: parent
        controller: cameraController
    }

    TapHandler {
        function deselectAllWaypoints() {
            if (!vehicle3DLoader.item)
                return;
            for (var i = 0; i < vehicle3DLoader.item.count; i++) {
                vehicle3DLoader.item.objectAt(i).waypointInstancing.selectedIndex = -1;
            }
        }

        onTapped: function (eventPoint) {
            if (!vehicle3DLoader.item)
                return;

            var models = [];
            for (var i = 0; i < vehicle3DLoader.item.count; i++) {
                models.push(vehicle3DLoader.item.objectAt(i).waypointConeModel);
            }

            var results = topView.pickSubset(eventPoint.position.x, eventPoint.position.y, models);
            if (results.length === 0) {
                deselectAllWaypoints();
                return;
            }

            var result = results[0];
            for (var i = 0; i < vehicle3DLoader.item.count; i++) {
                var vehicleItem = vehicle3DLoader.item.objectAt(i);
                if (result.objectHit === vehicleItem.waypointConeModel) {
                    vehicleItem.waypointInstancing.selectedIndex = result.instanceIndex;
                    return;
                }
            }

            deselectAllWaypoints();
        }
    }
}
