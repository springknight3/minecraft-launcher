import QtQuick

Rectangle {
    id: root
    color: "transparent"
    visible: false

    Rectangle {
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: 20
        }
        width: 230
        height: 80
        radius: 14
        color: "#111827"
        opacity: 0.8

        Text {
            anchors.centerIn: parent
            text: "Debug frame-time\nOverlay"
            color: "#e2e8f0"
            font.family: "JetBrains Mono"
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
