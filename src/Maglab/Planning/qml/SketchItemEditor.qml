import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls

Rectangle {
    id: _root
    required property var missionItem
    required property real availableWidth
    height: visible ? mainColumn.implicitHeight + _margin * 2 : 0
    width: availableWidth
    color: "transparent"

    readonly property real _margin: ScreenTools.defaultFontPixelWidth * 0.6
    readonly property real _cardPad: ScreenTools.defaultFontPixelWidth * 0.75
    readonly property real _radius: ScreenTools.defaultFontPixelWidth * 0.6
    readonly property bool _narrow: width < 340
    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    component Card: Rectangle {
        id: card
        default property alias content: cardColumn.data
        Layout.fillWidth: true
        implicitHeight: cardColumn.implicitHeight + _cardPad * 2
        color: qgcPal.window
        radius: _root._radius
        border.width: 1
        border.color: qgcPal.windowShade
        ColumnLayout {
            id: cardColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: _root._cardPad
            spacing: _root._margin
        }
    }

    component SectionTitle: RowLayout {
        property string title
        Layout.fillWidth: true
        spacing: _root._margin
        Rectangle {
            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 0.4
            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.1
            radius: width / 2
            color: "#FF7043"
        }
        QGCLabel {
            Layout.fillWidth: true
            text: title
            font.bold: true
            elide: Text.ElideRight
        }
    }

    component ParamSlider: QGCSlider {
        id: slider
        property var fact
        property real step: 1
        from: fact ? fact.min : 0
        to: fact ? fact.max : 1
        stepSize: step
        Layout.preferredHeight: implicitHeight
        Binding {
            target: slider
            property: "value"
            value: slider.fact ? slider.fact.value : 0
            when: !slider.pressed
            restoreMode: Binding.RestoreBindingOrValue
        }
        onMoved: if (fact) fact.value = value
    }

    component ParamRow: ColumnLayout {
        id: paramRow
        property string label
        property var fact
        property real step: 1
        property string unit: fact ? fact.units : ""
        Layout.fillWidth: true
        spacing: 2
        RowLayout {
            Layout.fillWidth: true
            spacing: _root._margin
            QGCLabel {
                Layout.fillWidth: _root._narrow
                Layout.preferredWidth: _root._narrow ? 0 : Math.max(ScreenTools.defaultFontPixelWidth * 7.5, _root.width * 0.2)
                Layout.maximumWidth: _root._narrow ? _root.width : Number.POSITIVE_INFINITY
                text: paramRow.label
                elide: Text.ElideRight
                font.pointSize: ScreenTools.smallFontPointSize
            }
            FactTextField {
                Layout.preferredWidth: _root._narrow ? ScreenTools.defaultFontPixelWidth * 9
                                                    : ScreenTools.defaultFontPixelWidth * 7
                Layout.maximumWidth: Layout.preferredWidth
                fact: paramRow.fact
                showUnits: false
            }
            QGCLabel {
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 3.5
                Layout.maximumWidth: Layout.preferredWidth
                text: paramRow.unit
                elide: Text.ElideRight
                color: qgcPal.colorGrey
                font.pointSize: ScreenTools.smallFontPointSize
            }
            ParamSlider {
                Layout.fillWidth: true
                visible: !_root._narrow
                fact: paramRow.fact
                step: paramRow.step
            }
        }
        ParamSlider {
            Layout.fillWidth: true
            visible: _root._narrow
            fact: paramRow.fact
            step: paramRow.step
        }
    }

    component AltitudeModeRow: RowLayout {
        property var modeFact
        Layout.fillWidth: true
        spacing: _root._margin
        QGCLabel {
            Layout.preferredWidth: Math.max(ScreenTools.defaultFontPixelWidth * 8, _root.width * 0.22)
            text: qsTr("高度参考")
            font.pointSize: ScreenTools.smallFontPointSize
        }
        FactComboBox {
            Layout.fillWidth: true
            fact: modeFact
            indexModel: false
        }
    }

    ColumnLayout {
        id: mainColumn
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: _root._margin
        spacing: _root._margin

        RowLayout {
            Layout.fillWidth: true
            spacing: _root._margin
            Rectangle {
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 0.5
                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.4
                radius: width / 2
                color: "#FF7043"
            }
            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("Sketch 手绘")
                font.bold: true
                font.pointSize: ScreenTools.mediumFontPointSize
                elide: Text.ElideRight
            }
            QGCLabel {
                text: qsTr("%1 点").arg(missionItem.pointCount)
                color: qgcPal.colorGrey
                font.pointSize: ScreenTools.smallFontPointSize
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: _root._margin
            QGCButton {
                Layout.fillWidth: true
                text: qsTr("测绘区域")
                primary: missionItem.drawMode === 0
                onClicked: missionItem.drawMode = 0
            }
            QGCButton {
                Layout.fillWidth: true
                text: qsTr("航点")
                primary: missionItem.drawMode === 1
                onClicked: missionItem.drawMode = 1
            }
            QGCButton {
                Layout.fillWidth: true
                text: missionItem.drawArmed ? qsTr("结束绘制") : qsTr("开始绘制")
                primary: missionItem.drawArmed
                onClicked: missionItem.drawArmed = !missionItem.drawArmed
            }
        }

        QGCButton {
            Layout.fillWidth: true
            text: qsTr("清除重画")
            enabled: missionItem.pointCount > 0
            onClicked: missionItem.clearPoints()
        }

        Card {
            visible: missionItem.drawMode === 0
            SectionTitle { title: qsTr("测绘参数") }
            ParamRow { label: qsTr("网格间距"); fact: missionItem.gridSpacing; step: 1 }
            ParamRow { label: qsTr("飞行高度"); fact: missionItem.altitude; step: 1 }
            AltitudeModeRow { modeFact: missionItem.altitudeMode }
            RowLayout {
                Layout.fillWidth: true
                spacing: _root._margin
                QGCButton {
                    Layout.fillWidth: true
                    text: qsTr("整理为四边形")
                    enabled: missionItem.pointCount >= 3
                    onClicked: missionItem.makeQuad()
                }
                QGCButton {
                    Layout.fillWidth: true
                    text: qsTr("生成测区")
                    primary: true
                    enabled: missionItem.pointCount >= 3
                    onClicked: missionItem.generateSurvey()
                }
            }
            QGCLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("生成后转换为原生 Survey，可继续调整测线、间距、角度和重叠率。")
                font.pointSize: ScreenTools.smallFontPointSize * 0.85
                color: qgcPal.colorGrey
            }
        }

        Card {
            visible: missionItem.drawMode === 1
            SectionTitle { title: qsTr("航点参数") }
            RowLayout {
                Layout.fillWidth: true
                spacing: _root._margin
                QGCLabel {
                    Layout.preferredWidth: Math.max(ScreenTools.defaultFontPixelWidth * 8, _root.width * 0.22)
                    text: qsTr("航线类型")
                    font.pointSize: ScreenTools.smallFontPointSize
                }
                QGCComboBox {
                    Layout.fillWidth: true
                    model: [ qsTr("直线"), qsTr("曲线") ]
                    currentIndex: missionItem.routeStyle
                    onActivated: (index) => missionItem.routeStyle = index
                }
            }
            ParamRow { label: qsTr("飞行速度"); fact: missionItem.flightSpeed; step: 0.5 }
            ParamRow { label: qsTr("飞行高度"); fact: missionItem.altitude; step: 1 }
            AltitudeModeRow { modeFact: missionItem.altitudeMode }
        }

        Card {
            SectionTitle { title: qsTr("航点编辑") }
            QGCLabel {
                Layout.fillWidth: true
                visible: missionItem.selectedIndex < 0
                text: missionItem.pointCount > 0
                      ? qsTr("点击地图上的航点圆点可单独编辑该点。")
                      : qsTr("在地图上按住拖动绘制航线，松手后自动简化为航点。")
                wrapMode: Text.WordWrap
                font.pointSize: ScreenTools.smallFontPointSize * 0.85
                color: qgcPal.colorGrey
            }
            RowLayout {
                Layout.fillWidth: true
                visible: missionItem.selectedIndex >= 0
                spacing: _root._margin
                QGCLabel {
                    text: qsTr("航点 #%1").arg(missionItem.selectedIndex + 1)
                    font.bold: true
                }
                QGCComboBox {
                    Layout.fillWidth: true
                    model: [ qsTr("跟随整体"), qsTr("直线"), qsTr("曲线") ]
                    currentIndex: missionItem.selectedIndex >= 0 ? missionItem.pointStyle(missionItem.selectedIndex) + 1 : 0
                    onActivated: (index) => missionItem.setPointStyle(missionItem.selectedIndex, index - 1)
                }
                QGCButton {
                    text: qsTr("删除")
                    onClicked: missionItem.removePoint(missionItem.selectedIndex)
                }
            }
        }

        Card {
            SectionTitle { title: qsTr("航线摘要") }
            Flow {
                Layout.fillWidth: true
                spacing: _root._margin * 1.5
                QGCLabel {
                    text: qsTr("距离 %1 %2")
                        .arg(QGroundControl.unitsConversion.metersToAppSettingsHorizontalDistanceUnits(missionItem.complexDistance).toFixed(0))
                        .arg(QGroundControl.unitsConversion.appSettingsHorizontalDistanceUnitsString)
                    font.pointSize: ScreenTools.smallFontPointSize
                }
                QGCLabel {
                    text: qsTr("航点 %1").arg(missionItem.pointCount)
                    font.pointSize: ScreenTools.smallFontPointSize
                }
            }
        }
    }
}
