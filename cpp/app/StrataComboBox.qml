import QtQuick
import QtQuick.Controls

ComboBox {
    id: control

    font.family: "DM Sans"
    palette.text: Theme.textPrimary
    palette.buttonText: Theme.textPrimary
    implicitHeight: 38

    contentItem: Text {
        leftPadding: 10
        rightPadding: 30
        text: control.displayText.length > 0 ? control.displayText : control.currentText
        font: control.font
        color: control.enabled
               ? Theme.textPrimary
               : (ThemeSettings.theme === "dark" ? "#94a3b8" : "#64748b")
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 12
        y: (control.height - height) / 2
        text: "▾"
        color: ThemeSettings.theme === "dark" ? "#cbd5e1" : "#475569"
        font.pixelSize: 16
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: ThemeSettings.theme === "dark" ? "#0f172a" : "#f1f5f9"
        border.color: control.activeFocus
                      ? (ThemeSettings.theme === "dark" ? "#60a5fa" : "#3b82f6")
                      : (ThemeSettings.theme === "dark" ? Theme.borderDark : Theme.borderLight)
        border.width: control.activeFocus ? 2 : 1
    }

    delegate: ItemDelegate {
        width: control.width
        contentItem: Text {
            text: {
                if (control.textRole && model[control.textRole] !== undefined)
                    return model[control.textRole]
                if (typeof modelData === "string" || typeof modelData === "number")
                    return modelData.toString()
                if (modelData && modelData.label !== undefined)
                    return modelData.label
                if (modelData && modelData.text !== undefined)
                    return modelData.text
                return ""
            }
            font: control.font
            color: Theme.textPrimary
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        highlighted: control.highlightedIndex === index
        background: Rectangle {
            color: parent.highlighted
                   ? (ThemeSettings.theme === "dark" ? Theme.selectionDark : Theme.selectionLight)
                   : (ThemeSettings.theme === "dark" ? "#1e293b" : "#ffffff")
        }
    }

    popup: Popup {
        y: control.height
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight, 280)
        padding: 4
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            radius: Theme.radiusSmall
            color: ThemeSettings.theme === "dark" ? "#1e293b" : "#ffffff"
            border.color: ThemeSettings.theme === "dark" ? "#475569" : "#cbd5e1"
        }
    }
}
