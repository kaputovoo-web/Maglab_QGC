import QtQuick
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls

Button {
    id:             control
    objectName:     toolStripAction ? toolStripAction.objectName : ""
    width:          contentLayoutItem.contentWidth + (contentMargins * 2)
    height:         width
    hoverEnabled:   !ScreenTools.isMobile
    enabled:        toolStripAction ? toolStripAction.enabled : true
    visible:        toolStripAction ? toolStripAction.visible : true
    imageSource:    (toolStripAction && modelData) ? (toolStripAction.showAlternateIcon ? modelData.alternateIconSource : modelData.iconSource) : ""
    text:           toolStripAction ? toolStripAction.text : ""
    checked:        toolStripAction ? toolStripAction.checked : false
    checkable:      toolStripAction ? (toolStripAction.dropPanelComponent || (modelData && modelData.checkable)) : false

    property var    toolStripAction:    undefined
    property var    dropPanel:          undefined
    property alias  radius:             buttonBkRect.radius
    property alias  fontPointSize:      innerText.font.pointSize
    property alias  imageSource:        innerImage.source
    property alias  contentWidth:       innerText.contentWidth

    property bool forceImageScale11: false
    property bool lightStyle:       false
    property bool showText:         true
    property real imageScale:        lightStyle ? 0.72 : (forceImageScale11 && (text == "") ? 0.8 : 0.6)
    property real contentMargins:    lightStyle ? 5 : innerText.height * 0.1

    property color _currentContentColor:  lightStyle
                                              ? ((checked || pressed) ? "#008C9E" : "#64748B")
                                              : ((checked || pressed) ? qgcPal.buttonHighlightText : qgcPal.text)
    property color _currentContentColorSecondary:  lightStyle
                                                      ? "#94A3B8"
                                                      : ((checked || pressed) ? qgcPal.text : qgcPal.buttonHighlight)
    property bool _groupStart: toolStripAction ? !!toolStripAction.groupStart : false

    signal dropped(int index)

    onCheckedChanged: { if (toolStripAction) toolStripAction.checked = checked }

    onClicked: {
        if (mainWindow.allowViewSwitch()) {
            dropPanel.hide()
            if (!toolStripAction.dropPanelComponent) {
                toolStripAction.triggered(this)
            } else if (checked) {
                var panelEdgeTopPoint = mapToItem(_root, width, 0)
                dropPanel.show(panelEdgeTopPoint, toolStripAction.dropPanelComponent, this)
                checked = true
                control.dropped(index)
            }
        } else if (checkable) {
            checked = !checked
        }
    }

    QGCPalette { id: qgcPal; colorGroupEnabled: control.enabled }

    ToolTip.visible: hovered && text !== "" && !ScreenTools.isMobile
    ToolTip.text: text
    ToolTip.delay: 500

    contentItem: Item {
        id:                 contentLayoutItem
        anchors.fill:       parent
        anchors.margins:    contentMargins

        Column {
            anchors.centerIn:   parent
            spacing:            0

            Image {
                id:                         innerImageColorful
                height:                     contentLayoutItem.height * imageScale
                width:                      contentLayoutItem.width  * imageScale
                smooth:                     true
                mipmap:                     true
                fillMode:                   Image.PreserveAspectFit
                antialiasing:               true
                sourceSize.height:          height
                sourceSize.width:           width
                anchors.horizontalCenter:   parent.horizontalCenter
                source:                     control.imageSource
                visible:                    source != "" && !!modelData && modelData.fullColorIcon
            }

            QGCColoredImage {
                id:                         innerImage
                height:                     contentLayoutItem.height * imageScale
                width:                      contentLayoutItem.width  * imageScale
                smooth:                     true
                mipmap:                     true
                color:                      _currentContentColor
                fillMode:                   Image.PreserveAspectFit
                antialiasing:               true
                sourceSize.height:          height
                sourceSize.width:           width
                anchors.horizontalCenter:   parent.horizontalCenter
                visible:                    source != "" && !(modelData && modelData.fullColorIcon)

                QGCColoredImage {
                    id:                         innerImageSecondColor
                    source:                     modelData ? modelData.alternateIconSource : ""
                    height:                     contentLayoutItem.height * imageScale
                    width:                      contentLayoutItem.width  * imageScale
                    smooth:                     true
                    mipmap:                     true
                    color:                      _currentContentColorSecondary
                    fillMode:                   Image.PreserveAspectFit
                    antialiasing:               true
                    sourceSize.height:          height
                    sourceSize.width:           width
                    anchors.horizontalCenter:   parent.horizontalCenter
                    visible:                    source != "" && !!modelData && modelData.biColorIcon
                }
            }

            QGCLabel {
                id:                         innerText
                text:                       control.text
                visible:                    control.showText
                height:                     control.showText ? implicitHeight : 0
                color:                      _currentContentColor
                anchors.horizontalCenter:   parent.horizontalCenter
                font.bold:                  !innerImage.visible && !innerImageColorful.visible
                opacity:                    !innerImage.visible ? 0.8 : 1.0
            }
        }
    }

    background: Rectangle {
        id:     buttonBkRect
        color:  control.lightStyle
                    ? ((control.checked || control.pressed) ? Qt.rgba(0.91, 0.97, 0.98, 0.70) :
                       ((control.enabled && control.hovered) ? Qt.rgba(0.95, 0.97, 0.99, 0.45) : "transparent"))
                    : ((control.checked || control.pressed) ?
                           qgcPal.buttonHighlight :
                           ((control.enabled && control.hovered) ? qgcPal.toolStripHoverColor : "transparent"))
        border.width: control.lightStyle && (control.checked || control.pressed) ? 1 : 0
        border.color: Qt.rgba(0.45, 0.78, 0.83, 0.62)

        Rectangle {
            visible: control.lightStyle && control._groupStart
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            height: 1
            color: "#E8EEF4"
        }
    }
}
