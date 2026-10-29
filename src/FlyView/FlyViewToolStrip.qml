import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView

ToolStrip {
    id: _root

    signal displayPreFlightChecklist
    signal displayMaglab

    FlyViewToolStripActionList {
        id: flyViewToolStripActionList

        onDisplayPreFlightChecklist: _root.displayPreFlightChecklist()
        onDisplayMaglab: _root.displayMaglab()
    }

    model: flyViewToolStripActionList.model
}
