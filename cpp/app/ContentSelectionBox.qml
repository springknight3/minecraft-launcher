import QtQuick

Item {
    id: control

    property bool checked: false
    property string text: ""
    signal selected(int modifiers)

    implicitWidth: visual.implicitWidth
    implicitHeight: visual.implicitHeight

    StrataCheckBox {
        id: visual
        anchors.fill: parent
        checked: control.checked
        text: control.text
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: mouse => control.selected(mouse.modifiers)
    }
}
