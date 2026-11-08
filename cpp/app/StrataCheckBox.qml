import QtQuick
import QtQuick.Controls

CheckBox {
    id: control

    spacing: 9
    font.family: "DM Sans"
    implicitHeight: Math.max(26, contentItem.implicitHeight)
    Accessible.name: text

    contentItem: Text {
        text: control.text
        font: control.font
        color: Theme.textPrimary
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
        wrapMode: Text.WordWrap
    }

    indicator: Rectangle {
        implicitWidth: 20
        implicitHeight: 20
        x: 0
        y: (control.height - height) / 2
        radius: 5
        color: control.checked
               ? "#2563eb"
               : (ThemeSettings.theme === "dark" ? Theme.fieldDark : Theme.fieldLight)
        border.color: control.checked
                      ? "#2563eb"
                      : (ThemeSettings.theme === "dark" ? Theme.borderDark : Theme.borderLight)

        Canvas {
            anchors.centerIn: parent
            width: 16
            height: 16
            visible: control.checked || progress > 0
            property real progress: control.checked ? 1 : 0
            Behavior on progress {
                NumberAnimation {
                    duration: Motion.reduceMotion ? 0 : 180
                    easing.type: Motion.standardEasing
                    easing.bezierCurve: Motion.standardCurve
                }
            }
            onProgressChanged: requestPaint()
            onPaint: {
                const context = getContext("2d")
                context.clearRect(0, 0, width, height)
                if (progress <= 0)
                    return
                const first = Math.sqrt(4 * 4 + 4 * 4)
                const second = Math.sqrt(8 * 8 + 8 * 8)
                const drawn = progress * (first + second)
                context.beginPath()
                context.moveTo(2, 8)
                if (drawn <= first) {
                    context.lineTo(2 + drawn / first * 4, 8 + drawn / first * 4)
                } else {
                    context.lineTo(6, 12)
                    context.lineTo(6 + (drawn - first) / second * 8,
                                   12 - (drawn - first) / second * 8)
                }
                context.strokeStyle = "#ffffff"
                context.lineWidth = 2
                context.lineCap = "round"
                context.lineJoin = "round"
                context.stroke()
            }
        }
    }
}
