import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1280
    height: 760
    minimumWidth: 980
    minimumHeight: 620
    visible: true
    title: "StrataLauncher"
    color: ThemeSettings.theme === "dark" ? "#0d1220" : "#eef3fb"

    property bool debugFrameTime: AppDebugFrameTime
    property int selectedInstanceIndex: -1
    property bool toastVisible: false
    property bool createDialogVisible: false
    property bool startupVisible: true
    property bool runtimeManagerVisible: false

    function showToast(message) {
        toastText.text = message
        toastVisible = true
        toastTimer.restart()
    }

    function launchSelected() {
        if (selectedInstanceIndex >= 0) {
            const instance = instanceModel.get(selectedInstanceIndex)
            LauncherController.openConsole(instance.id)
            if (instance.status === "starting" || instance.status === "ready") {
                LauncherController.stopInstance(instance.id)
            } else {
                LauncherController.launchInstance(instance.id)
            }
        }
    }

    ListModel {
        id: instanceModel
    }

    Connections {
        target: LauncherController
        function onInstancesChanged() {
            const selectedId = selectedInstanceIndex >= 0 && selectedInstanceIndex < instanceModel.count
                    ? instanceModel.get(selectedInstanceIndex).id : ""
            instanceModel.clear()
            const loaded = LauncherController.instances
            let restoredSelection = -1
            for (let index = 0; index < loaded.length; ++index) {
                instanceModel.append(loaded[index])
                if (loaded[index].id === selectedId)
                    restoredSelection = index
            }
            selectedInstanceIndex = restoredSelection >= 0
                    ? restoredSelection : (instanceModel.count > 0 ? 0 : -1)
        }
        function onOfflineAccountChanged() {
            offlineNameInput.text = LauncherController.offlineUsername
        }
        function onErrorMessageChanged() {
            if (LauncherController.errorMessage.length > 0) {
                root.showToast(LauncherController.errorMessage)
            }
        }
        function onInstallFinished(success, instanceName, error) {
            if (success) {
                createDialogVisible = false
                root.showToast("Installed " + instanceName)
            }
        }
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_F9) {
            root.debugFrameTime = !root.debugFrameTime
            event.accepted = true
        }
    }

    Timer {
        id: startupTimer
        interval: 3000
        running: true
        repeat: false
        onTriggered: {
            startupVisible = false
            LauncherController.refresh()
        }
    }

    Timer {
        id: toastTimer
        interval: 2200
        repeat: false
        onTriggered: toastVisible = false
    }

    Rectangle {
        anchors.fill: parent
        color: root.color
    }

    Rectangle {
        id: startupOverlay
        anchors.fill: parent
        visible: startupVisible
        color: ThemeSettings.theme === "dark" ? "#090d15" : "#eaf1ff"
        z: 50

        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.InOutQuad } }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 18
            width: Math.min(parent.width * 0.65, 700)

            Label {
                text: "StrataLauncher"
                font.family: "Outfit"
                font.pixelSize: 42
                color: ThemeSettings.theme === "dark" ? "#f8fbff" : "#101827"
                Layout.alignment: Qt.AlignHCenter
            }

            Rectangle {
                width: 102
                height: 102
                radius: 24
                color: ThemeSettings.theme === "dark" ? "#1a2433" : "#dfeaff"
                border.color: ThemeSettings.theme === "dark" ? "#2d465d" : "#bfd2ff"
                Layout.alignment: Qt.AlignHCenter
                ThemeAwareIcon {
                    anchors.centerIn: parent
                    iconName: "home"
                    size: 48
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: ThemeSettings.theme === "dark" ? "#24364d" : "#d6def5"
            }

            Text {
                text: "This is a hobby project and not meant to replace anything! May contain security issues and flaws! Not affiliated with Mojang Studios, Microsoft, or any other official platform partner."
                width: parent.width
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                font.family: "DM Sans"
                font.pixelSize: 14
                color: ThemeSettings.theme === "dark" ? "#dfe7f3" : "#334155"
            }

            ProgressBar {
                from: 0
                to: 1
                value: 0.72
                Layout.fillWidth: true
                Layout.maximumWidth: 420
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }

    Rectangle {
        id: shell
        anchors.fill: parent
        color: root.color
        visible: !startupVisible

        RowLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 18

            Rectangle {
                id: sidebar
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                radius: 18
                color: ThemeSettings.theme === "dark" ? "#121c2c" : "#ffffff"
                border.color: ThemeSettings.theme === "dark" ? "#1f2d3d" : "#e3e8f0"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Label {
                        text: "StrataLauncher"
                        font.family: "Outfit"
                        font.pixelSize: 24
                        color: ThemeSettings.theme === "dark" ? "#f4f7ff" : "#101827"
                        Layout.alignment: Qt.AlignLeft
                    }

                    Repeater {
                        model: ["Home", "Instances", "Servers", "Settings"]
                        delegate: Button {
                            property bool selected: index === 1
                            Layout.fillWidth: true
                            height: 42
                            text: modelData
                            flat: true
                            onClicked: {
                                if (modelData === "Instances") {
                                    runtimeManagerVisible = false
                                    selectedInstanceIndex = Math.max(0, selectedInstanceIndex)
                                } else if (modelData === "Settings") {
                                    runtimeManagerVisible = true
                                }
                                showToast(modelData + " selected")
                            }
                            contentItem: Row {
                                spacing: 10
                                ThemeAwareIcon {
                                    iconName: modelData.toLowerCase()
                                    size: 18
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Label {
                                    text: parent.parent.text
                                    font.family: "DM Sans"
                                    font.pixelSize: 15
                                    color: parent.parent.selected ? (ThemeSettings.theme === "dark" ? "#9ed8ff" : "#0f4fd8") : (ThemeSettings.theme === "dark" ? "#dfe7f1" : "#475569")
                                }
                            }
                            background: Rectangle {
                                radius: 10
                                color: selected ? (ThemeSettings.theme === "dark" ? "#162638" : "#eaf2ff") : "transparent"
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    Button {
                        Layout.fillWidth: true
                        text: "Reduce motion"
                        flat: true
                        onClicked: ThemeSettings.reduceMotion = !ThemeSettings.reduceMotion
                        contentItem: Row {
                            spacing: 8
                            ThemeAwareIcon { iconName: "reduce-motion"; size: 16 }
                            Label {
                                text: parent.parent.text
                                font.family: "DM Sans"
                                font.pixelSize: 14
                                color: ThemeSettings.theme === "dark" ? "#eaf2ff" : "#334155"
                            }
                        }
                    }

                    Switch {
                        checked: ThemeSettings.theme === "dark"
                        text: "Dark mode"
                        onToggled: ThemeSettings.toggleTheme()
                    }
                }
            }

            Rectangle {
                id: mainPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 18
                color: ThemeSettings.theme === "dark" ? "#0f1723" : "#ffffff"
                border.color: ThemeSettings.theme === "dark" ? "#1d2b3e" : "#e4e8f0"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 18

                    RowLayout {
                        spacing: 12
                        Layout.fillWidth: true

                        Label {
                            text: "Instances"
                            font.family: "Outfit"
                            font.pixelSize: 34
                            color: ThemeSettings.theme === "dark" ? "#f4f7ff" : "#0f172a"
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: "Create"
                            onClicked: createDialogVisible = true
                            background: Rectangle { radius: 10; color: ThemeSettings.theme === "dark" ? "#1d2b40" : "#e7efff" }
                            contentItem: Label { text: "Create"; color: ThemeSettings.theme === "dark" ? "#dfeeff" : "#1d4ed8" }
                        }

                        Button {
                            text: "Duplicate"
                            onClicked: showToast("Duplicate instance")
                            background: Rectangle { radius: 10; color: ThemeSettings.theme === "dark" ? "#182335" : "#f3f6fb" }
                            contentItem: Label { text: "Duplicate"; color: ThemeSettings.theme === "dark" ? "#e9f3ff" : "#1f2937" }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label {
                            text: "Offline player"
                            font.family: "DM Sans"
                            color: ThemeSettings.theme === "dark" ? "#dfe7f3" : "#475569"
                        }

                        TextField {
                            id: offlineNameInput
                            Layout.preferredWidth: 220
                            placeholderText: "1-16 letters, numbers, underscores"
                            text: LauncherController.offlineUsername
                            maximumLength: 16
                        }

                        Button {
                            text: "Save profile"
                            onClicked: LauncherController.saveOfflineUsername(offlineNameInput.text)
                        }

                        Item { Layout.fillWidth: true }
                    }

                    Rectangle {
                        visible: createDialogVisible && !LauncherController.installing
                        Layout.fillWidth: true
                        height: 250
                        radius: 14
                        color: ThemeSettings.theme === "dark" ? "#121f2f" : "#f8fafc"
                        border.color: ThemeSettings.theme === "dark" ? "#233349" : "#dfe5f3"

                        ColumnLayout {
                            anchors.centerIn: parent
                            width: parent.width - 36
                            spacing: 10

                            Label {
                                text: "Create instance"
                                font.family: "Outfit"
                                font.pixelSize: 22
                                color: ThemeSettings.theme === "dark" ? "#f6f8ff" : "#111827"
                            }

                            TextField {
                                id: instanceNameInput
                                Layout.fillWidth: true
                                placeholderText: "Instance name"
                                text: LauncherController.latestRelease.length > 0
                                      ? "Minecraft " + LauncherController.latestRelease : ""
                            }

                            ComboBox {
                                id: minecraftVersionBox
                                Layout.fillWidth: true
                                model: LauncherController.versions
                                textRole: "label"
                                valueRole: "id"
                                currentIndex: {
                                    const available = LauncherController.versions
                                    for (let index = 0; index < available.length; ++index) {
                                        if (available[index].id === LauncherController.latestRelease)
                                            return index
                                    }
                                    return available.length > 0 ? 0 : -1
                                }
                            }

                            Label {
                                text: LauncherController.usingCachedManifest
                                      ? "Using cached version data while offline."
                                      : LauncherController.versions.length > 0
                                        ? "Choose a version to install its metadata and client files."
                                        : LauncherController.errorMessage.length > 0
                                          ? "Could not load versions: " + LauncherController.errorMessage
                                          : "Fetching Mojang version data..."
                                font.family: "DM Sans"
                                font.pixelSize: 13
                                color: ThemeSettings.theme === "dark" ? "#aebedd" : "#64748b"
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: "Cancel"
                                    onClicked: createDialogVisible = false
                                }
                                Button {
                                    text: "Install"
                                    enabled: instanceNameInput.text.trim().length > 0
                                             && minecraftVersionBox.currentIndex >= 0
                                             && !LauncherController.installing
                                    onClicked: LauncherController.installInstance(
                                                   instanceNameInput.text, minecraftVersionBox.currentValue)
                                }
                            }
                        }
                    }

                    ListView {
                        id: instanceList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 12
                        model: instanceModel
                        clip: true
                        currentIndex: selectedInstanceIndex
                        highlightMoveDuration: 150

                        delegate: Rectangle {
                            width: instanceList.width
                            height: 92
                            radius: 16
                            color: (instanceList.currentIndex === index) ? (ThemeSettings.theme === "dark" ? "#1a2536" : "#eef6ff") : (ThemeSettings.theme === "dark" ? "#101b2a" : "#f7fafc")
                            border.color: (instanceList.currentIndex === index) ? (ThemeSettings.theme === "dark" ? "#3a6ea7" : "#c8ddff") : (ThemeSettings.theme === "dark" ? "#172638" : "#edf2f7")
                            property bool isSelected: instanceList.currentIndex === index

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    selectedInstanceIndex = index
                                    instanceList.currentIndex = index
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 16

                                Rectangle {
                                    width: 12
                                    height: 12
                                    radius: 6
                                    color: {
                                        switch (status) {
                                        case "ready": return "#46d482"
                                        case "starting": return "#56a9ff"
                                        case "crashed": return "#ff6b6b"
                                        default: return "#70819c"
                                        }
                                    }
                                }

                                ColumnLayout {
                                    spacing: 4
                                    Layout.fillWidth: true

                                    Label {
                                        text: name
                                        font.family: "Outfit"
                                        font.pixelSize: 22
                                        color: ThemeSettings.theme === "dark" ? "#edf4ff" : "#111827"
                                    }

                                    Label {
                                        text: "Minecraft " + version
                                        font.family: "DM Sans"
                                        font.pixelSize: 13
                                        color: ThemeSettings.theme === "dark" ? "#b7c6dc" : "#64748b"
                                    }
                                }

                                Row {
                                    spacing: 8
                                    Button {
                                        text: status === "starting" || status === "ready" ? "Stop" : "Launch"
                                        enabled: instanceList.currentIndex === index
                                        onClicked: root.launchSelected()
                                        background: Rectangle { radius: 10; color: ThemeSettings.theme === "dark" ? "#2a4d7d" : "#dfeeff" }
                                        contentItem: Label { text: parent.text; color: ThemeSettings.theme === "dark" ? "#ecf8ff" : "#0f4fd8" }
                                    }
                                    Button {
                                        text: "Settings"
                                        onClicked: settingsFlyout.visible = true
                                        background: Rectangle { radius: 10; color: ThemeSettings.theme === "dark" ? "#1a2431" : "#f3f6fb" }
                                        contentItem: Label { text: parent.text; color: ThemeSettings.theme === "dark" ? "#edf4ff" : "#1f2937" }
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        spacing: 12
                        Layout.fillWidth: true

                        Button {
                            id: launchButton
                            text: selectedInstanceIndex >= 0
                                  && (instanceModel.get(selectedInstanceIndex).status === "starting"
                                      || instanceModel.get(selectedInstanceIndex).status === "ready")
                                  ? "Stop" : "Launch"
                            enabled: selectedInstanceIndex >= 0
                            onClicked: root.launchSelected()
                            background: Rectangle {
                                radius: 12
                                color: enabled ? (ThemeSettings.theme === "dark" ? "#2a5dce" : "#2f80ed") : (ThemeSettings.theme === "dark" ? "#1e2d46" : "#dfe7f0")
                            }
                            contentItem: Label {
                                text: parent.text
                                color: enabled ? "#ffffff" : (ThemeSettings.theme === "dark" ? "#94a3b8" : "#7a8aa2")
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        Label {
                            text: selectedInstanceIndex >= 0 ? "Selected: " + instanceModel.get(selectedInstanceIndex).name : "No instance selected"
                            font.family: "DM Sans"
                            font.pixelSize: 14
                            color: ThemeSettings.theme === "dark" ? "#d8e3f7" : "#475569"
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: settingsFlyout
        visible: false
        anchors.centerIn: parent
        width: 620
        height: 420
        radius: 18
        color: ThemeSettings.theme === "dark" ? "#111b2a" : "#ffffff"
        border.color: ThemeSettings.theme === "dark" ? "#1d2c3d" : "#dfe6f5"
        z: 30

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: "Instance settings"
                    font.family: "Outfit"
                    font.pixelSize: 28
                    color: ThemeSettings.theme === "dark" ? "#edf4ff" : "#0f172a"
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: "Close"
                    onClicked: settingsFlyout.visible = false
                }
            }

            Label {
                text: selectedInstanceIndex >= 0 ? "Configure " + instanceModel.get(selectedInstanceIndex).name + " for launch, versions, and loader settings." : "Pick an instance to configure."
                font.family: "DM Sans"
                font.pixelSize: 15
                color: ThemeSettings.theme === "dark" ? "#dfe7f1" : "#475569"
                wrapMode: Text.WordWrap
            }

            Label {
                text: "Java runtime: " + (selectedInstanceIndex >= 0
                      && instanceModel.get(selectedInstanceIndex).runtimeMode === "MANAGED"
                      ? "Managed · " + instanceModel.get(selectedInstanceIndex).runtimeComponent
                      : "System Java")
                font.family: "DM Sans"
                font.pixelSize: 14
                color: ThemeSettings.theme === "dark" ? "#b7c6dc" : "#64748b"
            }

            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: "Install and use " + LauncherController.runtimeVendor + " Java"
                    enabled: selectedInstanceIndex >= 0 && !LauncherController.installingRuntime
                    onClicked: LauncherController.installInstanceRuntime(
                                   instanceModel.get(selectedInstanceIndex).id)
                }
                Button {
                    text: "Use System Java"
                    enabled: selectedInstanceIndex >= 0
                    onClicked: LauncherController.useSystemJava(
                                   instanceModel.get(selectedInstanceIndex).id)
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 12
                color: ThemeSettings.theme === "dark" ? "#121d31" : "#f3f7ff"
                border.color: ThemeSettings.theme === "dark" ? "#21354d" : "#dfe8f7"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Label { text: "Version"; font.family: "DM Sans"; color: ThemeSettings.theme === "dark" ? "#dfe7f3" : "#475569" }
                    ComboBox {
                        model: ["1.21.4", "1.21.3", "1.20.6"]
                        currentIndex: 0
                        Layout.fillWidth: true
                    }
                    Label { text: "Java runtime"; font.family: "DM Sans"; color: ThemeSettings.theme === "dark" ? "#dfe7f3" : "#475569" }
                    ComboBox {
                        model: ["Use system Java", "Temurin 21", "Zulu 21"]
                        currentIndex: 1
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }

    Rectangle {
        id: consolePanel
        visible: LauncherController.consoleInstanceName.length > 0
        anchors.centerIn: parent
        width: Math.min(parent.width - 80, 900)
        height: Math.min(parent.height - 80, 560)
        radius: 18
        color: ThemeSettings.theme === "dark" ? "#101927" : "#ffffff"
        border.color: ThemeSettings.theme === "dark" ? "#293d58" : "#dce4ef"
        z: 35

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: LauncherController.consoleInstanceName + " console"
                    font.family: "Outfit"
                    font.pixelSize: 24
                    color: ThemeSettings.theme === "dark" ? "#edf4ff" : "#101827"
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "Stop"
                    onClicked: LauncherController.stopConsoleInstance()
                }
                Button {
                    text: "Close"
                    onClicked: LauncherController.closeConsole()
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                TextArea {
                    text: LauncherController.consoleText
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.NoWrap
                    font.family: "JetBrains Mono"
                    font.pixelSize: 12
                    color: ThemeSettings.theme === "dark" ? "#dbeafe" : "#1e293b"
                    background: Rectangle {
                        color: ThemeSettings.theme === "dark" ? "#090f19" : "#f7f9fc"
                        radius: 10
                    }
                }
            }
        }
    }

    Rectangle {
        id: runtimeManagerPanel
        visible: runtimeManagerVisible
        anchors.centerIn: parent
        width: Math.min(parent.width - 80, 900)
        height: Math.min(parent.height - 80, 620)
        radius: 18
        color: ThemeSettings.theme === "dark" ? "#111b2a" : "#ffffff"
        border.color: ThemeSettings.theme === "dark" ? "#1d2c3d" : "#dfe6f5"
        z: 32

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "Java Runtime Manager"
                    font.family: "Outfit"
                    font.pixelSize: 28
                    color: ThemeSettings.theme === "dark" ? "#edf4ff" : "#0f172a"
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "Refresh"
                    onClicked: LauncherController.refreshRuntimes()
                }
                Button {
                    text: "Close"
                    onClicked: runtimeManagerVisible = false
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                Label {
                    text: "Default vendor"
                    color: ThemeSettings.theme === "dark" ? "#dfe7f3" : "#475569"
                }

                ComboBox {
                    id: runtimeVendorBox
                    model: [
                        { "label": "Mojang", "value": "MOJANG" },
                        { "label": "Eclipse Temurin", "value": "TEMURIN" },
                        { "label": "Azul Zulu", "value": "ZULU" }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: {
                        for (let index = 0; index < count; ++index)
                            if (model[index].value === LauncherController.runtimeVendor)
                                return index
                        return 0
                    }
                    onActivated: LauncherController.setRuntimePreferences(
                                     currentValue, redundantRuntimeSwitch.checked)
                }

                CheckBox {
                    id: redundantRuntimeSwitch
                    text: "Keep a separate copy per instance"
                    checked: LauncherController.redundantRuntimes
                    onToggled: LauncherController.setRuntimePreferences(
                                   runtimeVendorBox.currentValue, checked)
                }
            }

            Label {
                text: "Shared storage is the default. Changing this preference affects new installs; existing instances keep their saved Java component."
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                color: ThemeSettings.theme === "dark" ? "#aebedd" : "#64748b"
            }

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "Installed runtimes"
                    font.family: "Outfit"
                    font.pixelSize: 20
                    color: ThemeSettings.theme === "dark" ? "#edf4ff" : "#0f172a"
                }
                Item { Layout.fillWidth: true }
                TextField {
                    id: runtimeDeleteConfirmation
                    placeholderText: "Type DELETE to remove an unused runtime"
                    Layout.preferredWidth: 300
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                model: LauncherController.installedRuntimes
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 74
                    radius: 12
                    color: ThemeSettings.theme === "dark" ? "#121f2f" : "#f8fafc"
                    border.color: ThemeSettings.theme === "dark" ? "#233349" : "#dfe5f3"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 14
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Label {
                                text: vendor + " Java " + version
                                font.family: "Outfit"
                                font.pixelSize: 17
                                color: ThemeSettings.theme === "dark" ? "#edf4ff" : "#101827"
                            }
                            Label {
                                text: users.length > 0 ? "Used by " + users.join(", ") : "Not currently used"
                                font.family: "DM Sans"
                                font.pixelSize: 12
                                color: ThemeSettings.theme === "dark" ? "#aebedd" : "#64748b"
                            }
                        }
                        Button {
                            text: "Remove"
                            enabled: !inUse && runtimeDeleteConfirmation.text === "DELETE"
                            onClicked: LauncherController.deleteRuntime(directory)
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        visible: LauncherController.installingRuntime
        anchors.fill: parent
        color: "#b3000000"
        z: 56

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.72, 560)
            spacing: 18
            Label {
                text: "Installing Java runtime..."
                color: "#ffffff"
                font.family: "Outfit"
                font.pixelSize: 28
                Layout.alignment: Qt.AlignHCenter
            }
            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: 1
                value: LauncherController.runtimeInstallationProgress
            }
            Label {
                text: LauncherController.runtimeInstallationItem
                color: "#c5ccd6"
                font.family: "DM Sans"
                font.pixelSize: 14
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }

    Rectangle {
        id: toast
        visible: toastVisible
        width: 320
        height: 56
        radius: 16
        color: ThemeSettings.theme === "dark" ? "#182a3e" : "#ffffff"
        border.color: ThemeSettings.theme === "dark" ? "#293d58" : "#e2e8f0"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        z: 40

        Text {
            id: toastText
            anchors.centerIn: parent
            text: "Launching …"
            color: ThemeSettings.theme === "dark" ? "#edf4ff" : "#111827"
            font.family: "DM Sans"
            font.pixelSize: 14
        }
    }

    FrameTimeOverlay {
        id: overlay
        anchors.fill: parent
        visible: root.debugFrameTime
        z: 60
    }

    Rectangle {
        anchors.fill: parent
        visible: LauncherController.installing
        color: "#b3000000"
        z: 55

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.72, 560)
            spacing: 18

            Label {
                text: "Installing " + LauncherController.currentInstallName + "..."
                color: "#ffffff"
                font.family: "Outfit"
                font.pixelSize: 30
                Layout.alignment: Qt.AlignHCenter
            }

            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: 1
                value: LauncherController.installationProgress
            }

            Label {
                text: LauncherController.currentInstallItem.length > 0
                      ? "Downloading: " + LauncherController.currentInstallItem : "Preparing downloads..."
                color: "#c5ccd6"
                font.family: "DM Sans"
                font.pixelSize: 14
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
}
