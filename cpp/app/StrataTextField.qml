import QtQuick
import QtQuick.Controls

TextField {
    id: control

    font.family: "DM Sans"
    color: Theme.textPrimary
    placeholderTextColor: ThemeSettings.theme === "dark" ? "#94a3b8" : "#64748b"
    selectionColor: ThemeSettings.theme === "dark" ? "#2563eb" : "#93c5fd"
    selectedTextColor: "#ffffff"

    background: Rectangle {
        radius: Theme.radiusSmall
        color: ThemeSettings.theme === "dark" ? Theme.fieldDark : Theme.fieldLight
        border.color: control.activeFocus
                      ? (ThemeSettings.theme === "dark" ? "#60a5fa" : "#3b82f6")
                      : (ThemeSettings.theme === "dark" ? Theme.borderDark : Theme.borderLight)
        border.width: control.activeFocus ? 2 : 1
    }
}
