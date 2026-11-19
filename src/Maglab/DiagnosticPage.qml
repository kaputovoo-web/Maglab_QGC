import QtQuick
import QtQuick.Layouts
import QGC
import QGroundControl
import QGroundControl.Controls

Rectangle {
    color: palette.window
    QGCPalette { id: palette; colorGroupEnabled: true }
    AttitudeAnglesController { id: attitude }
    SingletonTest { id: diagnostic }

    Flickable {
        anchors.fill: parent
        anchors.margins: ScreenTools.defaultFontPixelWidth
        contentHeight: column.implicitHeight
        clip: true
        ColumnLayout {
            id: column
            width: parent.width
            spacing: ScreenTools.defaultFontPixelHeight
            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("Maglab Read-only Diagnostics")
                font.pointSize: ScreenTools.largeFontPointSize
            }
            QGCLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Displays received data for the active vehicle only. This page never injects GPS data or writes vehicle parameters.")
            }
            QGCLabel { text: attitude.activeVehicleAvailable ? qsTr("Active vehicle available") : qsTr("No active vehicle") }
            QGCLabel { text: qsTr("Roll: %1 · Pitch: %2 · Yaw: %3").arg(attitude.rollText).arg(attitude.pitchText).arg(attitude.yawText); Layout.fillWidth: true; wrapMode: Text.WordWrap }
            QGCLabel { text: qsTr("GPS latitude: %1").arg(diagnostic.test1 || "—") }
            QGCLabel { text: qsTr("GPS longitude: %1").arg(diagnostic.test2 || "—") }
        }
    }
}
