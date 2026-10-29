import QtQml
import QGC

QtObject {
    // Keep the archive singleton alive independently of whether its settings page is open.
    readonly property var archive: MissionArchiveManager
}
