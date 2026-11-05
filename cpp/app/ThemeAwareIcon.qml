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
        source: {
            const themeSuffix = ThemeSettings.theme === "dark" ? "dark" : "light";
            return "file:///home/eli/Projects/minecraft-launcher/TEMPORARY%20ASSETS/" + root.iconName + "-" + themeSuffix + ".svg";
        }
    }
}
