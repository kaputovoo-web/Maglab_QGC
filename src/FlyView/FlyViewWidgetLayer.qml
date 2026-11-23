import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import QtLocation
import QtPositioning
import QtQuick.Window
import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView
import QGroundControl.FlightMap
import QGroundControl.Viewer3D
import QGC

// This is the ui overlay layer for the widgets/tools for Fly View
Item {
    id: _root

    property var    parentToolInsets
    property var    totalToolInsets:        _totalToolInsets
    property var    mapControl
    property var    viewer3DCameraController

    property var    _activeVehicle:         QGroundControl.multiVehicleManager.activeVehicle
    property var    _planMasterController:  globals.planMasterControllerFlyView
    property var    _missionController:     _planMasterController.missionController
    property var    _geoFenceController:    _planMasterController.geoFenceController
    property var    _rallyPointController:  _planMasterController.rallyPointController
    property var    _guidedController:      globals.guidedControllerFlyView
    property real   _margins:               ScreenTools.defaultFontPixelWidth / 2
    property real   _toolsMargin:           ScreenTools.defaultFontPixelWidth * 0.75
    property rect   _centerViewport:        Qt.rect(0, 0, width, height)
    property real   _rightPanelWidth:       ScreenTools.defaultFontPixelWidth * 30
    property real   _layoutMargin:          ScreenTools.defaultFontPixelWidth * 0.75
    property bool   _layoutSpacing:         ScreenTools.defaultFontPixelWidth
    property bool   _showSingleVehicleUI:   true

    QGCToolInsets {
        id:                     _totalToolInsets
        leftEdgeTopInset:       toolStrip.leftEdgeTopInset
        leftEdgeCenterInset:    toolStrip.leftEdgeCenterInset
        leftEdgeBottomInset:    virtualJoystickMultiTouch.visible ? virtualJoystickMultiTouch.leftEdgeBottomInset : parentToolInsets.leftEdgeBottomInset
        rightEdgeTopInset:      topRightPanel.rightEdgeTopInset
        rightEdgeCenterInset:   Math.max(topRightPanel.rightEdgeCenterInset, rightMapControls.width + _toolsMargin)
        rightEdgeBottomInset:   bottomRightRowLayout.rightEdgeBottomInset
        topEdgeLeftInset:       toolStrip.topEdgeLeftInset
        topEdgeCenterInset:     mapScale.topEdgeCenterInset
        topEdgeRightInset:      topRightPanel.topEdgeRightInset
        bottomEdgeLeftInset:    virtualJoystickMultiTouch.visible ? virtualJoystickMultiTouch.bottomEdgeLeftInset : parentToolInsets.bottomEdgeLeftInset
        bottomEdgeCenterInset:  bottomRightRowLayout.bottomEdgeCenterInset
        bottomEdgeRightInset:   virtualJoystickMultiTouch.visible ? virtualJoystickMultiTouch.bottomEdgeRightInset : bottomRightRowLayout.bottomEdgeRightInset
    }

    FlyViewTopRightPanel {
        id:                     topRightPanel
        anchors.top:            parent.top
        anchors.right:          parent.right
        maximumHeight:          parent.height - (bottomRightRowLayout.height + _margins * 4)

        property real topEdgeRightInset:    height + _layoutMargin
        property real rightEdgeTopInset:    width + _layoutMargin
        property real rightEdgeCenterInset: rightEdgeTopInset
    }

    FlyViewTopRightColumnLayout {
        id:                 topRightColumnLayout
        anchors.top:        parent.top
        anchors.right:      parent.right
        spacing:            _layoutSpacing
        visible:           !topRightPanel.visible

        property real topEdgeRightInset:    childrenRect.height + _layoutMargin
        property real rightEdgeTopInset:    width + _layoutMargin
        property real rightEdgeCenterInset: rightEdgeTopInset
    }

    FlyViewBottomRightRowLayout {
        id:                 bottomRightRowLayout
        anchors.bottom:     parent.bottom
        anchors.right:      parent.right
        spacing:            _layoutSpacing

        property real bottomEdgeRightInset:     height + _layoutMargin
        property real bottomEdgeCenterInset:    bottomEdgeRightInset
        property real rightEdgeBottomInset:     width + _layoutMargin
    }

    FlyViewMissionCompleteDialog {
        missionController:      _missionController
        geoFenceController:     _geoFenceController
        rallyPointController:   _rallyPointController
    }

    // Prevent the map's PinchHandler from stealing touch grabs from the joystick pads (issue #13450)
    Binding {
        target:   mapControl
        property: "pinchZoomDisabledByVirtualJoysticks"
        value:    virtualJoystickMultiTouch.visible && virtualJoystickMultiTouch.item && virtualJoystickMultiTouch.item.stickActive
    }

    //-- Virtual Joystick
    Loader {
        id:                         virtualJoystickMultiTouch
        z:                          QGroundControl.zOrderTopMost + 1
        anchors.right:              parent.right
        anchors.rightMargin:        anchors.leftMargin
        height:                     Math.min(parent.height * 0.25, ScreenTools.defaultFontPixelWidth * 16)
        visible:                    _virtualJoystickEnabled && !QGroundControl.videoManager.fullScreen && !(_activeVehicle ? _activeVehicle.usingHighLatencyLink : false)
        anchors.bottom:             parent.bottom
        anchors.bottomMargin:       bottomLoaderMargin
        anchors.left:               parent.left
        anchors.leftMargin:         ( y > toolStrip.y + toolStrip.height ? toolStrip.width / 2 : toolStrip.width * 1.05 + toolStrip.x)
        source:                     "qrc:/qml/QGroundControl/FlyView/VirtualJoystick.qml"
        active:                     _virtualJoystickEnabled && !(_activeVehicle ? _activeVehicle.usingHighLatencyLink : false)

        property real bottomEdgeLeftInset:     parent.height-y
        property bool autoCenterThrottle:      QGroundControl.settingsManager.appSettings.virtualJoystickAutoCenterThrottle.rawValue
        property bool leftHandedMode:          QGroundControl.settingsManager.appSettings.virtualJoystickLeftHandedMode.rawValue
        property bool _virtualJoystickEnabled: QGroundControl.settingsManager.appSettings.virtualJoystick.rawValue
        property real bottomEdgeRightInset:    parent.height-y
        property var  _pipViewMargin:          _pipView.visible ? parentToolInsets.bottomEdgeLeftInset + ScreenTools.defaultFontPixelHeight * 2 :
                                               bottomRightRowLayout.height + ScreenTools.defaultFontPixelHeight * 1.5

        property var  bottomLoaderMargin:      _pipViewMargin >= parent.height / 2 ? parent.height / 2 : _pipViewMargin

        // Width is difficult to access directly hence this hack which may not work in all circumstances
        property real leftEdgeBottomInset:  visible ? bottomEdgeLeftInset + width/18 - ScreenTools.defaultFontPixelHeight*2 : 0
        property real rightEdgeBottomInset: visible ? bottomEdgeRightInset + width/18 - ScreenTools.defaultFontPixelHeight*2 : 0
        property real rootWidth:            _root.width
        property var  itemX:                virtualJoystickMultiTouch.x   // real X on screen

        onRootWidthChanged: virtualJoystickMultiTouch.status == Loader.Ready && visible ? virtualJoystickMultiTouch.item.uiTotalWidth = rootWidth : undefined
        onItemXChanged:     virtualJoystickMultiTouch.status == Loader.Ready && visible ? virtualJoystickMultiTouch.item.uiRealX = itemX : undefined

        //Loader status logic
        onLoaded: {
            if (virtualJoystickMultiTouch.visible) {
                virtualJoystickMultiTouch.item.calibration = true
                virtualJoystickMultiTouch.item.uiTotalWidth = rootWidth
                virtualJoystickMultiTouch.item.uiRealX = itemX
            } else {
                virtualJoystickMultiTouch.item.calibration = false
            }
        }
    }

    FlyViewToolStrip {
        id:                     toolStrip
        anchors.left:           parent.left
        anchors.leftMargin:     _toolsMargin
        anchors.top:            parent.top
        anchors.topMargin:      _toolsMargin
        z:                      QGroundControl.zOrderWidgets
        maxHeight:              parent.height - y - parentToolInsets.bottomEdgeLeftInset - _toolsMargin
        visible:                !QGroundControl.videoManager.fullScreen

        onDisplayPreFlightChecklist: {
            if (!preFlightChecklistLoader.active) {
                preFlightChecklistLoader.active = true
            }
            preFlightChecklistLoader.item.open()
        }

        onDisplayMaglab: {
            maglabPanelLoader.active = true
            if (maglabPanelLoader.item) {
                maglabPanelLoader.item.open()
            }
        }

        onDisplayMissionReplace: {
            if (typeof mainWindow !== "undefined" && mainWindow) {
                mainWindow.showPlanViewForInFlightReplace()
            }
        }


        property real topEdgeLeftInset:     visible ? y + height : 0
        property real leftEdgeTopInset:     visible ? x + width : 0
        property real leftEdgeCenterInset:  leftEdgeTopInset
    }

    // Right-side map controls. Follow is session-local and uses the existing
    // map instance; it does not create a second map or persist a setting.
    Column {
        id:                     rightMapControls
        anchors.right:          parent.right
        anchors.rightMargin:    _toolsMargin
        anchors.verticalCenter: parent.verticalCenter
        spacing:                8
        width:                  44
        z:                      QGroundControl.zOrderWidgets

        Button {
            id:                     followVehicleButton
            objectName:             "flyView_followVehicleButton"
            width:                  44
            height:                 44
            hoverEnabled:           !ScreenTools.isMobile
            // Not checkable: checked is a pure binding to the map state.
            // Letting Button toggle itself breaks that binding and can make
            // the button and map disagree while the vehicle is moving.
            checkable:              false
            checked:                mapControl ? mapControl.followVehicle : false
            padding:                0

            property color _accent: "#008C9E"
            property color _icon:   checked ? _accent : "#64748B"
            property color _bg:     checked ? Qt.rgba(0.91, 0.97, 0.98, 0.70) : Qt.rgba(1, 1, 1, 0.60)

            onClicked: {
                if (mapControl) {
                    mapControl.followVehicle = !mapControl.followVehicle
                }
            }

            background: Rectangle {
                radius:          width / 2
                color:           followVehicleButton._bg
                border.width:    1
                border.color:    followVehicleButton.checked ? Qt.rgba(0.45, 0.78, 0.83, 0.62) : Qt.rgba(0.62, 0.72, 0.82, 0.38)
            }

            contentItem: QGCColoredImage {
                source:      "/InstrumentValueIcons/drone.svg"
                color:       followVehicleButton._icon
                fillMode:    Image.PreserveAspectFit
                sourceSize.width:  23
                sourceSize.height: 23
                anchors.centerIn: parent
                width:       23
                height:      23
            }

            ToolTip.visible: hovered && !ScreenTools.isMobile
            ToolTip.text:    checked ? qsTr("取消跟随无人机") : qsTr("跟随无人机")
            ToolTip.delay:   500
        }

        Button {
            id:                     centerVehicleButton
            objectName:             "flyView_centerVehicleButton"
            width:                  44
            height:                 44
            hoverEnabled:           !ScreenTools.isMobile
            enabled:                mapControl && _activeVehicle && _activeVehicle.coordinate.isValid
            padding:                0

            onClicked: {
                if (mapControl && _activeVehicle && _activeVehicle.coordinate.isValid) {
                    // The centre button is a persistent "keep the vehicle
                    // centred" action, not a one-shot jump. Follow mode owns
                    // the centre until the operator pans the map or disables it.
                    mapControl.followVehicle = true
                    mapControl.center = _activeVehicle.coordinate
                }
            }

            background: Rectangle {
                radius:          width / 2
                color:           centerVehicleButton.enabled
                                     ? (centerVehicleButton.hovered ? Qt.rgba(0.93, 0.97, 0.99, 0.54) : Qt.rgba(1, 1, 1, 0.55))
                                     : Qt.rgba(1, 1, 1, 0.28)
                border.width:    1
                border.color:    centerVehicleButton.enabled
                                     ? Qt.rgba(0.62, 0.72, 0.82, 0.38)
                                     : Qt.rgba(0.62, 0.72, 0.82, 0.20)
            }

            contentItem: QGCColoredImage {
                source:      "/qmlimages/MapCenter.svg"
                color:       centerVehicleButton.enabled ? "#64748B" : "#A8B5C2"
                fillMode:    Image.PreserveAspectFit
                sourceSize.width:  21
                sourceSize.height: 21
                anchors.centerIn: parent
                width:       21
                height:      21
            }

            ToolTip.visible: hovered && !ScreenTools.isMobile
            ToolTip.text:    qsTr("无人机居中")
            ToolTip.delay:   500
        }
    }

    MapCompass {
        id:                 mapCompass
        anchors.right:      parent.right
        anchors.rightMargin: _toolsMargin
        anchors.top:        topRightPanel.visible ? topRightPanel.bottom : topRightColumnLayout.bottom
        anchors.topMargin:  _margins
        z:                  QGroundControl.zOrderWidgets
        bearing:             mapControl ? mapControl.bearing : 0
    }

    property bool magneticCurveVisible: false
    onMagneticCurveVisibleChanged: {
        if (magneticCurveLoader.item) {
            magneticCurveLoader.item.visible = magneticCurveVisible
        }
    }

    Loader {
        id: maglabPanelLoader
        active: false
        source: "qrc:/maglab/MagLabPanel.qml"
        onLoaded: {
            item.closed.connect(function() { maglabPanelLoader.active = false })
            item.magneticCurveVisibilityRequested.connect(function(isVisible) {
                magneticCurveLoader.active = true
                magneticCurveVisible = isVisible
                if (magneticCurveLoader.item) {
                    magneticCurveLoader.item.visible = isVisible
                }
            })
            item.open()
        }
    }

    Loader {
        id: magneticCurveLoader
        // Size to the small overlay itself. Anchoring the Loader to the whole
        // Fly View made the loaded Canvas expand to almost the full screen.
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 92
        anchors.rightMargin: 12
        width: item ? item.width : 0
        height: item ? item.height : 0
        z: QGroundControl.zOrderTopMost - 1
        active: QGroundControl.corePlugin.maglabModulesEnabled
        source: active ? "qrc:/maglab/FlightMagneticCurve.qml" : ""
        onLoaded: item.visible = magneticCurveVisible
    }

    VehicleWarnings {
        anchors.centerIn:   parent
        z:                  QGroundControl.zOrderTopMost
    }

    MapScale {
        id:                 mapScale
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom:     parent.bottom
        anchors.bottomMargin: (parentToolInsets ? parentToolInsets.bottomEdgeCenterInset : 0) + _toolsMargin
        mapControl:         _mapControl
        autoHide:           true
        visible:            !ScreenTools.isTinyScreen && QGroundControl.corePlugin.options.flyView.showMapScale && QGCViewer3DManager.displayMode !== QGCViewer3DManager.View3D && mapControl.pipState.state === mapControl.pipState.fullState

        property real topEdgeCenterInset: visible ? y + height : 0
    }

    Viewer3DScaleBar {
        objectName:         "viewer3DScaleBar"
        anchors.left:       toolStrip.right
        anchors.leftMargin: _toolsMargin
        anchors.top:        parent.top
        controller:         _root.viewer3DCameraController
        autoHide:           true
        visible:            !ScreenTools.isTinyScreen && QGroundControl.corePlugin.options.flyView.showMapScale && QGCViewer3DManager.displayMode === QGCViewer3DManager.View3D && !!_root.viewer3DCameraController
    }

    Loader {
        id: preFlightChecklistLoader
        sourceComponent: preFlightChecklistPopup
        active: false
    }

    Component {
        id: preFlightChecklistPopup
        FlyViewPreFlightChecklistPopup {
        }
    }
}
