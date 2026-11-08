import QtQuick
import QtQuick.Window

Item {
    id: root
    objectName: "frameTimeOverlay"

    visible: false
    property double lastFrameTimestamp: 0
    property var frameDeltas: []
    readonly property real averageFrameTime: frameDeltas.length > 0
                                             ? frameDeltas.reduce((total, value) => total + value, 0)
                                               / frameDeltas.length
                                             : 0
    readonly property real slowFramePercent: frameDeltas.length > 0
                                             ? 100 * frameDeltas.filter(value => value > 16.67).length
                                               / frameDeltas.length
                                             : 0

    function recordFrame() {
        const now = Date.now()
        if (lastFrameTimestamp > 0) {
            const samples = frameDeltas.slice()
            samples.push(now - lastFrameTimestamp)
            if (samples.length > 120)
                samples.shift()
            frameDeltas = samples
        }
        lastFrameTimestamp = now
    }

    function clearSamples() {
        lastFrameTimestamp = 0
        frameDeltas = []
    }

    Connections {
        target: root.Window.window
        enabled: root.visible
        function onFrameSwapped() {
            root.recordFrame()
        }
    }

    Rectangle {
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: 20
        }
        width: 250
        height: 96
        radius: Theme.radiusMedium
        color: ThemeSettings.theme === "dark" ? "#111827" : "#ffffff"
        border.color: ThemeSettings.theme === "dark" ? "#334155" : "#cbd5e1"
        opacity: 0.94

        Text {
            anchors.centerIn: parent
            text: "Frame time  " + root.averageFrameTime.toFixed(1) + " ms\n"
                  + "Slow frames  " + root.slowFramePercent.toFixed(1) + "%\n"
                  + "Samples      " + root.frameDeltas.length
            color: ThemeSettings.theme === "dark" ? "#e2e8f0" : "#1e293b"
            font.family: "JetBrains Mono"
            font.pixelSize: 13
            horizontalAlignment: Text.AlignLeft
            Accessible.name: "Debug frame-time statistics"
        }
    }
}
