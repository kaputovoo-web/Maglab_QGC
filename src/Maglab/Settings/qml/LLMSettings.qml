/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGC as Backend
import QGroundControl
import QGroundControl.FactControls
import QGroundControl.Controls

Rectangle {
    id:             _root
    color:          qgcPal.window
    anchors.fill:   parent
    anchors.margins: ScreenTools.defaultFontPixelWidth

    Backend.LLMSettings { id: settings }
    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    readonly property var _llmSettings: settings
    readonly property bool _narrow: width < ScreenTools.defaultFontPixelWidth * 65
    property real   _labelWidth:    _narrow ? 0 : ScreenTools.defaultFontPixelWidth * 18
    property real   _margins:       ScreenTools.defaultFontPixelWidth

    Flickable {
        anchors.fill:   parent
        clip:           true
        contentHeight:  _column.height + (_margins * 2)
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id:         _column
            spacing:    ScreenTools.defaultFontPixelHeight * 1.5
            width:      _root.width

            // ---- LLM API Section -------------------------------------------
            QGCLabel {
                text:           qsTr("LLM API Configuration")
                font.pointSize: ScreenTools.largeFontPointSize
            }
            Rectangle {
                Layout.fillWidth:   true
                implicitHeight:     _llmApiGrid.implicitHeight + (_root._margins * 2)
                color:              qgcPal.windowShade

                GridLayout {
                    id:             _llmApiGrid
                    width:          parent.width - _margins * 2
                    anchors.left:   parent.left
                    anchors.top:    parent.top
                    anchors.margins: _margins
                    columns:        _root._narrow ? 1 : 2
                    columnSpacing:  _margins
                    rowSpacing:     ScreenTools.defaultFontPixelHeight

                    QGCLabel {
                        text:               qsTr("API Endpoint:")
                        Layout.minimumWidth: _labelWidth
                    }
                    FactTextField {
                        Layout.fillWidth:   true
                        fact:               _llmSettings.apiEndpoint
                    }
                    QGCLabel {
                        text:               qsTr("Model Name:")
                        Layout.minimumWidth: _labelWidth
                    }
                    FactTextField {
                        Layout.fillWidth:   true
                        fact:               _llmSettings.modelName
                    }
                    QGCLabel {
                        text:               qsTr("API Key:")
                        Layout.minimumWidth: _labelWidth
                    }
                    FactTextField {
                        Layout.fillWidth:   true
                        fact:               _llmSettings.apiKey
                        echoMode:           TextInput.Password
                    }
                    QGCLabel {
                        text:               qsTr("Default Speed:")
                        Layout.minimumWidth: _labelWidth
                    }
                    FactTextField {
                        Layout.fillWidth:   true
                        fact:               _llmSettings.defaultFlightSpeed
                    }
                    QGCLabel {
                        text:               qsTr("Altitude Mode:")
                        Layout.minimumWidth: _labelWidth
                    }
                    FactComboBox {
                        Layout.fillWidth:   true
                        fact:               _llmSettings.defaultAltitudeMode
                    }
                }
            }

            // ---- STT Section -----------------------------------------------
            QGCLabel {
                text:           qsTr("Speech-to-Text")
                font.pointSize: ScreenTools.largeFontPointSize
            }
            Rectangle {
                Layout.fillWidth:   true
                implicitHeight:     _sttGrid.implicitHeight + (_root._margins * 2)
                color:              qgcPal.windowShade

                GridLayout {
                    id:             _sttGrid
                    width:          parent.width - _margins * 2
                    anchors.left:   parent.left
                    anchors.top:    parent.top
                    anchors.margins: _margins
                    columns:        _root._narrow ? 1 : 2
                    columnSpacing:  _margins
                    rowSpacing:     ScreenTools.defaultFontPixelHeight

                    QGCLabel {
                        text:               qsTr("STT Endpoint:")
                        Layout.minimumWidth: _labelWidth
                    }
                    FactTextField {
                        Layout.fillWidth:   true
                        fact:               _llmSettings.sttEndpoint
                    }
                    QGCLabel {
                        text:               qsTr("STT Model:")
                        Layout.minimumWidth: _labelWidth
                    }
                    FactTextField {
                        Layout.fillWidth:   true
                        fact:               _llmSettings.sttModel
                    }
                    QGCLabel {
                        text:               qsTr("STT API Key:")
                        Layout.minimumWidth: _labelWidth
                    }
                    FactTextField {
                        Layout.fillWidth:   true
                        fact:               _llmSettings.sttApiKey
                        echoMode:           TextInput.Password
                    }
                }
            }

            // ---- System Prompt Section -------------------------------------
            QGCLabel {
                text:           qsTr("System Prompt")
                font.pointSize: ScreenTools.largeFontPointSize
            }
            Rectangle {
                Layout.fillWidth:   true
                implicitHeight:     _promptGrid.implicitHeight + (_root._margins * 2)
                color:              qgcPal.windowShade

                GridLayout {
                    id:             _promptGrid
                    width:          parent.width - _margins * 2
                    anchors.left:   parent.left
                    anchors.top:    parent.top
                    anchors.margins: _margins
                    columns:        _root._narrow ? 1 : 2
                    columnSpacing:  _margins
                    rowSpacing:     ScreenTools.defaultFontPixelHeight

                    QGCLabel {
                        text:               qsTr("Prompt:")
                        Layout.minimumWidth: _labelWidth
                    }
                    FactTextField {
                        Layout.fillWidth:   true
                        Layout.minimumHeight: ScreenTools.defaultFontPixelHeight * 6
                        fact:               _llmSettings.systemPrompt
                    }
                }
            }

            // Spacer at bottom
            Item { Layout.fillHeight: true }
        }
    }
}
