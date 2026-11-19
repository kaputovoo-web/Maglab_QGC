import QtQuick
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls

Rectangle {
    id:         _root
    color:      lightStyle ? Qt.rgba(1, 1, 1, 0.58) : qgcPal.windowTransparent
    width:      lightStyle ? buttonSize + 20 : ScreenTools.defaultFontPixelWidth * 7
    height:     Math.min(maxHeight, toolStripColumn.height + (flickable.anchors.margins * 2))
    radius:     lightStyle ? 16 : ScreenTools.defaultFontPixelWidth / 2
    border.width: lightStyle ? 1 : 0
    border.color: Qt.rgba(0.62, 0.72, 0.82, 0.38)

    property alias  model:              repeater.model
    property real   maxHeight           ///< Maximum height for control, determines whether text is hidden to make control shorter
    property var    fontSize:           ScreenTools.smallFontPointSize
    property bool   lightStyle:         false
    property bool   showText:           true
    property real   buttonSize:         42

    property var _dropPanel: dropPanel

    function simulateClick(buttonIndex) {
        var button = toolStripColumn.children[buttonIndex]
        if (button.checkable) {
            button.checked = !button.checked
        }
        button.clicked()
    }

    signal dropped(int index)

    // Keep the map interactive through the transparent gaps around the
    // floating buttons. Individual buttons still consume their own clicks.
    DeadMouseArea {
        anchors.fill:  parent
        visible:       !_root.lightStyle
    }

    QGCFlickable {
        id:                 flickable
        anchors.margins:    lightStyle ? 6 : ScreenTools.defaultFontPixelWidth * 0.4
        anchors.fill:       parent
        contentHeight:      toolStripColumn.height
        flickableDirection: Flickable.VerticalFlick
        clip:               true

        Column {
            id:             toolStripColumn
            anchors.left:   parent.left
            anchors.right:  parent.right
            spacing:        lightStyle ? 4 : ScreenTools.defaultFontPixelWidth * 0.25

            Repeater {
                id: repeater

                ToolStripHoverButton {
                    id:                 buttonTemplate
                    x:                  _root.lightStyle ? (toolStripColumn.width - width) / 2 : 0
                    width:              _root.lightStyle ? _root.buttonSize : toolStripColumn.width
                    height:             _root.lightStyle ? _root.buttonSize : width
                    radius:             _root.lightStyle ? 11 : ScreenTools.defaultFontPixelWidth / 2
                    fontPointSize:      _root.fontSize
                    lightStyle:         _root.lightStyle
                    showText:           _root.showText
                    toolStripAction:    modelData
                    dropPanel:          _dropPanel
                    onDropped: (index) => _root.dropped(index)

                    onCheckedChanged: {
                        // We deal with exclusive check state manually since usinug autoExclusive caused all sorts of crazt problems
                        if (checked) {
                            for (var i=0; i<repeater.count; i++) {
                                if (i != index) {
                                    var button = repeater.itemAt(i)
                                    if (button.checked) {
                                        button.checked = false
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ToolStripDropPanel {
        id:         dropPanel
        toolStrip:  _root
    }
}
