import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGC
import QGroundControl
import QGroundControl.Controls

Rectangle {
    id: root
    color: "transparent"

    readonly property color _navy: "#0B2239"
    readonly property color _cyan: "#00A6B8"
    readonly property color _text: "#132B45"
    readonly property color _muted: "#607086"
    readonly property color _border: "#DCE5EE"
    readonly property real _margin: ScreenTools.defaultFontPixelWidth * 0.6
    readonly property real _cardPad: ScreenTools.defaultFontPixelWidth * 0.9
    readonly property real _radius: ScreenTools.defaultFontPixelWidth * 0.7
    readonly property bool _wide: width >= 640
    property int selectedIndex: fileModel.count > 0 ? 0 : -1
    readonly property string selectedPath: selectedIndex >= 0 && selectedIndex < fileModel.count
                                          ? fileModel.get(selectedIndex).path
                                          : ""

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    function reload() {
        fileModel.clear()
        var files = MagLabPaths.magMapFiles()
        for (var i = 0; i < files.length; ++i) {
            var path = String(files[i])
            var parts = path.split("/")
            fileModel.append({ "name": parts[parts.length - 1], "path": path })
        }
        root.selectedIndex = fileModel.count > 0 ? 0 : -1
    }

    Component.onCompleted: reload()

    ListModel { id: fileModel }

    ColumnLayout {
        anchors.fill: parent
        spacing: root._margin

        RowLayout {
            Layout.fillWidth: true
            spacing: root._margin
            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("磁图库")
                color: root._navy
                font.bold: true
                font.pointSize: ScreenTools.largeFontPointSize
            }
            QGCLabel {
                text: qsTr("%1 个文件").arg(fileModel.count)
                color: root._muted
                font.pointSize: ScreenTools.smallFontPointSize
            }
            QGCButton {
                text: qsTr("刷新")
                onClicked: root.reload()
            }
        }

        QGCComboBox {
            Layout.fillWidth: true
            visible: !root._wide && fileModel.count > 0
            model: fileModel
            textRole: "name"
            currentIndex: root.selectedIndex
            onActivated: (index) => root.selectedIndex = index
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: root._margin

            Rectangle {
                Layout.preferredWidth: root._wide ? Math.min(250, root.width * 0.3) : 0
                Layout.fillHeight: true
                visible: root._wide && fileModel.count > 0
                radius: root._radius
                color: "#FFFFFF"
                border.width: 1
                border.color: root._border

                ListView {
                    id: fileList
                    anchors.fill: parent
                    anchors.margins: root._margin
                    clip: true
                    spacing: 2
                    model: fileModel
                    delegate: Rectangle {
                        width: fileList.width
                        height: ScreenTools.defaultFontPixelHeight * 2.2
                        radius: 6
                        color: root.selectedIndex === index ? Qt.rgba(0.0, 0.65, 0.72, 0.12) : "transparent"
                        QGCLabel {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 8
                            text: model.name
                            color: root.selectedIndex === index ? root._navy : root._text
                            font.bold: root.selectedIndex === index
                            font.pointSize: ScreenTools.smallFontPointSize
                            elide: Text.ElideMiddle
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectedIndex = index
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: root._radius
                color: "#FFFFFF"
                border.width: 1
                border.color: root._border

                Image {
                    anchors.fill: parent
                    anchors.margins: root._margin
                    visible: root.selectedPath !== ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: false
                    source: root.selectedPath !== "" ? Qt.resolvedUrl(root.selectedPath) : ""
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    width: parent.width * 0.78
                    visible: fileModel.count === 0
                    spacing: root._margin
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 54
                        Layout.preferredHeight: 54
                        radius: 27
                        color: Qt.rgba(0.0, 0.65, 0.72, 0.10)
                        QGCLabel {
                            anchors.centerIn: parent
                            text: "∿"
                            color: root._cyan
                            font.pointSize: 24
                        }
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("还没有磁图")
                        color: root._navy
                        font.bold: true
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: qsTr("在“采集与磁图”中选择 CSV 并生成后，磁图会出现在这里。")
                        color: root._muted
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                }
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            visible: root.selectedPath !== ""
            text: root.selectedPath
            color: root._muted
            font.pointSize: ScreenTools.smallFontPointSize * 0.85
            elide: Text.ElideMiddle
        }
    }
}
