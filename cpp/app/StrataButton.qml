import QtQuick
import QtQuick.Controls

Button {
    id: control

    implicitHeight: Math.max(36, contentItem.implicitHeight + 16)
    padding: 10
    property string iconName: ""
    property bool dangerous: false
    property bool warning: false
    font.family: "DM Sans"
    font.weight: Font.DemiBold

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.dangerous
              ? (control.down ? "#991b1b" : "#b91c1c")
              : control.warning
                ? (control.down ? "#b45309" : "#d97706")
                : !control.enabled
              ? (ThemeSettings.theme === "dark" ? "#283548" : Theme.borderLight)
              : control.down
                ? (ThemeSettings.theme === "dark" ? "#334e75" : "#bfdbfe")
                : control.checked
                  ? (ThemeSettings.theme === "dark" ? "#334e75" : "#dbeafe")
                  : control.hovered
                    ? (ThemeSettings.theme === "dark" ? "#293a52" : "#eff6ff")
                    : (ThemeSettings.theme === "dark" ? "#1e293b" : "#ffffff")
        border.color: control.dangerous ? "#7f1d1d"
                     : control.warning ? "#92400e"
                     : (ThemeSettings.theme === "dark" ? Theme.borderDark : Theme.borderLight)
        border.width: 1
        opacity: control.flat ? 0.78 : 1
        Behavior on color { ColorAnimation { duration: Motion.fastDuration } }
    }

    contentItem: Item {
        implicitWidth: buttonContent.implicitWidth
        implicitHeight: buttonContent.implicitHeight
        Row {
            id: buttonContent
            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
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
                color: control.dangerous || control.warning
                       ? "#ffffff"
                       : control.enabled
                         ? Theme.textPrimary
                         : (ThemeSettings.theme === "dark" ? "#94a3b8" : "#64748b")
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
    }
}
