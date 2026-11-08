import QtQuick

Item {
    id: root

    property string iconName: "home"
    property int size: 18

    implicitWidth: size
    implicitHeight: size

    Image {
        anchors.centerIn: parent
        width: root.size
        height: root.size
        fillMode: Image.PreserveAspectFit
        objectName: "themeAwareIconImage"
        source: {
            const themeSuffix = ThemeSettings.theme === "dark" ? "light" : "dark";
            return "qrc:/assets/icons/" + root.iconName + "-" + themeSuffix + ".svg";
        }
    }
}
