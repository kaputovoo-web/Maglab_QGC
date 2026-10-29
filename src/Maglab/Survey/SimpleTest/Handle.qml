import QtQuick          2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts      1.2
import Qt.labs.folderlistmodel 2.15

import QGroundControl               1.0
import QGroundControl.Palette       1.0
import QGroundControl.FactSystem    1.0
import QGroundControl.FactControls  1.0
import QGroundControl.Controls      1.0
import QGroundControl.ScreenTools   1.0
import MagLab 1.0
import Handle 1.0
import RTSPCapture 1.0

AnalyzePage {
    id:              mavlinkConsolePage

    property int  currentChart: 0   // 0 = 无, 2 = 显示图片
    property var  path1:        ""

    Handle {
        id: _Handle
    }

    RTSPCapture {
        id: _RTSPCapture
    }

    // 注意：AnalyzePage 把 pageComponent 暴露为 pageLoader.sourceComponent，
    // 因此页面内容必须放在这个 Component 里；直接写 pageComponent: pageComponent
    // 是自引用，Loader 拿不到任何东西，内容会挂到 AnalyzePage 外层并压住标题区。
    Component {
        id: pageComponent

        Connections {
            target: _Handle
            // 信号名必须与 C++ 的 Handle::genmagmapFinished 完全一致：
            // 写成 on_genmagmapFinished 时这个处理器永远不会被调用。
            function onGenmagmapFinished() {
                path1 = _Handle.magmapFilePath
                // 生成完成后直接切到图片视图，否则新生成的磁图不会被显示。
                currentChart = 2
            }
        }

        FolderListModel {
            id: folderModel
            folder: MagLabPaths.surveyDir()
            nameFilters: ["*.txt"]
            showDirs: false
            showDotAndDotDot: false
            sortField: FolderListModel.Name
        }

        ComboBox {
            id: txtCombo
            anchors.left: parent.left
            anchors.top: parent.top
            width: 400
            height: 50
            anchors.margins: 10
            textRole: "fileName"

            model: ListModel {
                id: fileListModel
            }

            // 当文件夹内容加载完成后，手动填充 ListModel
            Connections {
                target: folderModel
                function onCountChanged() {
                    fileListModel.clear()
                    for (var i = 0; i < folderModel.count; i++) {
                        fileListModel.append({
                                                 fileName: folderModel.get(i, "fileName"),
                                                 filePath: folderModel.get(i, "filePath")
                                             })
                    }
                }
            }
        }

        Button {
            id: button1
            anchors.left: parent.left
            anchors.top: txtCombo.bottom
            anchors.margins: 10
            width: 120
            height: 50
            text: qsTr("生成磁图")
            font.pixelSize: 20
            // 空目录时 currentIndex 为 -1，fileListModel.get(-1) 是 undefined，
            // 再取 .fileName 会抛 TypeError。
            // 生成过程在工作线程执行，期间禁用按钮避免重复触发。
            enabled: txtCombo.currentIndex >= 0 && !_Handle.busy

            onClicked: {
                var selectedItem = fileListModel.get(txtCombo.currentIndex)
                if (!selectedItem) {
                    return
                }
                _Handle.magmapPath = selectedItem.filePath
            }
        }

        // 生成进度反馈：三角剖分与栅格化都在工作线程，UI 只显示状态
        BusyIndicator {
            id: genBusy
            anchors.left: button1.right
            anchors.verticalCenter: button1.verticalCenter
            anchors.leftMargin: 12
            width: 40
            height: 40
            running: _Handle.busy
            visible: _Handle.busy
        }

        QGCLabel {
            anchors.left: genBusy.visible ? genBusy.right : button1.right
            anchors.verticalCenter: button1.verticalCenter
            anchors.leftMargin: 10
            anchors.right: parent.right
            elide: Text.ElideRight
            color: _Handle.errorString !== "" ? "red" : "white"
            text: _Handle.errorString !== "" ? _Handle.errorString : _Handle.progressText
        }

        Button {
            id: button2
            anchors.left: parent.left
            anchors.top: button1.bottom
            anchors.margins: 10
            width: 120
            height: 50
            text: qsTr("拍照")
            font.pixelSize: 20

            onClicked: {
                _RTSPCapture.rtsp_capture("rtsp://192.168.144.108:554/stream=0",
                                          MagLabPaths.magMapDir())
            }
        }

        Button {
            id: button3
            anchors.left: parent.left
            anchors.top: button2.bottom
            anchors.margins: 10
            width: 120
            height: 50
            text: qsTr("录像")
            font.pixelSize: 20

            onClicked: {
                _RTSPCapture.startRecord("rtsp://192.168.144.108:554/stream=0",
                                         MagLabPaths.videoDir() + "/test.mp4")
            }
        }

        Button {
            id: button4
            anchors.left: button3.right
            anchors.top: button3.top
            anchors.margins: 10
            width: 120
            height: 50
            text: qsTr("停止")
            font.pixelSize: 20

            onClicked: {
                _RTSPCapture.stopRecord()
            }
        }

        ComboBox {
            id: txtCombo2
            anchors.left: parent.left
            anchors.top: button3.bottom
            width: 400
            height: 50
            anchors.margins: 10
            textRole: "fileName"

            model: ListModel {
                id: fileListModel2
            }

            Connections {
                target: folderModel2
                function onCountChanged() {
                    fileListModel2.clear()
                    for (var i = 0; i < folderModel2.count; i++) {
                        fileListModel2.append({
                                                  fileName: folderModel2.get(i, "fileName"),
                                                  filePath: folderModel2.get(i, "filePath")
                                              })
                    }
                }
            }
        }

        Button {
            id: button5
            anchors.left: parent.left
            anchors.top: txtCombo2.bottom
            anchors.margins: 10
            width: 120
            height: 50
            text: qsTr("选择视图")
            font.pixelSize: 20
            enabled: txtCombo2.currentIndex >= 0

            onClicked: {
                var selectedItem = fileListModel2.get(txtCombo2.currentIndex)
                if (!selectedItem) {
                    return
                }
                path1 = selectedItem.filePath
                currentChart = 2
            }
        }

        FolderListModel {
            id: folderModel2
            folder: MagLabPaths.magMapDir()
            nameFilters: ["*.jpg"]
            showDirs: false
            showDotAndDotDot: false
            sortField: FolderListModel.Name
        }

        Image {
            anchors.top: parent.top
            anchors.right: parent.right
            width: parent.width / 2
            height: parent.width / 2
            visible: currentChart === 2
            fillMode: Image.PreserveAspectFit
            // resolvedUrl 才能正确处理含空格/中文/# 的路径
            source: path1 ? Qt.resolvedUrl(path1) : ""
        }
    }
}