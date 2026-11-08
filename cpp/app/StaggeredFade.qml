import QtQuick

SequentialAnimation {
    id: animation

    property Item targetItem
    property int startDelay: 0
    property int fadeDuration: 260

    PauseAnimation { duration: animation.startDelay }
    NumberAnimation {
        target: animation.targetItem
        property: "opacity"
        from: 0
        to: 1
        duration: animation.fadeDuration
        easing.type: Motion.standardEasing
        easing.bezierCurve: Motion.standardCurve
    }

    onFinished: destroy()
}
