pragma Singleton

import QtQuick

QtObject {
    readonly property var standardCurve: [0.25, 0.1, 0.25, 1, 1, 1]
    readonly property var emphasisCurve: [0.22, 1, 0.36, 1, 1, 1]
    readonly property var expressCurve: [0.2, 0.8, 0.2, 1, 1, 1]
    readonly property bool reduceMotion: ThemeSettings.reduceMotion
}
