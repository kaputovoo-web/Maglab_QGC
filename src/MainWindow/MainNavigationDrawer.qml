import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// Expandable light navigation drawer opened from the QGC logo.
/// It deliberately covers only the left portion of the window; the map
/// remains visible through a very light dark scrim.
Item {
    id: root

    anchors.fill: parent
    visible:      false
    z:            QGroundControl.zOrderTopMost + 2

    property bool   opened:     false
    property string currentView: "fly"
    property string currentTool: ""
    property bool   advancedUI:  QGroundControl.corePlugin.showAdvancedUI
    property real   _activeY:     0
    property real   _activeH:     0

    readonly property real _panelWidth: Math.max(220, Math.min(240, width - 16))
    readonly property real _topOffset:  ScreenTools.toolbarHeight
    readonly property color _surface:   "#FFFFFF"
    readonly property color _soft:      "#F8FAFC"
    readonly property color _border:    "#E4EBF2"
    readonly property color _text:      "#263445"
    readonly property color _muted:     "#718096"
    readonly property color _accent:    "#008C9E"
    readonly property color _accentSoft: "#E7F6F8"

    signal requestClose()

    function open() {
        closeTimer.stop()
        opened = true
        visible = true
        forceActiveFocus()
    }

    function close() {
        opened = false
        closeTimer.restart()
    }

    function toggle() {
        if (opened) close()
        else open()
    }

    Timer {
        id: closeTimer
        interval: 240
        onTriggered: {
            if (!root.opened) root.visible = false
        }
    }

    // Dimmed, click-through-to-close area below the top toolbar.
    Rectangle {
        id: scrim
        x: 0
        y: root._topOffset
        width: parent.width
        height: parent.height - y
        color: "#190F172A"
        opacity: root.opened ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    // The drawer itself.
    Rectangle {
        id: panel
        x: root.opened ? 0 : -width
        y: root._topOffset
        width: root._panelWidth
        height: parent.height - y
        color: root._surface
        radius: 16
        border.width: 1
        border.color: root._border

        Behavior on x {
            NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
        }

        // A restrained shadow edge; no heavy drop shadow.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -6
            z: -1
            radius: parent.radius + 6
            color: "#100F172A"
        }

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 16
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Image {
                    source:    "qrc:/maglab/heu.svg"
                    width:     28
                    height:    28
                    fillMode:  Image.PreserveAspectFit
                    smooth:     true
                    mipmap:     true
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    QGCLabel {
                        text:       qsTr("Maglab")
                        color:      root._text
                        font.bold:  true
                        font.pointSize: ScreenTools.mediumFontPointSize
                    }

                    QGCLabel {
                        text:       qsTr("Navigation")
                        color:      root._muted
                        font.pointSize: ScreenTools.smallFontPointSize * 0.9
                    }
                }

                Button {
                    objectName: "mainNavigation_closeButton"
                    width:      32
                    height:     32
                    padding:    0
                    flat:       true
                    hoverEnabled: !ScreenTools.isMobile
                    onClicked:  root.close()

                    background: Rectangle {
                        radius: width / 2
                        color: parent.hovered ? "#F1F5F9" : "transparent"
                    }

                    contentItem: QGCLabel {
                        text:               "×"
                        color:              root._muted
                        font.pointSize:     ScreenTools.largeFontPointSize
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment:   Text.AlignVCenter
                    }
                }
            }

            Item { Layout.preferredHeight: 14 }

            // Menu groups. Keep the existing navigation entry points and
            // objectNames so QML UI tests continue to find them.
            ColumnLayout {
                id: menuColumn
                Layout.fillWidth: true
                spacing: 0

            Repeater {
                model: [
                    { section: "FLIGHT", label: qsTr("Fly View"), objectName: "toolbar_viewFly", icon: "/qmlimages/PaperPlane.svg", key: "fly" },
                    { section: "FLIGHT", label: qsTr("Plan View"), objectName: "toolbar_viewPlan", icon: "/qmlimages/Plan.svg", key: "plan" },

                    { section: "CONFIGURATION", label: qsTr("Vehicle Setup"), objectName: "toolbar_viewConfigure", icon: "/res/GearWithPaperPlane.svg", key: "configure" },
                    { section: "CONFIGURATION", label: qsTr("Application Settings"), objectName: "toolbar_viewSettings", icon: "qrc:/maglab/heu.svg", key: "settings", visible: !QGroundControl.corePlugin.options.combineSettingsAndSetup },

                    { section: "TOOLS", label: qsTr("Analyze"), objectName: "toolbar_viewAnalyze", icon: "/qmlimages/Analyze.svg", key: "analyze", visible: root.advancedUI }
                ]

                delegate: ColumnLayout {
                    id: menuDelegate
                    Layout.fillWidth: true
                    spacing: 0

                    required property int index
                    required property var modelData

                    property bool _showSection: index === 0 || index === 2 || (index === 4 && root.advancedUI)

                    QGCLabel {
                        Layout.fillWidth: true
                        Layout.topMargin: index === 0 ? 0 : 12
                        Layout.bottomMargin: 5
                        visible: menuDelegate._showSection
                        text: modelData.section
                        color: root._muted
                        font.pointSize: ScreenTools.smallFontPointSize * 0.78
                        font.letterSpacing: 0.8
                    }

                    Button {
                        id: menuButton
                        objectName: modelData.objectName
                        Layout.fillWidth: true
                        height: 44
                        visible: modelData.visible !== false
                        hoverEnabled: !ScreenTools.isMobile
                        padding: 0

                        property bool selected: {
                            if (modelData.key === "fly") return root.currentView === "fly"
                            if (modelData.key === "plan") return root.currentView === "plan"
                            if (modelData.key === "configure") return root.currentView === "tool" && root.currentTool === qsTr("Vehicle Configuration")
                            if (modelData.key === "settings") return root.currentView === "tool" && (root.currentTool === qsTr("Application Settings") || root.currentTool === qsTr("Settings"))
                            if (modelData.key === "analyze") return root.currentView === "tool" && root.currentTool === qsTr("Analyze Tools")
                            return false
                        }

                        onSelectedChanged: {
                            if (selected) {
                                var p = mapToItem(menuColumn, 0, 0)
                                root._activeY = p.y
                                root._activeH = height
                            }
                        }

                        Component.onCompleted: {
                            if (selected) {
                                root._activeY = mapToItem(menuColumn, 0, 0).y
                                root._activeH = height
                            }
                        }

                        onClicked: {
                            if (!mainWindow.allowViewSwitch()) return
                            root.close()
                            switch (modelData.key) {
                            case "fly":     mainWindow.showFlyView(); break
                            case "plan":    mainWindow.showPlanView(); break
                            case "configure": mainWindow.showVehicleConfig(); break
                            case "settings":  mainWindow.showSettingsTool(); break
                            case "analyze":   mainWindow.showAnalyzeTool(); break
                            }
                        }

                        background: Rectangle {
                            radius: 10
                            color: menuButton.selected ? root._accentSoft
                                   : (menuButton.hovered ? root._soft : "transparent")
                            border.width: menuButton.selected ? 1 : 0
                            border.color: "#9DDCE3"
                        }

                        contentItem: RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 11

                            QGCColoredImage {
                                source:     modelData.icon
                                width:      19
                                height:     19
                                fillMode:   Image.PreserveAspectFit
                                color:      menuButton.selected ? root._accent : root._muted
                            }

                            QGCLabel {
                                Layout.fillWidth: true
                                text:       modelData.label
                                color:      menuButton.selected ? root._accent : root._text
                                font.weight: menuButton.selected ? 650 : 400
                                font.pointSize: ScreenTools.defaultFontPointSize
                            }

                            QGCLabel {
                                visible:    menuButton.selected
                                text:       "›"
                                color:      root._accent
                                font.pointSize: ScreenTools.mediumFontPointSize
                            }
                        }
                    }
                }
            }
            }

            Item { Layout.fillHeight: true }

            Image {
                Layout.fillWidth: true
                Layout.preferredHeight: width / 6.8
                Layout.bottomMargin: 10
                source: "qrc:/maglab/xunxun.png"
                fillMode: Image.PreserveAspectFit
                opacity: 0.52
                smooth: true
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: root._border
                Layout.bottomMargin: 12
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                QGCLabel {
                    Layout.fillWidth: true
                    text:       QGroundControl.appName + "  " + QGroundControl.qgcVersion
                    color:      root._muted
                    font.pointSize: ScreenTools.smallFontPointSize * 0.82
                    elide:      Text.ElideRight
                }

                Button {
                    objectName: "toolbar_viewClose"
                    text:       qsTr("Close")
                    flat:       true
                    padding:    6
                    hoverEnabled: !ScreenTools.isMobile
                    onClicked: {
                        if (!mainWindow.allowViewSwitch()) return
                        root.close()
                        mainWindow.close()
                    }

                    contentItem: QGCLabel {
                        text:       qsTr("Close")
                        color:      parent.hovered ? "#D64545" : root._muted
                        font.pointSize: ScreenTools.smallFontPointSize * 0.9
                    }
                }
            }
        }

        // Scrollspy-style active rail. The Drawer uses the current view
        // state instead of a web IntersectionObserver; the marker still
        // follows the active directory entry and moves smoothly.
        Rectangle {
            id: menuRail
            x: 7
            y: content.y + menuColumn.y
            width: 2
            height: menuColumn.height
            radius: 1
            color: root._border
            z: 1
        }

        Rectangle {
            id: activeMenuRail
            x: 6
            y: content.y + menuColumn.y + root._activeY + (root._activeH - Math.min(root._activeH, 24)) / 2
            width: 4
            height: Math.min(root._activeH, 24)
            radius: 2
            color: root._accent
            z: 2
            opacity: root._activeH > 0 ? 1 : 0

            Behavior on y {
                NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
            }
            Behavior on height {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
        }
    }

    Keys.onEscapePressed: (event) => {
        if (root.opened) {
            root.close()
            event.accepted = true
        }
    }

    // Keep focus on the drawer so Escape is handled even when the pointer is
    // over the scrim.
    focus: opened
    activeFocusOnTab: true
}
