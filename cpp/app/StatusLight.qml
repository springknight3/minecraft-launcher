import QtQuick

Item {
    id: root

    property string status: "stopped"
    property int size: 12

    implicitWidth: size * 2.8
    implicitHeight: size * 2.8

    Rectangle {
        id: pulse
        anchors.centerIn: parent
        width: root.size
        height: root.size
        radius: width / 2
        color: "#46d482"
        opacity: 0
        visible: root.status === "ready"

        SequentialAnimation {
            running: root.status === "ready" && !Motion.reduceMotion
            loops: Animation.Infinite
            ParallelAnimation {
                NumberAnimation { target: pulse; property: "scale"; from: 0.8; to: 2.8; duration: 2000 }
                NumberAnimation { target: pulse; property: "opacity"; from: 0.35; to: 0; duration: 2000 }
            }
        }
    }

    Rectangle {
        id: dot
        anchors.centerIn: parent
        width: root.size
        height: root.size
        radius: width / 2
        color: root.status === "ready" ? "#46d482"
               : root.status === "starting" ? "#56a9ff"
               : root.status === "crashed" ? "#ff6b6b" : "#70819c"

        SequentialAnimation {
            running: root.status === "ready" && !Motion.reduceMotion
            loops: Animation.Infinite
            NumberAnimation { target: dot; property: "opacity"; from: 1; to: 0.7; duration: 1000 }
            NumberAnimation { target: dot; property: "opacity"; from: 0.7; to: 1; duration: 1000 }
        }

        SequentialAnimation {
            running: root.status === "crashed" && !Motion.reduceMotion
            loops: Animation.Infinite
            PauseAnimation { duration: 500 }
            NumberAnimation { target: dot; property: "opacity"; to: 0.7; duration: 1 }
            PauseAnimation { duration: 500 }
            NumberAnimation { target: dot; property: "opacity"; to: 1; duration: 1 }
        }
    }
}
