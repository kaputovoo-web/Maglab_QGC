import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView

ToolStrip {
    id: _root

    signal displayPreFlightChecklist
    signal displayMaglab
    signal displayMissionReplace

    FlyViewToolStripActionList {
        id: flyViewToolStripActionList

        onDisplayPreFlightChecklist: _root.displayPreFlightChecklist()
        onDisplayMaglab: _root.displayMaglab()
        onDisplayMissionReplace: _root.displayMissionReplace()
    }

    model: flyViewToolStripActionList.model
    lightStyle: true
    showText: false
    buttonSize: 42
}
