import QtQuick
import QtQuick.Controls

TabButton {
    id: control

    font.family: "DM Sans"
    font.weight: Font.DemiBold
    property string iconName: ""
    property bool dangerous: false
    contentItem: Row {
        anchors.centerIn: parent
        spacing: 7
        ThemeAwareIcon {
            visible: control.iconName.length > 0
            iconName: control.iconName
            size: 16
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: control.text
            font: control.font
            color: control.dangerous ? "#ffffff" : Theme.textPrimary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        anchors.margins: 4
        radius: Theme.radiusMedium
        color: control.dangerous
               ? (control.checked ? "#b91c1c" : "#7f1d1d")
               : control.checked
               ? (ThemeSettings.theme === "dark" ? Theme.selectionDark : Theme.selectionLight)
               : "transparent"
        border.color: control.dangerous ? "#ef4444"
                      : control.checked
                      ? (ThemeSettings.theme === "dark" ? "#60a5fa" : "#3b82f6")
                      : "transparent"
    }
}
