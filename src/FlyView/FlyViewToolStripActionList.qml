import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Viewer3D

ToolStripActionList {
    id: _root

    signal displayPreFlightChecklist
    signal displayMaglab

    model: [
        Viewer3DShowAction { },
        ToolStripAction {
            objectName: "flyToolStrip_maglabButton"
            text: qsTr("MagLab")
            iconSource: "qrc:/maglab/MaglabIcon.svg"
            visible: QGroundControl.corePlugin.maglabModulesEnabled
            onTriggered: _root.displayMaglab()
        },
        PreFlightCheckListShowAction { onTriggered: displayPreFlightChecklist() },
        GuidedActionTakeoff { },
        GuidedActionLand { },
        GuidedActionRTL { },
        GuidedActionPause { },
        FlyViewAdditionalActionsButton { },
        FlyViewGripperButton { }
    ]
}
