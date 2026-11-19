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
    readonly property int _modeColumns: width >= 520 ? 5 : 3
    property int _demoIndex: 0
    property real _spiralFirstArcRadius: missionItem.spiralFirstArcRadius
    property real _spiralSuggestedMaxSpeed: missionItem.spiralSafeEntrySpeed
    property real _spiralRequestedBank: missionItem.spiralRequestedEntryBankAngle
    property bool _spiralSpeedWarning: missionItem.spiralEntrySpeedLimited
    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    // ---------------- shared building blocks ----------------
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
            color: "#29B6F6"
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

    component EnumSelector: RowLayout {
        id: enumRow
        property var fact
        Layout.fillWidth: true
        spacing: _root._margin
        FactComboBox {
            Layout.fillWidth: true
            visible: _root._narrow
            fact: enumRow.fact
            indexModel: false
        }
        Repeater {
            model: _root._narrow ? [] : (fact ? fact.enumStrings : [])
            QGCButton {
                Layout.fillWidth: true
                text: modelData
                primary: fact && fact.enumIndex === index
                onClicked: if (fact) fact.enumIndex = index
            }
        }
    }

    component StatsRow: Flow {
        Layout.fillWidth: true
        spacing: _root._margin * 1.5
        QGCLabel {
            text: qsTr("距离 %1 %2")
                .arg(QGroundControl.unitsConversion.metersToAppSettingsHorizontalDistanceUnits(missionItem.complexDistance).toFixed(0))
                .arg(QGroundControl.unitsConversion.appSettingsHorizontalDistanceUnitsString)
            font.pointSize: ScreenTools.smallFontPointSize
        }
        QGCLabel {
            text: qsTr("航点 %1").arg(missionItem.waypointCount)
            font.pointSize: ScreenTools.smallFontPointSize
        }
        QGCLabel {
            text: {
                var seconds = missionItem.estimatedFlightTime
                if (seconds <= 0) return qsTr("时间 --")
                return qsTr("时间 %1:%2").arg(Math.floor(seconds / 60))
                    .arg(Math.round(seconds % 60) < 10 ? "0" : "")
                    + Math.round(seconds % 60)
            }
            font.pointSize: ScreenTools.smallFontPointSize
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
                color: "#29B6F6"
            }
            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("LLM 航线规划")
                font.bold: true
                font.pointSize: ScreenTools.mediumFontPointSize
                elide: Text.ElideRight
            }
            QGCLabel {
                text: missionItem.readyForSaveState === 0 ? qsTr("就绪") : qsTr("待完善")
                color: missionItem.readyForSaveState === 0 ? "#36ad7d" : qgcPal.colorOrange
                font.pointSize: ScreenTools.smallFontPointSize
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: _root._modeColumns
            columnSpacing: _root._margin * 0.5
            rowSpacing: _root._margin * 0.5
            Repeater {
                model: [qsTr("语音"), qsTr("螺旋"), qsTr("FOM"), qsTr("自定义"), qsTr("方形")]
                QGCButton {
                    Layout.fillWidth: true
                    text: modelData
                    primary: missionItem.planMode === index
                    onClicked: missionItem.planMode = index
                }
            }
        }

        Card {
            visible: missionItem.planMode === 0
            SectionTitle { title: qsTr("语音识别") }
            RowLayout {
                Layout.fillWidth: true
                spacing: _root._margin
                Rectangle {
                    Layout.preferredWidth: ScreenTools.defaultFontPixelHeight * 3.6
                    Layout.preferredHeight: width
                    radius: width / 2
                    color: micMouse.recording ? "#E53935" : "#1976D2"
                    QGCLabel {
                        anchors.centerIn: parent
                        text: micMouse.recording ? qsTr("松开\n结束") : qsTr("按住\n说话")
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        font.bold: true
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    MouseArea {
                        id: micMouse
                        anchors.fill: parent
                        property bool recording: false
                        onPressed: { recording = true; missionItem.startVoiceRecording() }
                        onReleased: if (recording) { recording = false; missionItem.stopVoiceRecording() }
                    }
                }
                QGCTextField {
                    id: promptInput
                    Layout.fillWidth: true
                    placeholderText: qsTr("例如：螺旋扫描 高度80米 间距25米 顺时针8圈")
                    property bool fromVoice: false
                    textColor: fromVoice ? "#1976D2" : qgcPal.textFieldText
                }
            }
            Connections {
                target: missionItem
                function onVoiceTranscriptReady(text) {
                    promptInput.fromVoice = true
                    promptInput.text = text
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: _root._margin
                QGCButton {
                    text: qsTr("示例")
                    onClicked: {
                        var demos = [
                            qsTr("螺旋扫描，高度80米，间距25米，顺时针8圈"),
                            qsTr("FOM标定，半径30米，3圈，50米高度"),
                            qsTr("测绘 200x300 间距25 高度80"),
                            qsTr("Draw a square 200m per side, 50m altitude")
                        ]
                        promptInput.fromVoice = false
                        promptInput.text = demos[_root._demoIndex % demos.length]
                        _root._demoIndex++
                    }
                }
                QGCButton {
                    Layout.fillWidth: true
                    text: qsTr("解析任务")
                    primary: true
                    onClicked: missionItem.sendToLLM(promptInput.text)
                }
            }
            QGCLabel {
                Layout.fillWidth: true
                visible: (missionItem.voiceStatusText || "") !== ""
                text: missionItem.voiceStatusText || ""
                wrapMode: Text.WordWrap
                color: "#29B6F6"
                font.pointSize: ScreenTools.smallFontPointSize
            }
            QGCLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("语音与 LLM 端点在“应用设置 > Maglab AI Settings”配置。")
                font.pointSize: ScreenTools.smallFontPointSize * 0.85
                color: qgcPal.colorGrey
            }
        }

        // ---------------- Spiral ----------------
        Card {
            visible: missionItem.planMode === 1
            SectionTitle { title: qsTr("航线参数") }
            ParamRow { label: qsTr("首圈半径"); fact: missionItem.strategyFactForMode(1, "startRadius"); step: 1 }
            ParamRow { label: qsTr("航线间距"); fact: missionItem.strategyFactForMode(1, "spacing"); step: 1 }
            ParamRow { label: qsTr("圈数"); fact: missionItem.strategyFactForMode(1, "turns"); step: 0.5 }
            ParamRow { label: qsTr("旋转角度"); fact: missionItem.strategyFactForMode(1, "rotationAngle"); step: 1 }
            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("方向")
                font.pointSize: ScreenTools.smallFontPointSize
            }
            EnumSelector { fact: missionItem.strategyFactForMode(1, "direction") }
        }

        Card {
            visible: missionItem.planMode === 1
            SectionTitle { title: qsTr("飞行设置") }
            ParamRow { label: qsTr("飞行速度"); fact: missionItem.strategyFactForMode(1, "flightSpeed"); step: 0.5 }
            ParamRow { label: qsTr("最大坡度"); fact: missionItem.strategyFactForMode(1, "maxBankAngle"); step: 1 }
            ParamRow { label: qsTr("飞行高度"); fact: missionItem.strategyFactForMode(1, "altitude"); step: 1 }
            AltitudeModeRow { modeFact: missionItem.strategyFactForMode(1, "altitudeMode") }
            QGCLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: _spiralSpeedWarning ? "#FF9800" : qgcPal.colorGrey
                font.pointSize: ScreenTools.smallFontPointSize * 0.9
                text: _spiralSpeedWarning
                      ? qsTr("首圈限速 %1 m/s（半径 %2 m，请求坡度 %3°）。沿更大圆周后才允许提高速度。")
                          .arg(_spiralSuggestedMaxSpeed.toFixed(1))
                          .arg(_spiralFirstArcRadius.toFixed(1))
                          .arg(_spiralRequestedBank.toFixed(1))
                      : qsTr("首圈半径 %1 m，坡度 %2°，已预留 20% 横向加速度余量。")
                          .arg(_spiralFirstArcRadius.toFixed(1))
                          .arg(_spiralRequestedBank.toFixed(1))
            }
        }

        // ---------------- FOM ----------------
        Card {
            visible: missionItem.planMode === 2
            SectionTitle { title: qsTr("航线参数") }
            ParamRow { label: qsTr("边长"); fact: missionItem.strategyFactForMode(2, "sideLength"); step: 1 }
            ParamRow { label: qsTr("每边圈数"); fact: missionItem.strategyFactForMode(2, "cycles"); step: 1 }
        }
        Card {
            visible: missionItem.planMode === 2
            SectionTitle { title: qsTr("飞行设置") }
            ParamRow { label: qsTr("飞行速度"); fact: missionItem.strategyFactForMode(2, "flightSpeed"); step: 0.5 }
            ParamRow { label: qsTr("飞行高度"); fact: missionItem.strategyFactForMode(2, "altitude"); step: 1 }
            AltitudeModeRow { modeFact: missionItem.strategyFactForMode(2, "altitudeMode") }
        }
        Card {
            visible: missionItem.planMode === 2
            SectionTitle { title: qsTr("姿态激励") }
            ParamRow { label: qsTr("Roll 幅度"); fact: missionItem.strategyFactForMode(2, "rollAmp"); step: 1 }
            ParamRow { label: qsTr("Roll 上限"); fact: missionItem.strategyFactForMode(2, "rollLimit"); step: 1 }
            ParamRow { label: qsTr("Pitch 幅度"); fact: missionItem.strategyFactForMode(2, "pitchAmp"); step: 1 }
            ParamRow { label: qsTr("Pitch 上限"); fact: missionItem.strategyFactForMode(2, "pitchLimit"); step: 1 }
            ParamRow { label: qsTr("Yaw 幅度"); fact: missionItem.strategyFactForMode(2, "yawAmp"); step: 1 }
            ParamRow { label: qsTr("Yaw 上限"); fact: missionItem.strategyFactForMode(2, "yawLimit"); step: 1 }
            QGCLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("幅度由对应上限约束，不会修改飞控全局参数。")
                font.pointSize: ScreenTools.smallFontPointSize * 0.85
                color: qgcPal.colorGrey
            }
        }

        // ---------------- Custom ----------------
        Card {
            visible: missionItem.planMode === 3
            SectionTitle { title: qsTr("飞行设置") }
            QGCLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: missionItem.waypointCount > 0
                      ? qsTr("已接收 %1 个航点，可继续调整高度和速度。").arg(missionItem.waypointCount)
                      : qsTr("在“语音”模式解析任务，或从任务文件加载轨迹。")
                font.pointSize: ScreenTools.smallFontPointSize
                color: missionItem.waypointCount > 0 ? qgcPal.text : qgcPal.colorOrange
            }
            ParamRow { label: qsTr("飞行速度"); fact: missionItem.strategyFactForMode(3, "flightSpeed"); step: 0.5 }
            ParamRow { label: qsTr("飞行高度"); fact: missionItem.strategyFactForMode(3, "altitude"); step: 1 }
            AltitudeModeRow { modeFact: missionItem.strategyFactForMode(3, "altitudeMode") }
        }

        // ---------------- Expanding square ----------------
        Card {
            visible: missionItem.planMode === 4
            SectionTitle { title: qsTr("航线参数") }
            ParamRow { label: qsTr("航线间距"); fact: missionItem.strategyFactForMode(4, "spacing"); step: 1 }
            ParamRow { label: qsTr("边数"); fact: missionItem.strategyFactForMode(4, "legs"); step: 1 }
            ParamRow { label: qsTr("旋转角度"); fact: missionItem.strategyFactForMode(4, "rotationAngle"); step: 1 }
            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("方向")
                font.pointSize: ScreenTools.smallFontPointSize
            }
            EnumSelector { fact: missionItem.strategyFactForMode(4, "direction") }
        }
        Card {
            visible: missionItem.planMode === 4
            SectionTitle { title: qsTr("飞行设置") }
            ParamRow { label: qsTr("飞行速度"); fact: missionItem.strategyFactForMode(4, "flightSpeed"); step: 0.5 }
            ParamRow { label: qsTr("飞行高度"); fact: missionItem.strategyFactForMode(4, "altitude"); step: 1 }
            AltitudeModeRow { modeFact: missionItem.strategyFactForMode(4, "altitudeMode") }
        }

        Card {
            SectionTitle { title: qsTr("航线摘要") }
            StatsRow {}
        }

        Card {
            RowLayout {
                Layout.fillWidth: true
                spacing: _root._margin
                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("AI 安全复核")
                    font.bold: true
                }
                Switch {
                    id: aiReviewSwitch
                    checked: false
                }
            }
            QGCLabel {
                Layout.fillWidth: true
                visible: aiReviewSwitch.checked
                wrapMode: Text.WordWrap
                text: qsTr("AI 复核只提供风险提示，不授予飞行许可，也不替代现场检查。")
                font.pointSize: ScreenTools.smallFontPointSize * 0.85
                color: qgcPal.colorGrey
            }
            QGCButton {
                Layout.fillWidth: true
                visible: aiReviewSwitch.checked
                text: missionItem.safetyReviewBusy ? qsTr("复核中…") : qsTr("开始复核")
                enabled: !missionItem.safetyReviewBusy && missionItem.readyForSaveState === 0
                onClicked: missionItem.reviewSafety()
            }
            QGCLabel {
                Layout.fillWidth: true
                visible: aiReviewSwitch.checked && (missionItem.safetyReviewText || "") !== ""
                text: missionItem.safetyReviewText || ""
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
                font.pointSize: ScreenTools.smallFontPointSize
                color: "#FFB300"
            }
        }
    }
}
