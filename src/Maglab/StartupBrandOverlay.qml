import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// Lightweight desktop startup branding. Android uses the native splash
/// resource with the same HYH + HEU artwork.
Item {
    id: root
    anchors.fill: parent
    visible: true
    opacity: 1.0
    z: QGroundControl.zOrderTopMost + 50

    Rectangle {
        anchors.fill: parent
        color: "#F7FAFC"
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.72, 720)
        spacing: 28

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(parent.width * 0.46, 360)
            Layout.preferredHeight: width * 0.807
            source: "qrc:/maglab/hyh.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.preferredHeight: width * 0.266
            source: "qrc:/maglab/heu_standard.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }

        QGCLabel {
            Layout.alignment: Qt.AlignHCenter
            text: "Maglab"
            color: "#102A43"
            font.family: "Times New Roman"
            font.bold: true
            font.pixelSize: 30
        }

        QGCLabel {
            Layout.alignment: Qt.AlignHCenter
            text: "HYH × HEU"
            color: "#627D98"
            font.family: "Times New Roman"
            font.pixelSize: 14
        }
    }

    Timer {
        interval: 3000
        running: true
        onTriggered: fadeOut.start()
    }

    NumberAnimation {
        id: fadeOut
        target: root
        property: "opacity"
        from: 1.0
        to: 0.0
        duration: 350
        onFinished: root.visible = false
    }
}
