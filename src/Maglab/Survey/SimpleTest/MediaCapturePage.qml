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
    readonly property color _error: "#D64545"
    readonly property real _margin: ScreenTools.defaultFontPixelWidth * 0.6
    readonly property real _cardPad: ScreenTools.defaultFontPixelWidth * 0.9
    readonly property real _radius: ScreenTools.defaultFontPixelWidth * 0.7
    readonly property bool _narrow: width < 520
    readonly property var videoManager: QGroundControl.videoManager

    property string statusText: qsTr("待机")
    property bool statusOk: true
    property string lastFile: ""

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    function newName(prefix, suffix) {
        return prefix + Qt.formatDateTime(new Date(), "yyyyMMdd_hhmmss_zzz") + "_" +
               Math.random().toString(36).substring(2, 10) + suffix
    }

    Connections {
        target: root.videoManager
        function onScreenshotComplete(success, filename) {
            root.statusOk = success
            root.lastFile = success ? filename : ""
            root.statusText = success ? qsTr("截图已保存") : qsTr("截图失败")
        }
        function onRecordingChanged() {
            root.statusOk = true
            root.statusText = root.videoManager.recording ? qsTr("正在录像") : qsTr("录像已停止")
        }
    }

    component Card: Rectangle {
        id: card
        default property alias content: cardColumn.data
        Layout.fillWidth: true
        implicitHeight: cardColumn.implicitHeight + root._cardPad * 2
        color: "#FFFFFF"
        radius: root._radius
        border.width: 1
        border.color: root._border
        ColumnLayout {
            id: cardColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: root._cardPad
            spacing: root._margin
        }
    }

    component SectionTitle: RowLayout {
        property string title
        property string hint: ""
        Layout.fillWidth: true
        spacing: root._margin
        Rectangle {
            Layout.preferredWidth: 3
            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.1
            radius: 2
            color: root._cyan
        }
        QGCLabel {
            text: title
            color: root._navy
            font.bold: true
            font.pointSize: ScreenTools.mediumFontPointSize
        }
        Item { Layout.fillWidth: true }
        QGCLabel {
            visible: hint !== ""
            text: hint
            color: root._muted
            font.pointSize: ScreenTools.smallFontPointSize
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: mainColumn.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: mainColumn
            width: parent.width
            spacing: root._margin

            RowLayout {
                Layout.fillWidth: true
                spacing: root._margin
                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("相机采集")
                    color: root._navy
                    font.bold: true
                    font.pointSize: ScreenTools.largeFontPointSize
                }
                Rectangle {
                    implicitWidth: videoLabel.implicitWidth + 16
                    implicitHeight: videoLabel.implicitHeight + 8
                    radius: height / 2
                    color: root.videoManager.hasVideo ? Qt.rgba(0.18, 0.62, 0.48, 0.14) : Qt.rgba(0.85, 0.45, 0.10, 0.14)
                    QGCLabel {
                        id: videoLabel
                        anchors.centerIn: parent
                        text: root.videoManager.hasVideo ? qsTr("视频已配置") : qsTr("未配置视频")
                        color: root.videoManager.hasVideo ? "#2F9E7A" : "#D97706"
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                }
            }

            Card {
                SectionTitle {
                    title: qsTr("视频源")
                    hint: root.videoManager.hasVideo ? qsTr("就绪") : qsTr("不可用")
                }
                QGCLabel {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("使用 QGroundControl 已配置的视频流进行截图和录像，不再使用旧版独立 RTSP 线程。")
                    color: root._muted
                    font.pointSize: ScreenTools.smallFontPointSize
                }
                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("视频保存目录：%1").arg(QGroundControl.settingsManager.appSettings.videoSavePath)
                    color: root._muted
                    font.pointSize: ScreenTools.smallFontPointSize * 0.85
                    elide: Text.ElideMiddle
                }
                QGCLabel {
                    Layout.fillWidth: true
                    visible: !root.videoManager.hasVideo
                    wrapMode: Text.WordWrap
                    text: qsTr("请先在“应用设置 > 视频”中配置相机 RTSP 地址。")
                    color: "#D97706"
                    font.pointSize: ScreenTools.smallFontPointSize
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root._narrow ? 1 : 2
                columnSpacing: root._margin
                rowSpacing: root._margin

                Card {
                    SectionTitle {
                        title: qsTr("截图")
                        hint: qsTr("保存到磁图目录")
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("从当前视频帧生成 JPG，后端写盘完成后返回结果。")
                        color: root._muted
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    QGCButton {
                        Layout.fillWidth: true
                        text: qsTr("拍摄截图")
                        primary: true
                        enabled: root.videoManager.hasVideo && MagLabPaths.storageReady
                        onClicked: {
                            var path = MagLabPaths.magMapDir() + "/" + root.newName("vis", ".jpg")
                            root.statusOk = true
                            root.statusText = qsTr("正在截图…")
                            root.videoManager.grabImage(path)
                        }
                    }
                }

                Card {
                    SectionTitle {
                        title: qsTr("录像")
                        hint: root.videoManager.recording ? qsTr("录制中") : qsTr("待机")
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("录像保存到应用设置中的视频目录，停止后由后端完成文件收尾。")
                        color: root._muted
                        font.pointSize: ScreenTools.smallFontPointSize
                    }
                    QGCButton {
                        Layout.fillWidth: true
                        text: root.videoManager.recording ? qsTr("停止录像") : qsTr("开始录像")
                        primary: !root.videoManager.recording
                        enabled: root.videoManager.hasVideo
                        onClicked: {
                            if (root.videoManager.recording) {
                                mainWindow.showMessageDialog(qsTr("停止录像"),
                                                             qsTr("停止后当前视频文件将完成收尾，确认停止？"),
                                                             Dialog.Yes | Dialog.Cancel,
                                                             function() { root.videoManager.stopRecording() })
                            } else {
                                root.statusText = qsTr("正在开始录像…")
                                root.videoManager.startRecording(root.newName("rec", ""))
                            }
                        }
                    }
                }
            }

            Card {
                SectionTitle { title: qsTr("状态") }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: root._margin
                    Rectangle {
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: root.statusOk ? "#2F9E7A" : root._error
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        text: root.statusText
                        color: root.statusOk ? root._text : root._error
                        font.bold: true
                        elide: Text.ElideRight
                    }
                }
                QGCLabel {
                    Layout.fillWidth: true
                    visible: root.lastFile !== ""
                    text: root.lastFile
                    color: root._muted
                    font.pointSize: ScreenTools.smallFontPointSize * 0.85
                    elide: Text.ElideMiddle
                }
            }
        }
    }
}
