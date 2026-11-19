import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

Button {
    id:             control
    padding:        ScreenTools.defaultFontPixelWidth * 0.75
    hoverEnabled:   !ScreenTools.isMobile
    autoExclusive:  true
    icon.color:     textColor

    property color _accent:       "#008C9E"
    property color _accentSoft:   "#E7F6F8"
    property color _hoverSurface: "#F1F5F9"
    property color _textColor:    "#263445"
    property color textColor: checked || pressed ? _accent : _textColor
    property bool expandable: false
    property bool expanded:   false

    signal toggleExpand()

    QGCPalette {
        id:                 qgcPal
        colorGroupEnabled:  control.enabled
    }

    background: Rectangle {
        color:      checked || pressed ? _accentSoft : (enabled && hovered ? _hoverSurface : "transparent")
        radius:     10
        border.width: checked || pressed ? 1 : 0
        border.color: "#9DDCE3"

        // Settings directory rail. The active segment replaces the neutral
        // 2px track for the current category.
        Rectangle {
            anchors.left:   parent.left
            anchors.top:    parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 8
            anchors.bottomMargin: 8
            width:          2
            radius:         1
            color:          "#E4EBF2"
        }

        Rectangle {
            anchors.left:   parent.left
            anchors.top:    parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 8
            anchors.bottomMargin: 8
            width:          2
            radius:         1
            color:          _accent
            opacity:        checked || pressed ? 1 : 0
        }
    }

    contentItem: RowLayout {
        spacing: ScreenTools.defaultFontPixelWidth

        QGCColoredImage {
            source: control.icon.source
            color:  control.icon.color
            width:  ScreenTools.defaultFontPixelHeight
            height: ScreenTools.defaultFontPixelHeight
        }

        QGCLabel {
            id:                     displayText
            Layout.fillWidth:       true
            text:                   control.text
            color:                  control.textColor
            font.weight:            control.checked || control.pressed ? 650 : 400
            horizontalAlignment:    QGCLabel.AlignLeft
        }

        QGCColoredImage {
            visible:    control.expandable
            source:     "/InstrumentValueIcons/cheveron-right.svg"
            color:      control.textColor
            width:      ScreenTools.defaultFontPixelHeight * 0.75
            height:     width
            rotation:   control.expanded ? 90 : 0

            MouseArea {
                anchors.fill: parent
                anchors.margins: -ScreenTools.defaultFontPixelWidth
                onClicked: control.toggleExpand()
            }
        }
    }
}
