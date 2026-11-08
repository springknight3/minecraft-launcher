import QtQuick
import QtQuick.Controls

SpinBox {
    id: control

    implicitWidth: 132
    implicitHeight: 38
    font.family: "DM Sans"
    palette.text: Theme.textPrimary
    palette.buttonText: Theme.textPrimary

    contentItem: TextInput {
        z: 2
        text: control.displayText
        font: control.font
        color: control.enabled ? Theme.textPrimary : Theme.textSecondary
        selectionColor: Theme.selectionLight
        selectedTextColor: Theme.textPrimary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        onAccepted: control.value = control.valueFromText(text, control.locale)
    }

    up.indicator: Rectangle {
        x: control.width - width - 3
        y: 3
        width: 30
        height: control.height / 2 - 3
        radius: Theme.radiusSmall
        color: control.up.pressed ? "#1d4ed8"
               : ThemeSettings.theme === "dark" ? "#334e75" : "#cbd5e1"
        border.color: ThemeSettings.theme === "dark" ? Theme.borderDark : Theme.borderLight
        Text {
            anchors.centerIn: parent
            text: "+"
            color: "#ffffff"
            font.family: "DM Sans"
            font.pixelSize: 16
        }
    }

    down.indicator: Rectangle {
        x: control.width - width - 3
        y: control.height / 2
        width: 30
        height: control.height / 2 - 3
        radius: Theme.radiusSmall
        color: control.down.pressed ? "#1d4ed8"
               : ThemeSettings.theme === "dark" ? "#334e75" : "#cbd5e1"
        border.color: ThemeSettings.theme === "dark" ? Theme.borderDark : Theme.borderLight
        Text {
            anchors.centerIn: parent
            text: "-"
            color: "#ffffff"
            font.family: "DM Sans"
            font.pixelSize: 16
        }
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: ThemeSettings.theme === "dark" ? Theme.fieldDark : Theme.fieldLight
        border.color: control.activeFocus
                      ? (ThemeSettings.theme === "dark" ? "#60a5fa" : "#3b82f6")
                      : (ThemeSettings.theme === "dark" ? Theme.borderDark : Theme.borderLight)
    }
}
