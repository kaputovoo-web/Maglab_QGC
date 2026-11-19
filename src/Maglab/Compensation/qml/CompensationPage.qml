import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGC
import QGroundControl
import QGroundControl.Controls

Item {
    id: root
    property real scrollOffset: 0
    readonly property color _ink: "#102A43"
    readonly property color _muted: "#627D98"
    readonly property color _line: "#D9E2EC"
    readonly property color _soft: "#F5F8FB"
    readonly property color _teal: "#008C9E"
    readonly property real _gap: ScreenTools.defaultFontPixelWidth * 0.9
    readonly property real _pad: ScreenTools.defaultFontPixelWidth * 1.1
    readonly property real _radius: ScreenTools.defaultFontPixelWidth * 0.9

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    component Card: Rectangle {
        id: card
        default property alias content: body.data
        Layout.fillWidth: true
        implicitHeight: body.implicitHeight + root._pad * 2
        color: "#FFFFFF"
        radius: root._radius
        border.width: 1
        border.color: root._line
        ColumnLayout {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: root._pad
            spacing: root._gap
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: content.implicitHeight + root._pad * 2
        clip: true
        onContentYChanged: root.scrollOffset = contentY

        ColumnLayout {
            id: content
            x: root._pad
            y: root._pad
            width: parent.width - root._pad * 2
            spacing: root._gap

            RowLayout {
                Layout.fillWidth: true
                spacing: root._gap
                Rectangle {
                    Layout.preferredWidth: 4
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 2.1
                    radius: 2
                    color: root._teal
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: root._gap * 0.15
                    QGCLabel {
                        text: qsTr("磁干扰补偿")
                        color: root._ink
                        font.bold: true
                        font.pointSize: ScreenTools.largeFontPointSize
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        text: qsTr("Compensation · 模块预留，不影响实时采集与磁图显示")
                        color: root._muted
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                }
            }

            Card {
                RowLayout {
                    Layout.fillWidth: true
                    spacing: root._gap
                    Rectangle {
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 44
                        radius: 22
                        color: "#E5F7F8"
                        QGCLabel {
                            anchors.centerIn: parent
                            text: "◎"
                            color: root._teal
                            font.pixelSize: 24
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: root._gap * 0.2
                        QGCLabel {
                            text: qsTr("补偿算法工作区")
                            color: root._ink
                            font.bold: true
                            font.pointSize: ScreenTools.mediumFontPointSize
                        }
                        QGCLabel {
                            Layout.fillWidth: true
                            text: qsTr("后续将在此接入 Tolles-Lawson 物理补偿、在线矢量补偿和物理引导多源融合补偿。当前阶段仅保留模块入口，不执行任何自动写入或飞行参数修改。")
                            color: root._muted
                            wrapMode: Text.WordWrap
                            font.pointSize: ScreenTools.smallFontPointSize
                        }
                    }
                }
            }

            Card {
                QGCLabel {
                    text: qsTr("规划中的能力")
                    color: root._ink
                    font.bold: true
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
                Repeater {
                    model: [
                        { title: qsTr("Tolles-Lawson 物理补偿"), detail: qsTr("基于飞行姿态和磁通门分量的经典补偿模型") },
                        { title: qsTr("在线矢量补偿"), detail: qsTr("实时估计干扰项，不改变原始 CSV") },
                        { title: qsTr("物理引导多源融合"), detail: qsTr("融合标量、矢量和姿态数据，保留可追溯性") }
                    ]
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: root._gap * 0.6
                        Rectangle {
                            Layout.preferredWidth: 8
                            Layout.preferredHeight: 8
                            radius: 4
                            color: root._teal
                            opacity: 0.55
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            QGCLabel { text: modelData.title; color: root._ink; font.bold: true; font.pointSize: ScreenTools.smallFontPointSize }
                            QGCLabel { Layout.fillWidth: true; text: modelData.detail; color: root._muted; wrapMode: Text.WordWrap; font.pointSize: ScreenTools.smallFontPointSize * 0.9 }
                        }
                        QGCLabel { text: qsTr("预留"); color: root._teal; font.pointSize: ScreenTools.smallFontPointSize * 0.85 }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: note.implicitHeight + root._pad * 1.2
                radius: root._radius
                color: root._soft
                border.width: 1
                border.color: root._line
                QGCLabel {
                    id: note
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: root._pad
                    text: qsTr("安全边界：补偿模块不会自动修改飞控参数、任务或磁测原始数据。算法启用前必须单独评审和验证。")
                    color: root._muted
                    wrapMode: Text.WordWrap
                    font.pointSize: ScreenTools.smallFontPointSize
                }
            }
            Item { Layout.fillHeight: true }
        }
    }
}
