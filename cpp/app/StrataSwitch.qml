import QtQuick
import QtQuick.Controls

Switch {
    id: control

    spacing: 10
    padding: 0
    implicitWidth: contentItem.implicitWidth + indicator.width + spacing
    implicitHeight: Math.max(contentItem.implicitHeight, indicator.height)
    Accessible.name: text

    contentItem: Text {
        text: control.text
        font.family: "DM Sans"
        font.pixelSize: 14
        color: ThemeSettings.theme === "dark" ? "#eaf2ff" : "#334155"
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Item {
        id: switchIndicator
        width: 44
        height: 26
        x: control.width - width
        y: (control.height - height) / 2

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: control.checked ? "#2563eb"
                                   : (ThemeSettings.theme === "dark" ? "#475569" : "#94a3b8")
            opacity: control.down ? 0.86 : 1
            Behavior on color {
                ColorAnimation {
                    duration: Motion.reduceMotion ? 0 : 180
                    easing.type: Motion.standardEasing
                    easing.bezierCurve: Motion.standardCurve
                }
            }
            Behavior on opacity {
                NumberAnimation { duration: Motion.fastDuration }
            }
        }

        Rectangle {
            id: thumb
            width: 16
            height: 16
            x: control.checked ? 26 : 2
            y: 5
            radius: width / 2
            color: "#ffffff"
            Behavior on x {
                NumberAnimation {
                    duration: Motion.reduceMotion ? 0 : 180
                    easing.type: Motion.standardEasing
                    easing.bezierCurve: Motion.emphasisCurve
                }
            }
        }

        SequentialAnimation {
            id: toggleBounce
            NumberAnimation {
                target: thumb
                property: "scale"
                to: 1.12
                duration: Motion.reduceMotion ? 0 : 90
                easing.type: Motion.standardEasing
            }
            NumberAnimation {
                target: thumb
                property: "scale"
                to: 1
                duration: Motion.reduceMotion ? 0 : 90
                easing.type: Motion.standardEasing
            }
        }
    }

    onCheckedChanged: {
        if (!Motion.reduceMotion)
            toggleBounce.restart()
    }
}
