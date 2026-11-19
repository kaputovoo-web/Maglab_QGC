import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Viewer3D

ToolStripActionList {
    id: _root

    signal displayPreFlightChecklist
    signal displayMaglab
    signal displayMissionReplace

    model: [
        Viewer3DShowAction { },
        PreFlightCheckListShowAction {
            groupStart: true
            onTriggered: displayPreFlightChecklist()
        },
        GuidedActionTakeoff { },
        GuidedActionLand { },
        GuidedActionRTL { },
        GuidedActionPause { },
        ToolStripAction {
            objectName: "flyToolStrip_missionReplaceButton"
            text: qsTr("航线更新")
            iconSource: "/res/waypoint.svg"
            groupStart: true
            visible: QGroundControl.corePlugin.maglabModulesEnabled
            onTriggered: _root.displayMissionReplace()
        },
        ToolStripAction {
            objectName: "flyToolStrip_maglabButton"
            text: qsTr("MagLab")
            iconSource: "qrc:/maglab/heu.svg"
            fullColorIcon: true
            visible: QGroundControl.corePlugin.maglabModulesEnabled
            onTriggered: _root.displayMaglab()
        },
        FlyViewAdditionalActionsButton {
            groupStart: true
        },
        FlyViewGripperButton { }
    ]
}
