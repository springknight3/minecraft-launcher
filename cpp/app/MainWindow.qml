import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1280
    height: 760
    minimumWidth: 980
    minimumHeight: 620
    visible: true
    title: "StrataLauncher"
    color: Theme.backgroundStart

    property bool debugFrameTime: AppDebugFrameTime
    property int selectedInstanceIndex: -1
    property bool toastVisible: false
    property bool startupEntranceComplete: false
    property bool startupMinimumElapsed: false
    property bool startupInitializationStarted: false
    property bool createDialogVisible: false
    property bool startupVisible: true
    property bool runtimeManagerVisible: false
    property bool accountFlyoutVisible: false
    property string pendingRemoveAccountId: ""
    property string selectedServerId: ""
    property string pendingServerDeletionId: ""
    property string pendingEulaStartServerId: ""
    property var pendingRuntimeDeletion: null
    property string serverContentProjectId: ""
    property int serverTabIndex: 0
    property string capturingControlKey: ""
    property var controlDraft: ({})
    property string selectedPage: "Home"
    property string settingsTab: "General"
    property bool sidebarCollapsed: false
    property real sidebarWidth: sidebarCollapsed ? 64 : 250
    property real serverDetailWidth: selectedServer !== null ? 600 : 0
    property var selectedContentIndices: []
    property int lastContentSelection: -1
    property var installedBulkQueue: []
    property string installedBulkAction: ""
    property string pendingRuntimeVendor: LauncherController.runtimeVendor
    property bool pendingRedundantRuntimes: LauncherController.redundantRuntimes
    property bool pendingDarkMode: ThemeSettings.theme === "dark"
    property bool pendingReduceMotion: ThemeSettings.reduceMotion
    property bool pendingFrameTimeOverlay: ThemeSettings.frameTimeOverlayEnabled
    property bool pendingTrackInstanceLaunches: LauncherController.trackInstanceLaunches
    property var contentController: LauncherController.contentController
    property var instanceSettingsController: LauncherController.instanceSettingsController
    property var selectedServer: {
        const servers = LauncherController.servers
        for (let index = 0; index < servers.length; ++index) {
            if (servers[index].id === selectedServerId)
                return servers[index]
        }
        return null
    }
    Behavior on sidebarWidth {
        NumberAnimation {
            duration: Motion.reduceMotion ? 0 : Motion.slowDuration
            easing.type: Motion.standardEasing
            easing.bezierCurve: Motion.emphasisCurve
        }
    }
    Behavior on serverDetailWidth {
        NumberAnimation {
            duration: Motion.reduceMotion ? 0 : Motion.slowDuration
            easing.type: Motion.standardEasing
            easing.bezierCurve: Motion.emphasisCurve
        }
    }
    function beginLauncherSettingsEdit() {
        pendingDarkMode = ThemeSettings.theme === "dark"
        pendingReduceMotion = ThemeSettings.reduceMotion
        pendingFrameTimeOverlay = ThemeSettings.frameTimeOverlayEnabled
        pendingTrackInstanceLaunches = LauncherController.trackInstanceLaunches
        pendingRuntimeVendor = LauncherController.runtimeVendor
        pendingRedundantRuntimes = LauncherController.redundantRuntimes
        launcherClientIdInput.text = LauncherController.microsoftClientId
        globalJavaFlagsField.text = LauncherController.globalJavaArguments.join("\n")
    }

    function openLauncherSettings() {
        beginLauncherSettingsEdit()
        settingsTab = "General"
        runtimeManagerVisible = true
        Qt.callLater(() => staggerPageItems(generalSettings))
    }

    function closeLauncherSettings() {
        runtimeManagerVisible = false
    }

    function saveLauncherSettings() {
        ThemeSettings.theme = pendingDarkMode ? "dark" : "light"
        ThemeSettings.reduceMotion = pendingReduceMotion
        ThemeSettings.frameTimeOverlayEnabled = pendingFrameTimeOverlay
        LauncherController.setTrackInstanceLaunches(pendingTrackInstanceLaunches)
        root.debugFrameTime = AppDebugFrameTimeForced || pendingFrameTimeOverlay
        LauncherController.setMicrosoftClientId(launcherClientIdInput.text)
        LauncherController.setGlobalJavaArguments(globalJavaFlagsField.text)
        LauncherController.setRuntimePreferences(pendingRuntimeVendor, pendingRedundantRuntimes)
        root.showToast("Saving launcher settings…")
        closeLauncherSettings()
    }

    function toggleInstalledContent(index, shiftHeld) {
        const selected = selectedContentIndices.slice()
        if (shiftHeld && lastContentSelection >= 0) {
            const start = Math.min(lastContentSelection, index)
            const end = Math.max(lastContentSelection, index)
            for (let item = start; item <= end; ++item) {
                if (selected.indexOf(item) < 0)
                    selected.push(item)
            }
        } else {
            const selectedIndex = selected.indexOf(index)
            if (selectedIndex < 0)
                selected.push(index)
            else
                selected.splice(selectedIndex, 1)
        }
        selectedContentIndices = selected
        lastContentSelection = index
    }

    function applyInstalledContentAction(action) {
        installedBulkQueue = selectedContentIndices.slice().sort((left, right) => right - left)
        installedBulkAction = action
        selectedContentIndices = []
        lastContentSelection = -1
        if (installedBulkQueue.length > 0)
            installedBulkTimer.start()
    }

    function collectPageItems(container, pageItems) {
        const children = container.children
        for (let index = 0; index < children.length && pageItems.length < 6; ++index) {
            const item = children[index]
            if (!item.visible)
                continue
            if (!(item instanceof Control) && item.children.length > 0)
                collectPageItems(item, pageItems)
            else if ("opacity" in item)
                pageItems.push(item)
        }
    }

    function bindControlKey(event) {
        if (capturingControlKey.length === 0)
            return
        if (event.key === Qt.Key_Escape) {
            capturingControlKey = ""
            event.accepted = true
            return
        }
        let keyName = Qt.keyToString(event.key).toLowerCase().replace(/\s+/g, ".")
        if (event.key === Qt.Key_Shift)
            keyName = "left.shift"
        else if (event.key === Qt.Key_Control)
            keyName = "left.control"
        else if (event.key === Qt.Key_Alt)
            keyName = "left.alt"
        else if (event.key === Qt.Key_Meta)
            keyName = "left.meta"
        if (keyName.length === 0)
            return
        const updated = Object.assign({}, controlDraft)
        updated[capturingControlKey] = "key.keyboard." + keyName
        controlDraft = updated
        capturingControlKey = ""
        event.accepted = true
    }

    Keys.onPressed: event => bindControlKey(event)

    function staggerPageItems(page) {
        if (!page)
            return
        const pageItems = []
        collectPageItems(page, pageItems)
        let delayIndex = 0
        for (let index = 0; index < pageItems.length; ++index) {
            const item = pageItems[index]
            if (Motion.reduceMotion) {
                item.opacity = 1
                continue
            }
            item.opacity = 0
            const animation = staggeredFadeComponent.createObject(root, {
                "targetItem": item,
                "startDelay": delayIndex * 38,
                "fadeDuration": Motion.normalDuration
            })
            if (!animation)
                throw new Error("Could not create staggered page animation.")
            animation.start()
            ++delayIndex
        }
    }

    property Component homePageComponent: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 34
        spacing: 18

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 18
            radius: Theme.radiusHero
            color: ThemeSettings.theme === "dark" ? "#991e293b" : "#ccffffff"
            border.color: ThemeSettings.theme === "dark" ? "#334e75" : "#cbd5e1"

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 48, 720)
                spacing: 18
                Label {
                    text: "Stratalauncher"
                    font.family: "Outfit"
                    font.pixelSize: 38
                    color: ThemeSettings.theme === "dark" ? "#f8fafc" : "#0f172a"
                    Layout.alignment: Qt.AlignHCenter
                }
                Label {
                    text: "Launch, manage, and organize your Minecraft instances from a single shell."
                    font.family: "DM Sans"
                    font.pixelSize: 16
                    color: ThemeSettings.theme === "dark" ? "#cbd5e1" : "#475569"
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: 620
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8
                    Repeater {
                        model: LauncherController.recentLaunchedInstances
                        delegate: Rectangle {
                            required property var modelData
                            Layout.alignment: Qt.AlignHCenter
                            Layout.fillWidth: true
                            Layout.maximumWidth: 560
                            implicitHeight: 62
                            radius: Theme.radiusMedium
                            color: ThemeSettings.theme === "dark" ? "#7f172437" : "#a6ffffff"
                            border.color: ThemeSettings.theme === "dark" ? Theme.borderDark : Theme.borderLight
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 12
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    font.family: "Outfit"
                                    font.pixelSize: 18
                                    color: Theme.textPrimary
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                StrataButton {
                                    text: "Launch"
                                    iconName: "start"
                                    onClicked: {
                                        LauncherController.openConsole(modelData.id)
                                        LauncherController.launchInstance(modelData.id)
                                    }
                                }
                            }
                        }
                    }
                }
                StrataButton {
                    text: "Create instance"
                    iconName: "add"
                    Accessible.name: "Create a Minecraft instance"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: {
                        root.selectPage("Instances")
                        root.createDialogVisible = true
                    }
                }
            }
        }
        Text {
            Layout.fillWidth: true
            text: 'Stratalauncher is completely open source and free (as in free beer and free speech); visit the <a href="https://github.com/springknight3/stratalauncher">source page</a> or <a href="https://strata-web-alpha.vercel.app">our website</a>.'
            textFormat: Text.RichText
            color: ThemeSettings.theme === "dark" ? "#93c5fd" : "#1d4ed8"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            onLinkActivated: link => root.openExternalLink(link)
        }
    }
    property Component aboutPageComponent: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 34
        spacing: 18

        Label {
            text: "About StrataLauncher"
            font.family: "Outfit"
            font.pixelSize: 34
            color: Theme.textPrimary
        }
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.radiusMedium
            color: ThemeSettings.theme === "dark" ? "#991e293b" : "#ccffffff"
            border.color: ThemeSettings.theme === "dark" ? "#233349" : "#dfe5f3"
            implicitHeight: aboutWarning.implicitHeight + 32
            Text {
                id: aboutWarning
                objectName: "aboutWarning"
                anchors.fill: parent
                anchors.margins: 16
                text: "This is a hobby project and not meant to replace anything! May contain security issues and flaws! Not affiliated with Mojang Studios, Microsoft, or any other official platform partner."
                wrapMode: Text.WordWrap
                font.family: "DM Sans"
                font.pixelSize: 15
                color: Theme.textPrimary
            }
        }
        Label {
            text: "Minecraft is a trademark of Mojang Studios."
            font.family: "DM Sans"
            font.pixelSize: 13
            color: Theme.textSecondary
        }
        Label {
            text: "Version " + AppVersion
            font.family: "DM Sans"
            font.pixelSize: 13
            color: Theme.textSecondary
        }
        Item { Layout.fillHeight: true }
    }
    property Component serversPageComponent: ColumnLayout {
        objectName: "serversPageRoot"
        anchors.fill: parent
        anchors.margins: 22
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Servers"
                font.family: "Outfit"
                font.pixelSize: 34
                color: Theme.textPrimary
            }
            Item { Layout.fillWidth: true }
            StrataButton {
                text: "Refresh"
                iconName: "refresh"
                enabled: !LauncherController.serverBusy
                onClicked: LauncherController.refreshServers()
            }
            StrataButton {
                objectName: "serverCreateButton"
                text: "Create server"
                iconName: "add"
                enabled: !LauncherController.serverBusy
                onClicked: serverCreatePopup.open()
            }
        }

        Popup {
            id: serverTerminatePopup
            parent: Overlay.overlay
            anchors.centerIn: parent
            width: Math.min(root.width - 48, 480)
            modal: true
            dim: true
            focus: true
            closePolicy: Popup.NoAutoClose
            background: Rectangle {
                radius: Theme.radiusLarge
                color: ThemeSettings.theme === "dark" ? "#101927" : "#ffffff"
                border.color: ThemeSettings.theme === "dark" ? "#293d58" : "#dce4ef"
            }
            contentItem: ColumnLayout {
                spacing: 12
                Label {
                    text: "Terminate the server immediately? Unsaved world data may be lost."
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    color: Theme.textPrimary
                }

                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    StrataButton {
                        text: "Cancel"
                        iconName: "close"
                        onClicked: serverTerminatePopup.close()
                    }
                    StrataButton {
                        text: "Terminate"
                        iconName: "terminate"
                        dangerous: true
                        onClicked: {
                            LauncherController.terminateServer(root.selectedServerId)
                            serverTerminatePopup.close()
                        }
                    }

                }
            }
        }

        Popup {
            id: serverEulaPopup
            parent: Overlay.overlay
            anchors.centerIn: parent
            width: Math.min(root.width - 48, 540)
            modal: true
            dim: true
            focus: true
            closePolicy: Popup.NoAutoClose
            background: Rectangle {
                radius: Theme.radiusLarge
                color: ThemeSettings.theme === "dark" ? "#101927" : "#ffffff"
                border.color: ThemeSettings.theme === "dark" ? "#293d58" : "#dce4ef"
            }
            contentItem: ColumnLayout {
                spacing: 14
                Label {
                    text: "You must agree to start this server"
                    font.family: "Outfit"
                    font.pixelSize: 22
                    color: Theme.textPrimary
                    Layout.fillWidth: true
                }
                Text {
                    text: "To start this server, you must agree to Mojang's End User License Agreement ( <a href=\"https://www.minecraft.net/en-us/eula\">here</a> )."
                    textFormat: Text.RichText
                    wrapMode: Text.WordWrap
                    color: Theme.textSecondary
                    Layout.fillWidth: true
                    onLinkActivated: link => root.openExternalLink(link)
                }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    StrataButton {
                        text: "Cancel"
                        iconName: "close"
                        onClicked: serverEulaPopup.close()
                    }
                    StrataButton {
                        text: "Accept"
                        iconName: "secure"
                        onClicked: {
                            root.pendingEulaStartServerId = root.selectedServerId
                            LauncherController.setServerEulaAccepted(root.selectedServerId, true)
                            serverEulaPopup.close()
                        }
                    }
                }
            }
        }

        Label {
            text: LauncherController.serverBusy
                  ? LauncherController.serverStatus : "Create and configure isolated Minecraft servers."
            color: Theme.textSecondary
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: root.selectedServer !== null ? 14 : 0

            Rectangle {
                id: serverListPanel
                Layout.preferredWidth: root.selectedServer !== null ? 300 : -1
                Layout.fillWidth: root.selectedServer === null
                Layout.fillHeight: true
                radius: Theme.radiusMedium
                color: ThemeSettings.theme === "dark" ? "#991e293b" : "#ccffffff"
                border.color: ThemeSettings.theme === "dark" ? "#233349" : "#dfe5f3"

                ListView {
                    id: devServerList
                    objectName: "devServerList"
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true
                    spacing: 8
                    model: LauncherController.servers
                    TapHandler {
                        onTapped: {
                            if (devServerList.indexAt(point.position.x, point.position.y) < 0) {
                                root.selectedServerId = ""
                                LauncherController.selectServer("")
                            }
                        }
                    }
                    delegate: Rectangle {
                        required property var modelData
                        property bool hovered: false
                        width: devServerList.width
                        height: 74
                        radius: Theme.radiusSmall
                        color: modelData.id === root.selectedServerId
                               ? (ThemeSettings.theme === "dark" ? "#1b3048" : "#e7efff")
                               : (ThemeSettings.theme === "dark" ? "#991e293b" : "#ccffffff")
                        border.color: ThemeSettings.theme === "dark" ? "#263a54" : "#e2e8f0"
                        scale: hovered ? 1 : 0.99
                        HoverHandler { onHoveredChanged: parent.hovered = hovered }
                        Behavior on scale {
                            NumberAnimation {
                                duration: Motion.fastDuration
                                easing.type: Motion.standardEasing
                                easing.bezierCurve: Motion.standardCurve
                            }
                        }
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 8
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    font.family: "Outfit"
                                    font.pixelSize: 15
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.distribution + " · " + modelData.version
                                          + " · " + (modelData.running
                                                     ? modelData.processState
                                                     : modelData.installed ? "Installed" : "Not installed")
                                    font.pixelSize: 12
                                    color: Theme.textSecondary
                                    elide: Text.ElideRight
                                }
                            }
                        }
                        TapHandler {
                            enabled: !LauncherController.serverContentBusy
                            onTapped: {
                                root.selectedServerId = modelData.id
                                root.pendingEulaStartServerId = ""
                                root.serverTabIndex = 0
                                LauncherController.selectServer(modelData.id)
                                serverNameField.text = modelData.name
                                serverOnlineModeCheck.checked = modelData.onlineMode
                                serverManagedJavaCheck.checked = modelData.useManagedJavaRuntime
                                serverViewDistance.value = modelData.viewDistance
                                serverJavaFlags.text = modelData.javaFlags.join("\n")
                                serverPropertiesEditor.text = ""
                                LauncherController.loadServerProperties(modelData.id)
                            }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: LauncherController.servers.length === 0
                        text: LauncherController.serverBusy ? "Loading servers…" : "No servers yet."
                        color: Theme.textSecondary
                    }
                }
            }

            Rectangle {
                id: serverDetailPanel
                Layout.minimumWidth: 0
                Layout.preferredWidth: root.serverDetailWidth
                Layout.maximumWidth: 1200
                Layout.fillWidth: root.selectedServer !== null
                Layout.fillHeight: true
                radius: Theme.radiusMedium
                clip: true
                color: ThemeSettings.theme === "dark" ? "#991e293b" : "#ccffffff"
                border.color: ThemeSettings.theme === "dark" ? "#233349" : "#dfe5f3"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10
                    visible: root.selectedServer !== null
                    TabBar {
                        id: serverTabs
                        Layout.fillWidth: true
                        padding: 8
                        background: Rectangle { color: "transparent" }
                        currentIndex: root.serverTabIndex
                        StrataTabButton { text: "Overview"; iconName: "server" }
                        StrataTabButton { text: "Console"; iconName: "console" }
                        StrataTabButton {
                            text: root.selectedServer
                                  && root.selectedServer.distribution === "FABRIC"
                                  ? "Mods" : "Plugins"
                            iconName: "download"
                            enabled: root.selectedServer
                                     && (root.selectedServer.distribution === "PAPER"
                                         || root.selectedServer.distribution === "FABRIC")
                        }
                        StrataTabButton {
                            text: "Dangerous"
                            iconName: "dangerous"
                            dangerous: true
                        }
                        onCurrentIndexChanged: {
                            root.serverTabIndex = currentIndex
                            if (currentIndex === 2 && root.selectedServer) {
                                root.serverContentProjectId = ""
                                LauncherController.searchServerContent(root.selectedServerId, "")
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: root.serverTabIndex === 0
                        StrataTextField {
                            id: serverNameField
                            Layout.fillWidth: true
                            placeholderText: "Server name"
                            text: root.selectedServer ? root.selectedServer.name : ""
                        }
                        Label {
                            text: root.selectedServer
                                  ? (root.selectedServer.installed ? "Installed" : "Not installed")
                                  : ""
                            color: Theme.textSecondary
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: root.serverTabIndex === 0
                        StrataCheckBox {
                            id: serverOnlineModeCheck
                            text: "Online mode"
                            checked: root.selectedServer ? root.selectedServer.onlineMode : true
                        }
                        StrataCheckBox {
                            id: serverManagedJavaCheck
                            objectName: "serverManagedJavaCheck"
                            text: "Managed Java"
                            checked: root.selectedServer
                                     ? root.selectedServer.useManagedJavaRuntime : false
                        }
                        Label { text: "View distance" }
                        StrataSpinBox {
                            id: serverViewDistance
                            from: 2
                            to: 32
                            value: root.selectedServer ? root.selectedServer.viewDistance : 10
                        }
                    }

                    Label {
                        visible: root.serverTabIndex === 0
                        text: "Java flags (one argument per line)"
                        color: ThemeSettings.theme === "dark" ? "#dfe7f1" : "#475569"
                    }
                    StrataTextArea {
                        id: serverJavaFlags
                        visible: root.serverTabIndex === 0
                        Layout.fillWidth: true
                        Layout.preferredHeight: 66
                        text: root.selectedServer ? root.selectedServer.javaFlags.join("\n") : ""
                        placeholderText: "-Xmx4G"
                        wrapMode: TextEdit.NoWrap
                        font.family: "JetBrains Mono"
                    }

                    RowLayout {
                        visible: root.serverTabIndex === 0
                        Layout.fillWidth: true
                        Label {
                            text: "server.properties"
                            font.family: "Outfit"
                            font.pixelSize: 18
                            color: Theme.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        StrataButton {
                            text: "Save settings"
                            iconName: "save"
                            enabled: !LauncherController.serverBusy
                                     && !(root.selectedServer && root.selectedServer.running)
                            onClicked: LauncherController.updateServerSettings(
                                           root.selectedServerId, serverNameField.text,
                                           root.selectedServer.eulaAccepted, serverJavaFlags.text,
                                           serverViewDistance.value, serverOnlineModeCheck.checked,
                                           serverManagedJavaCheck.checked)
                        }
                        StrataButton {
                            text: "Save properties"
                            iconName: "save"
                            enabled: !LauncherController.serverBusy
                                     && !(root.selectedServer && root.selectedServer.running)
                            onClicked: LauncherController.saveServerProperties(
                                           root.selectedServerId, serverPropertiesEditor.text)
                        }
                    }

                    ProgressBar {
                        visible: root.serverTabIndex === 0 && LauncherController.serverBusy
                        Layout.fillWidth: true
                        value: LauncherController.serverProgress
                    }
                    Label {
                        visible: root.serverTabIndex === 0 && LauncherController.serverBusy
                        Layout.fillWidth: true
                        text: LauncherController.serverInstallItem
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }
                    Label {
                        visible: root.serverTabIndex === 1
                        Layout.fillWidth: true
                        text: LauncherController.serverResourceStatus
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        visible: root.serverTabIndex === 1
                        Layout.fillWidth: true
                        StatusLight {
                            status: LauncherController.serverProcessState === "crashed"
                                    ? "crashed"
                                    : LauncherController.serverProcessState === "starting"
                                      ? "starting"
                                      : root.selectedServer && root.selectedServer.running
                                        ? "ready" : "stopped"
                            size: 12
                        }
                        Label {
                            text: root.selectedServer && root.selectedServer.running
                                  ? LauncherController.serverProcessState
                                  : LauncherController.serverProcessState === "crashed"
                                    ? "Terminated" : "Stopped"
                            color: Theme.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        StrataButton {
                            objectName: "startServerButton"
                            text: ""
                            iconName: "start"
                            Accessible.name: "Start server"
                            ToolTip.visible: hovered
                            ToolTip.text: "Start server"
                            enabled: root.selectedServer !== null
                                     && root.selectedServer.installed
                                     && !root.selectedServer.running
                                     && !LauncherController.serverBusy
                            onClicked: {
                                if (root.selectedServer.eulaAccepted)
                                    LauncherController.startServer(root.selectedServerId)
                                else
                                    serverEulaPopup.open()
                            }
                        }
                        StrataButton {
                            text: ""
                            iconName: "stop"
                            warning: true
                            Accessible.name: "Stop server"
                            ToolTip.visible: hovered
                            ToolTip.text: "Stop server"
                            enabled: root.selectedServer !== null
                                     && root.selectedServer.running
                                     && !LauncherController.serverBusy
                            onClicked: LauncherController.stopServer(root.selectedServerId)
                        }
                        StrataButton {
                            text: ""
                            iconName: "terminate"
                            dangerous: true
                            Accessible.name: "Terminate server immediately"
                            ToolTip.visible: hovered
                            ToolTip.text: "Terminate"
                            enabled: root.selectedServer !== null
                                     && root.selectedServer.running
                                     && !LauncherController.serverBusy
                            onClicked: serverTerminatePopup.open()
                        }
                    }
                    ScrollView {
                        id: serverConsoleScrollView
                        visible: root.serverTabIndex === 1
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        StrataTextArea {
                            id: serverConsoleEditor
                            objectName: "serverConsoleEditor"
                            width: serverConsoleScrollView.availableWidth
                            text: LauncherController.serverConsoleText
                            readOnly: true
                            wrapMode: TextEdit.NoWrap
                            font.family: "JetBrains Mono"
                            font.pixelSize: 11
                            selectByMouse: true
                            onTextChanged: cursorPosition = length
                        }
                    }
                    RowLayout {
                        visible: root.serverTabIndex === 1
                        Layout.fillWidth: true
                        StrataTextField {
                            id: serverCommandField
                            objectName: "serverCommandField"
                            Layout.fillWidth: true
                            enabled: root.selectedServer !== null
                                     && root.selectedServer.running
                            placeholderText: "Enter a server command"
                            onAccepted: {
                                LauncherController.sendServerCommand(root.selectedServerId, text)
                                clear()
                            }
                        }
                        StrataButton {
                            text: "Send"
                            enabled: root.selectedServer !== null && root.selectedServer.running
                                     && serverCommandField.text.trim().length > 0
                            onClicked: {
                                LauncherController.sendServerCommand(root.selectedServerId,
                                                                     serverCommandField.text)
                                serverCommandField.clear()
                            }
                        }
                    }

                    Label {
                        visible: root.serverTabIndex === 0
                        Layout.fillWidth: true
                        text: "server.properties (saved changes apply next time the server starts)"
                        color: ThemeSettings.theme === "dark" ? "#dfe7f1" : "#475569"
                    }
                    StrataTextArea {
                        id: serverPropertiesEditor
                        visible: root.serverTabIndex === 0
                        objectName: "serverPropertiesEditor"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: LauncherController.serverProperties
                        placeholderText: "Properties load when a server is selected."
                        wrapMode: TextEdit.NoWrap
                        font.family: "JetBrains Mono"
                        font.pixelSize: 12
                        selectByMouse: true
                    }
                    ColumnLayout {
                        visible: root.serverTabIndex === 2
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Label {
                            text: root.selectedServer
                                  && root.selectedServer.distribution === "PAPER"
                                  ? "Modrinth server plugins"
                                  : "Modrinth server mods"
                            font.family: "Outfit"
                            font.pixelSize: 20
                            color: Theme.textPrimary
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            StrataTextField {
                                id: serverContentSearch
                                Layout.fillWidth: true
                                enabled: !LauncherController.serverContentBusy
                                placeholderText: "Search Modrinth"
                                onAccepted: LauncherController.searchServerContent(
                                                root.selectedServerId, text)
                            }
                            StrataButton {
                                text: "Search"
                                iconName: "download"
                                enabled: root.selectedServer !== null
                                         && !LauncherController.serverContentBusy
                                onClicked: LauncherController.searchServerContent(
                                               root.selectedServerId, serverContentSearch.text)
                            }
                        }
                        ListView {
                            id: serverContentProjectList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 6
                            model: LauncherController.serverContentProjects
                            delegate: Rectangle {
                                required property var modelData
                                width: serverContentProjectList.width
                                height: 66
                                radius: Theme.radiusSmall
                                color: root.serverContentProjectId === modelData.id
                                       ? (ThemeSettings.theme === "dark" ? "#243852" : "#e4edfa")
                                       : (ThemeSettings.theme === "dark" ? "#172437" : "#f4f7fc")
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            Layout.fillWidth: true
                                            text: modelData.title
                                            font.bold: true
                                            color: Theme.textPrimary
                                            elide: Text.ElideRight
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: modelData.description
                                            color: Theme.textSecondary
                                            elide: Text.ElideRight
                                        }
                                    }
                                    Label {
                                        text: Number(modelData.downloads).toLocaleString()
                                        color: Theme.textSecondary
                                    }
                                }
                                TapHandler {
                                    onTapped: {
                                        root.serverContentProjectId = modelData.id
                                        LauncherController.loadServerContentVersions(
                                                    root.selectedServerId, modelData.id)
                                    }
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            StrataComboBox {
                                id: serverContentVersionBox
                                Layout.fillWidth: true
                                model: LauncherController.serverContentVersions
                                textRole: "versionNumber"
                                valueRole: "id"
                                enabled: !LauncherController.serverContentBusy
                                         && model.length > 0
                                currentIndex: model.length > 0 ? 0 : -1
                            }
                            StrataButton {
                                text: "Install"
                                iconName: "download"
                                enabled: root.serverContentProjectId.length > 0
                                         && serverContentVersionBox.currentIndex >= 0
                                         && !LauncherController.serverContentBusy
                                onClicked: LauncherController.installServerContent(
                                               root.selectedServerId,
                                               root.serverContentProjectId,
                                               serverContentVersionBox.currentValue)
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: LauncherController.serverContentStatus
                            wrapMode: Text.WordWrap
                            color: Theme.textSecondary
                        }
                    }
                }
                ColumnLayout {
                    visible: root.serverTabIndex === 3
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Label {
                        text: "Dangerous server actions"
                        font.family: "Outfit"
                        font.pixelSize: 20
                        color: "#ef4444"
                    }
                    Label {
                        text: "Repair replaces the server JAR. Deleting permanently removes this server and its data."
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.textSecondary
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        StrataButton {
                            objectName: "installServerButton"
                            text: "Repair server"
                            iconName: "refresh"
                            dangerous: true
                            enabled: root.selectedServer !== null
                                     && !LauncherController.serverBusy
                                     && !root.selectedServer.running
                            onClicked: LauncherController.installServer(root.selectedServerId)
                        }
                        StrataButton {
                            text: "Install managed Java"
                            iconName: "runtime"
                            enabled: root.selectedServer !== null
                                     && !LauncherController.serverBusy
                                     && !root.selectedServer.running
                            onClicked: LauncherController.installServerRuntime(
                                           root.selectedServerId)
                        }
                        StrataButton {
                            text: "Delete server"
                            iconName: "delete"
                            dangerous: true
                            enabled: root.selectedServer !== null
                                     && !LauncherController.serverBusy
                                     && !root.selectedServer.running
                            onClicked: {
                                root.pendingServerDeletionId = root.selectedServerId
                                serverDeleteConfirmation.clear()
                            }
                        }
                    }
                    ProgressBar {
                        visible: LauncherController.serverBusy
                        Layout.fillWidth: true
                        value: LauncherController.serverProgress
                    }
                    Label {
                        visible: LauncherController.serverBusy
                        Layout.fillWidth: true
                        text: LauncherController.serverInstallItem
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }
                    Item { Layout.fillHeight: true }
                }

                Label {
                    anchors.centerIn: parent
                    visible: root.selectedServer === null
                    text: "Select a server to edit its settings."
                    color: Theme.textSecondary
                }
            }
        }

        Connections {
            target: LauncherController
            function onServersChanged() {
                if (root.pendingEulaStartServerId.length > 0
                        && !LauncherController.serverBusy
                        && root.selectedServer
                        && root.selectedServer.id === root.pendingEulaStartServerId
                        && root.selectedServer.eulaAccepted) {
                    const serverId = root.pendingEulaStartServerId
                    root.pendingEulaStartServerId = ""
                    LauncherController.startServer(serverId)
                }
                if (!LauncherController.serverBusy
                        && LauncherController.serverStatus === "Ready")
                    serverPropertiesEditor.text = LauncherController.serverProperties
            }
        }
    }

    Connections {
        target: ThemeSettings
        function onSettingsError(message) {
            root.showToast(message)
        }
    }

    function selectPage(page) {
        if (page === "Settings") {
            openLauncherSettings()
            return
        }
        if (root.selectedPage === page)
            return
        root.selectedPage = page
        if (page === "Instances") {
            runtimeManagerVisible = false
            selectedInstanceIndex = Math.max(0, selectedInstanceIndex)
        } else {
            runtimeManagerVisible = false
        }
        if (page === "Servers")
            LauncherController.refreshServers()
        Qt.callLater(() => {
            let pageItem = null
            if (page === "Home")
                pageItem = homePageLoader.item
            else if (page === "About")
                pageItem = aboutPageLoader.item
            else if (page === "Content")
                pageItem = contentPageLoader.item
            else if (page === "Servers")
                pageItem = serversPageLoader.item
            else if (page === "Instances")
                pageItem = instancePage
            root.staggerPageItems(pageItem)
        })
    }

    function showToast(message) {
        toastText.text = message
        toastVisible = true
        toastTimer.restart()
    }

    function openExternalLink(url) {
        const opened = Qt.openUrlExternally(url)
        root.showToast(opened ? "Opened link in your browser." : "Couldn't open the link.")
    }

    function finishStartupIfReady() {
        if (startupMinimumElapsed && startupInitializationStarted
                && !LauncherController.initializing) {
            startupVisible = false
        }
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

    function requestAccountRemoval(accountId) {
        if (!accountId) {
            showToast("Unable to identify the selected account")
            return
        }
        pendingRemoveAccountId = accountId
        accountDeleteConfirmation.clear()
    }

    ListModel {
        id: instanceModel
    }

    Connections {
        target: LauncherController
        function onInitializationChanged() {
            root.finishStartupIfReady()
        }
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
        function onAccountsChanged() {
            accountClientIdInput.text = LauncherController.microsoftClientId
            launcherClientIdInput.text = LauncherController.microsoftClientId
        }
        function onGlobalJavaArgumentsChanged() {
            globalJavaFlagsField.text = LauncherController.globalJavaArguments.join("\n")
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

    Shortcut {
        sequence: "F9"
        enabled: !AppDebugFrameTimeForced
        onActivated: {
            root.debugFrameTime = !root.debugFrameTime
            ThemeSettings.frameTimeOverlayEnabled = root.debugFrameTime
        }
    }

    Shortcut {
        sequence: "Ctrl+F9"
        enabled: root.debugFrameTime
        onActivated: overlay.clearSamples()
    }

    Component.onCompleted: startupEntranceTimer.start()

    Component {
        id: staggeredFadeComponent
        StaggeredFade {}
    }

    Timer {
        id: startupEntranceTimer
        interval: 0
        repeat: false
        onTriggered: {
            if (!startupVisible)
                return
            startupEntranceComplete = true
            startupInitializationStarted = true
            LauncherController.refresh()
            startupMinimumTimer.start()
        }
    }

    Timer {
        id: startupMinimumTimer
        interval: 3000
        repeat: false
        onTriggered: {
            startupMinimumElapsed = true
            root.finishStartupIfReady()
        }
    }

    Timer {
        id: toastTimer
        interval: 2200
        repeat: false
        onTriggered: toastVisible = false
    }

    Timer {
        id: installedBulkTimer
        interval: 100
        repeat: true
        onTriggered: {
            if (contentController.busy)
                return
            if (root.installedBulkQueue.length === 0) {
                stop()
                return
            }
            const queue = root.installedBulkQueue.slice()
            const index = queue.shift()
            root.installedBulkQueue = queue
            if (root.installedBulkAction === "delete")
                contentController.removeInstalled(index)
            else
                contentController.setInstalledEnabled(index,
                                                       root.installedBulkAction === "enable")
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.backgroundStart }
            GradientStop { position: 1; color: Theme.backgroundEnd }
        }
    }

    Rectangle {
        id: startupOverlay
        objectName: "startupOverlay"
        anchors.fill: parent
        visible: startupVisible || opacity > 0
        opacity: startupVisible ? 1 : 0
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.startupStart }
            GradientStop { position: 1; color: Theme.startupEnd }
        }
        z: 50

        Behavior on opacity {
            NumberAnimation {
                duration: Motion.normalDuration
                easing.type: Motion.standardEasing
                easing.bezierCurve: Motion.expressCurve
            }
        }

        Column {
            id: startupContent
            objectName: "startupContent"
            anchors.centerIn: startupOverlay
            spacing: 8
            width: Math.min(startupOverlay.width * 0.75, 560)
            scale: startupEntranceComplete ? 1 : 0.97
            transformOrigin: Item.Center
            Behavior on scale {
                NumberAnimation {
                    duration: Motion.slowDuration
                    easing.type: Motion.standardEasing
                    easing.bezierCurve: Motion.emphasisCurve
                }
            }

            Label {
                text: "STRATA"
                width: parent.width
                font.family: "Outfit"
                font.pixelSize: 58
                font.weight: Font.Black
                color: "#ffffff"
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                text: "MINECRAFT LAUNCHER"
                width: parent.width
                font.family: "Outfit"
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: "#ffffff"
                horizontalAlignment: Text.AlignHCenter
            }

            Item {
                width: 1
                height: 16
            }

            Text {
                text: "This is a hobby project and not meant to replace anything! May contain security issues and flaws! Not affiliated with Mojang Studios, Microsoft, or any other official platform partner."
                width: parent.width
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                font.family: "DM Sans"
                font.pixelSize: 14
                color: "#fef3c7"
            }

            Text {
                text: "Minecraft is a trademark of Mojang Studios."
                width: parent.width
                font.family: "DM Sans"
                font.pixelSize: 12
                opacity: 0.78
                color: "#ffffff"
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Rectangle {
        id: shell
        objectName: "shell"
        anchors.fill: parent
        color: "transparent"
        z: 1
        enabled: !startupVisible && startupOverlay.opacity <= 0

        RowLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 18

            Rectangle {
                id: sidebar
                objectName: "sidebar"
                Layout.minimumWidth: 0
                Layout.maximumWidth: 250
                Layout.preferredWidth: root.sidebarWidth
                Layout.fillHeight: true
                clip: true
                radius: Theme.radiusLarge
                color: ThemeSettings.theme === "dark" ? "#e6111827" : "#e6ffffff"
                border.color: ThemeSettings.theme === "dark" ? "#1f2d3d" : "#e3e8f0"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            visible: !root.sidebarCollapsed
                            text: "StrataLauncher"
                            font.family: "Outfit"
                            font.pixelSize: 24
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                        }
                        StrataButton {
                            objectName: "sidebarToggleButton"
                            text: ""
                            implicitWidth: root.sidebarCollapsed ? 32 : 34
                            leftPadding: root.sidebarCollapsed ? 5 : 10
                            rightPadding: root.sidebarCollapsed ? 5 : 10
                            Accessible.name: root.sidebarCollapsed ? "Expand sidebar" : "Collapse sidebar"
                            contentItem: ThemeAwareIcon {
                                iconName: root.sidebarCollapsed ? "menu" : "close"
                                size: 18
                            }
                            onClicked: {
                                root.sidebarCollapsed = !root.sidebarCollapsed
                                root.sidebarWidth = root.sidebarCollapsed ? 64 : 250
                            }
                        }
                    }

                    Repeater {
                        model: ["Home", "Instances", "Content", "Servers", "Settings", "About"]
                        delegate: StrataButton {
                            property bool selected: modelData === root.selectedPage
                            property bool navHovered: false
                            objectName: modelData === "Servers" ? "serversNavigationButton" : ""
                            Accessible.name: modelData + " page"
                            Layout.fillWidth: true
                            height: 42
                            text: modelData
                            flat: true
                            leftPadding: root.sidebarCollapsed ? 5 : 10
                            rightPadding: root.sidebarCollapsed ? 5 : 10
                            scale: down ? 0.97 : navHovered ? 1.02 : 1
                            HoverHandler {
                                onHoveredChanged: parent.navHovered = hovered
                            }
                            Behavior on scale {
                                NumberAnimation {
                                    duration: Motion.fastDuration
                                    easing.type: Motion.standardEasing
                                    easing.bezierCurve: Motion.standardCurve
                                }
                            }
                            onClicked: {
                                if (modelData === "Settings")
                                    root.openLauncherSettings()
                                else
                                    root.selectPage(modelData)
                            }
                            contentItem: Row {
                                spacing: 10
                                ThemeAwareIcon {
                                    iconName: modelData === "Content" ? "download"
                                              : modelData === "Servers" ? "server"
                                              : modelData === "About" ? "options"
                                              : modelData.toLowerCase()
                                    size: 18
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Label {
                                    visible: !root.sidebarCollapsed
                                    text: parent.parent.text
                                    font.family: "DM Sans"
                                    font.pixelSize: 15
                                    color: parent.parent.selected ? (ThemeSettings.theme === "dark" ? "#9ed8ff" : "#0f4fd8") : (ThemeSettings.theme === "dark" ? "#dfe7f1" : "#475569")
                                }
                            }
                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: selected ? (ThemeSettings.theme === "dark" ? "#162638" : "#eaf2ff") : "transparent"
                                Behavior on color {
                                    ColorAnimation {
                                        duration: Motion.fastDuration
                                        easing.type: Motion.standardEasing
                                        easing.bezierCurve: Motion.standardCurve
                                    }
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                }
            }

            Rectangle {
                id: mainPanel
                objectName: "mainPanel"
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusLarge
                color: ThemeSettings.theme === "dark" ? "#99111827" : "#d9ffffff"
                border.color: ThemeSettings.theme === "dark" ? "#1d2b3e" : "#e4e8f0"

                ColumnLayout {
                    id: instancePage
                    objectName: "instancePage"
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 18
                    visible: root.selectedPage === "Instances"

                    RowLayout {
                        spacing: 12
                        Layout.fillWidth: true

                        Label {
                            text: "Instances"
                            font.family: "Outfit"
                            font.pixelSize: 34
                            color: Theme.textPrimary
                        }

                        Item { Layout.fillWidth: true }

                        StrataButton {
                            text: "Create"
                            iconName: "add"
                            onClicked: createDialogVisible = true
                            background: Rectangle { radius: Theme.radiusSmall; color: ThemeSettings.theme === "dark" ? "#1d2b40" : "#e7efff" }
                        }

                        StrataButton {
                            text: "Duplicate"
                            iconName: "duplicate"
                            onClicked: showToast("Duplicate instance")
                            background: Rectangle { radius: Theme.radiusSmall; color: ThemeSettings.theme === "dark" ? "#182335" : "#f3f6fb" }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        StrataButton {
                            objectName: "accountButton"
                            iconName: "account-add"
                            text: LauncherController.activeAccountName.length > 0
                                  ? "Account: " + LauncherController.activeAccountName
                                  : "Select account"
                            onClicked: root.accountFlyoutVisible = !root.accountFlyoutVisible
                        }

                        Item { Layout.fillWidth: true }
                    }

                    Rectangle {
                        visible: createDialogVisible && !LauncherController.installing
                        Layout.fillWidth: true
                        height: 360
                        radius: Theme.radiusMedium
                        color: ThemeSettings.theme === "dark" ? "#991e293b" : "#ccffffff"
                        border.color: ThemeSettings.theme === "dark" ? "#233349" : "#dfe5f3"

                        ColumnLayout {
                            id: createDialogContent
                            anchors.centerIn: parent
                            width: parent.width - 36
                            spacing: 10

                            Label {
                                text: "Create instance"
                                font.family: "Outfit"
                                font.pixelSize: 22
                                color: Theme.textPrimary
                            }

                            StrataTextField {
                                id: instanceNameInput
                                Layout.fillWidth: true
                                placeholderText: "Instance name"
                                text: LauncherController.latestRelease.length > 0
                                      ? "Minecraft " + LauncherController.latestRelease : ""
                            }

                            StrataComboBox {
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
                                onCurrentValueChanged: {
                                    if (loaderTypeBox.currentValue
                                            && loaderTypeBox.currentValue !== "VANILLA"
                                            && currentValue) {
                                        LauncherController.loadLoaderVersions(
                                                    loaderTypeBox.currentValue, currentValue)
                                    }
                                }
                            }

                            property Component contentPageComponent: ColumnLayout {
                                id: contentPage
                                anchors.fill: parent
                                anchors.margins: 22
                                spacing: 14
                                visible: root.selectedPage === "Content"

                                        Label {
                                        text: "Content"
                                        font.family: "Outfit"
                                        font.pixelSize: 34
                                        color: Theme.textPrimary
                                    }

                                    Label {
                                        text: "Browse Modrinth and manage compatible content for an instance."
                                        font.family: "DM Sans"
                                        font.pixelSize: 14
                                        color: Theme.textSecondary
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 10

                                        StrataComboBox {
                                            id: contentInstanceBox
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: 260
                                            model: LauncherController.instances
                                            textRole: "name"
                                            valueRole: "id"
                                            onCurrentValueChanged: {
                                                root.selectedContentIndices = []
                                                root.lastContentSelection = -1
                                                if (currentValue && contentTypeBox.currentValue)
                                                    contentController.selectTarget(currentValue,
                                                                                   contentTypeBox.currentValue)
                                                }
                                        }

                                        StrataComboBox {
                                            id: contentTypeBox
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: 180
                                            model: [
                                                { "label": "Mods", "id": "MOD" },
                                                { "label": "Resource packs", "id": "RESOURCE_PACK" },
                                                { "label": "Data packs", "id": "DATA_PACK" },
                                                { "label": "Shader packs", "id": "SHADER" }
                                            ]
                                            textRole: "label"
                                            valueRole: "id"
                                            onCurrentValueChanged: {
                                                root.selectedContentIndices = []
                                                root.lastContentSelection = -1
                                                if (contentInstanceBox.currentValue && currentValue)
                                                    contentController.selectTarget(contentInstanceBox.currentValue,
                                                                                   currentValue)
                                            }
                                        }

                                        StrataComboBox {
                                            id: contentWorldBox
                                            visible: contentTypeBox.currentValue === "DATA_PACK"
                                            enabled: contentController.worlds.length > 0
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: 180
                                            model: contentController.worlds
                                            displayText: currentIndex >= 0 ? currentText : "Select world"
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        StrataTextField {
                                            id: contentSearchField
                                            Layout.fillWidth: true
                                            placeholderText: "Search Modrinth projects"
                                            enabled: contentInstanceBox.currentIndex >= 0
                                                     && !contentController.loadingTarget
                                            onAccepted: contentController.search(text, 0)
                                        }
                                        StrataButton {
                                            text: "Search"
                                            enabled: contentInstanceBox.currentIndex >= 0
                                                     && !contentController.loadingTarget
                                            onClicked: contentController.search(contentSearchField.text, 0)
                                        }
                                        StrataButton {
                                            text: "‹"
                                            enabled: contentController.canGoBack && !contentController.searching
                                            onClicked: contentController.search(contentSearchField.text,
                                                                                Math.max(0, contentController.pageOffset
                                                                                         - contentController.pageSize))
                                        }
                                        Label {
                                            text: contentController.totalProjects > 0
                                                  ? (Math.floor(contentController.pageOffset
                                                                / contentController.pageSize) + 1)
                                                    + " / " + Math.ceil(contentController.totalProjects
                                                                        / contentController.pageSize)
                                                  : "—"
                                            color: ThemeSettings.theme === "dark" ? "#dfe7f1" : "#475569"
                                        }
                                        StrataButton {
                                            text: "›"
                                            enabled: contentController.canGoForward && !contentController.searching
                                            onClicked: contentController.search(contentSearchField.text,
                                                                                contentController.pageOffset
                                                                                + contentController.pageSize)
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        spacing: 14

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.preferredWidth: 1
                                            radius: Theme.radiusMedium
                                            color: ThemeSettings.theme === "dark" ? "#991e293b" : "#ccffffff"
                                            border.color: ThemeSettings.theme === "dark" ? "#233349" : "#dfe5f3"

                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.margins: 12
                                                spacing: 8
                                                Label {
                                                    text: "Modrinth projects"
                                                    font.family: "Outfit"
                                                    font.pixelSize: 19
                                                    color: Theme.textPrimary
                                                }
                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    visible: root.selectedContentIndices.length > 0
                                                    Label {
                                                        Layout.fillWidth: true
                                                        text: root.selectedContentIndices.length + " selected"
                                                        color: ThemeSettings.theme === "dark" ? "#cbd5e1" : "#475569"
                                                    }
                                                    StrataButton {
                                                        text: "Disable"
                                                        enabled: !contentController.busy
                                                        onClicked: root.applyInstalledContentAction("disable")
                                                    }
                                                    StrataButton {
                                                        text: "Enable"
                                                        enabled: !contentController.busy
                                                        onClicked: root.applyInstalledContentAction("enable")
                                                    }
                                                    StrataButton {
                                                        text: "Delete"
                                                        enabled: !contentController.busy
                                                        onClicked: root.applyInstalledContentAction("delete")
                                                    }
                                                }
                                                ListView {
                                                    id: contentProjectList
                                                    Layout.fillWidth: true
                                                    Layout.fillHeight: true
                                                    clip: true
                                                    spacing: 8
                                                    model: contentController.projects
                                                    delegate: Rectangle {
                                                        required property var modelData
                                                        width: contentProjectList.width
                                                        height: 94
                                                        radius: Theme.radiusSmall
                                                        color: ThemeSettings.theme === "dark" ? "#991e293b" : "#ccffffff"
                                                        border.color: ThemeSettings.theme === "dark" ? "#263a54" : "#e2e8f0"
                                                        ColumnLayout {
                                                            anchors.fill: parent
                                                            anchors.margins: 10
                                                            spacing: 3
                                                            Label {
                                                                text: modelData.title
                                                                font.family: "Outfit"
                                                                font.pixelSize: 16
                                                                color: Theme.textPrimary
                                                                elide: Text.ElideRight
                                                                Layout.fillWidth: true
                                                            }
                                                            Label {
                                                                text: modelData.description
                                                                font.family: "DM Sans"
                                                                font.pixelSize: 12
                                                                color: Theme.textSecondary
                                                                elide: Text.ElideRight
                                                                wrapMode: Text.WordWrap
                                                                Layout.fillWidth: true
                                                            }
                                                            Label {
                                                                text: Number(modelData.downloads).toLocaleString()
                                                                      + " downloads"
                                                                font.family: "DM Sans"
                                                                font.pixelSize: 11
                                                                color: Theme.textSecondary
                                                            }
                                                        }
                                                        MouseArea {
                                                            anchors.fill: parent
                                                            onClicked: contentController.loadVersions(modelData.id)
                                                        }
                                                    }
                                                    Label {
                                                        anchors.centerIn: parent
                                                        visible: contentController.projects.length === 0
                                                        text: contentController.searching ? "Searching Modrinth..."
                                                              : "Search for compatible projects."
                                                        color: Theme.textSecondary
                                                    }
                                                }
                                                StrataComboBox {
                                                    id: contentVersionBox
                                                    Layout.fillWidth: true
                                                    model: contentController.versions
                                                    textRole: "versionNumber"
                                                    valueRole: "id"
                                                    displayText: currentIndex >= 0 ? currentText : "Choose a compatible version"
                                                }
                                                StrataButton {
                                                    text: contentController.busy ? contentController.installItem : "Install selected"
                                                    enabled: !contentController.busy && contentVersionBox.currentIndex >= 0
                                                             && (contentTypeBox.currentValue !== "DATA_PACK"
                                                                 || contentWorldBox.currentIndex >= 0)
                                                    onClicked: contentController.installSelected(
                                                                   contentVersionBox.currentValue,
                                                                   contentTypeBox.currentValue === "DATA_PACK"
                                                                     ? contentWorldBox.currentValue : "")
                                                }
                                            }
                                        }

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.preferredWidth: 1
                                            radius: Theme.radiusMedium
                                            color: ThemeSettings.theme === "dark" ? "#991e293b" : "#ccffffff"
                                            border.color: ThemeSettings.theme === "dark" ? "#233349" : "#dfe5f3"
                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.margins: 12
                                                spacing: 8
                                                Label {
                                                    text: "Installed content"
                                                    font.family: "Outfit"
                                                    font.pixelSize: 19
                                                    color: Theme.textPrimary
                                                }
                                                ListView {
                                                    id: installedContentList
                                                    Layout.fillWidth: true
                                                    Layout.fillHeight: true
                                                    clip: true
                                                    spacing: 7
                                                    model: contentController.installedContent
                                                    delegate: Rectangle {
                                                        required property var modelData
                                                        required property int index
                                                        width: installedContentList.width
                                                        height: 72
                                                        radius: Theme.radiusSmall
                                                        color: ThemeSettings.theme === "dark" ? "#991e293b" : "#ccffffff"
                                                        border.color: ThemeSettings.theme === "dark" ? "#263a54" : "#e2e8f0"
                                                        RowLayout {
                                                            anchors.fill: parent
                                                            anchors.margins: 8
                                                            spacing: 6
                                                            ContentSelectionBox {
                                                                checked: root.selectedContentIndices.indexOf(index) >= 0
                                                                text: "Select"
                                                                Accessible.name: "Select " + modelData.title
                                                                onSelected: modifiers =>
                                                                                root.toggleInstalledContent(
                                                                                    index,
                                                                                    (modifiers
                                                                                     & Qt.ShiftModifier) !== 0)
                                                            }
                                                            ColumnLayout {
                                                                Layout.fillWidth: true
                                                                spacing: 2
                                                                Label {
                                                                    text: modelData.title + " · " + modelData.versionNumber
                                                                    font.family: "DM Sans"
                                                                    font.pixelSize: 13
                                                                    color: Theme.textPrimary
                                                                    elide: Text.ElideRight
                                                                    Layout.fillWidth: true
                                                                }
                                                                Label {
                                                                    text: modelData.filename
                                                                    font.family: "JetBrains Mono"
                                                                    font.pixelSize: 10
                                                                    color: Theme.textSecondary
                                                                    elide: Text.ElideMiddle
                                                                    Layout.fillWidth: true
                                                                }
                                                            }
                                                            StrataButton {
                                                                text: modelData.enabled ? "Disable" : "Enable"
                                                                iconName: modelData.enabled
                                                                          ? "secure-off" : "secure"
                                                                enabled: !contentController.busy
                                                                onClicked: contentController.setInstalledEnabled(
                                                                               index, !modelData.enabled)
                                                            }
                                                            StrataButton {
                                                                text: "Delete"
                                                                iconName: "delete"
                                                                enabled: !contentController.busy
                                                                onClicked: contentController.removeInstalled(index)
                                                            }
                                                        }
                                                    }
                                                    Label {
                                                        anchors.centerIn: parent
                                                        visible: contentController.installedContent.length === 0
                                                        text: "No content installed in this category."
                                                        color: Theme.textSecondary
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    ProgressBar {
                                        Layout.fillWidth: true
                                        visible: contentController.busy
                                        indeterminate: contentController.installProgress < 0
                                        value: Math.max(0, contentController.installProgress)
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: contentController.statusMessage
                                        visible: text.length > 0
                                        wrapMode: Text.WordWrap
                                        font.family: "DM Sans"
                                        font.pixelSize: 12
                                        color: Theme.textSecondary
                                    }
                                }

                            StrataComboBox {
                                id: loaderTypeBox
                                Layout.fillWidth: true
                                model: [
                                    { "label": "Vanilla", "id": "VANILLA" },
                                    { "label": "Fabric", "id": "FABRIC" },
                                    { "label": "Quilt", "id": "QUILT" },
                                    { "label": "Forge", "id": "FORGE" },
                                    { "label": "NeoForge", "id": "NEOFORGE" }
                                ]
                                textRole: "label"
                                valueRole: "id"
                                onCurrentValueChanged: {
                                    if (currentValue && currentValue !== "VANILLA"
                                            && minecraftVersionBox.currentValue) {
                                        LauncherController.loadLoaderVersions(
                                                    currentValue, minecraftVersionBox.currentValue)
                                    } else {
                                        LauncherController.loadLoaderVersions("VANILLA", "")
                                    }
                                }
                            }

                            StrataComboBox {
                                id: loaderVersionBox
                                Layout.fillWidth: true
                                visible: loaderTypeBox.currentValue !== "VANILLA"
                                model: LauncherController.loaderVersions
                                enabled: !LauncherController.loadingLoaderVersions
                                         && LauncherController.loaderVersions.length > 0
                                currentIndex: model.length > 0 ? 0 : -1
                            }

                            Label {
                                visible: loaderTypeBox.currentValue !== "VANILLA"
                                text: LauncherController.loadingLoaderVersions
                                      ? "Fetching compatible loader versions..."
                                      : LauncherController.loaderVersionsError.length > 0
                                        ? LauncherController.loaderVersionsError
                                        : LauncherController.loaderVersions.length === 0
                                          ? "No compatible loader versions are available."
                                          : "Choose a loader version."
                                wrapMode: Text.Wrap
                                font.family: "DM Sans"
                                font.pixelSize: 13
                                color: Theme.textSecondary
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
                                color: Theme.textSecondary
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Item { Layout.fillWidth: true }
                                StrataButton {
                                    text: "Cancel"
                                    onClicked: createDialogVisible = false
                                }
                                StrataButton {
                                    text: "Create"
                                    enabled: instanceNameInput.text.trim().length > 0
                                             && minecraftVersionBox.currentIndex >= 0
                                             && (loaderTypeBox.currentValue === "VANILLA"
                                                 || loaderVersionBox.currentIndex >= 0)
                                             && !LauncherController.loadingLoaderVersions
                                             && !LauncherController.installing
                                    onClicked: LauncherController.installInstanceWithLoader(
                                                   instanceNameInput.text,
                                                   minecraftVersionBox.currentValue,
                                                   loaderTypeBox.currentValue,
                                                   loaderVersionBox.currentValue)
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
                        TapHandler {
                            onTapped: {
                                if (instanceList.indexAt(point.position.x, point.position.y) < 0) {
                                    selectedInstanceIndex = -1
                                    instanceList.currentIndex = -1
                                }
                            }
                        }

                        delegate: Rectangle {
                            id: instanceCard
                            width: instanceList.width
                            height: 92
                            radius: Theme.radiusLarge
                            color: (instanceList.currentIndex === index || instanceCardHover.hovered) ? (ThemeSettings.theme === "dark" ? "#cc334155" : "#ccdbeafe") : (ThemeSettings.theme === "dark" ? "#991e293b" : "#b3ffffff")
                            border.color: (instanceList.currentIndex === index || instanceCardHover.hovered) ? (ThemeSettings.theme === "dark" ? "#3a6ea7" : "#c8ddff") : (ThemeSettings.theme === "dark" ? "#172638" : "#edf2f7")
                            property bool isSelected: instanceList.currentIndex === index
                            objectName: "instanceCard"
                            scale: isSelected || instanceCardHover.hovered ? 1 : 0.99

                            HoverHandler { id: instanceCardHover }
                            Behavior on scale {
                                NumberAnimation {
                                    duration: Motion.fastDuration
                                    easing.type: Motion.standardEasing
                                    easing.bezierCurve: Motion.standardCurve
                                }
                            }
                            Behavior on color {
                                ColorAnimation {
                                    duration: Motion.fastDuration
                                    easing.type: Motion.standardEasing
                                    easing.bezierCurve: Motion.standardCurve
                                }
                            }
                            Behavior on border.color {
                                ColorAnimation {
                                    duration: Motion.fastDuration
                                    easing.type: Motion.standardEasing
                                    easing.bezierCurve: Motion.standardCurve
                                }
                            }

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
                                spacing: 12
                                Row {
                                    spacing: 8
                                    StrataButton {
                                        text: status === "starting" || status === "ready" ? "Stop" : "Launch"
                                        iconName: status === "starting" || status === "ready"
                                                  ? "stop" : "start"
                                        width: 116
                                        enabled: instanceList.currentIndex === index
                                        Accessible.name: text + " " + name
                                        scale: down ? 0.96 : 1
                                        Behavior on scale {
                                            NumberAnimation {
                                                duration: Motion.fastDuration
                                                easing.type: Motion.standardEasing
                                                easing.bezierCurve: Motion.standardCurve
                                            }
                                        }
                                        onClicked: root.launchSelected()
                                        background: Rectangle { radius: Theme.radiusSmall; color: ThemeSettings.theme === "dark" ? "#2a4d7d" : "#dfeeff" }
                                    }
                                    StrataButton {
                                        text: "Settings"
                                        iconName: "settings"
                                        width: 116
                                        Accessible.name: "Settings for " + name
                                        scale: down ? 0.96 : 1
                                        Behavior on scale {
                                            NumberAnimation {
                                                duration: Motion.fastDuration
                                                easing.type: Motion.standardEasing
                                                easing.bezierCurve: Motion.standardCurve
                                            }
                                        }
                                        onClicked: {
                                            selectedInstanceIndex = index
                                            settingsFlyout.open()
                                            instanceSettingsController.loadInstance(id)
                                        }
                                        background: Rectangle { radius: Theme.radiusSmall; color: ThemeSettings.theme === "dark" ? "#1a2431" : "#f3f6fb" }
                                    }
                                }
                                Item { Layout.fillWidth: true }
                                StatusLight { status: status; size: 12 }
                                ColumnLayout {
                                    spacing: 4
                                    Layout.preferredWidth: 250
                                    Layout.maximumWidth: 250
                                    Label {
                                        Layout.fillWidth: true
                                        text: name
                                        font.family: "Outfit"
                                        font.pixelSize: 22
                                        color: Theme.textPrimary
                                        horizontalAlignment: Text.AlignRight
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Minecraft " + version
                                        font.family: "DM Sans"
                                        font.pixelSize: 13
                                        color: Theme.textSecondary
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        spacing: 12
                        Layout.fillWidth: true

                        StrataButton {
                            id: launchButton
                            text: selectedInstanceIndex >= 0
                                  && (instanceModel.get(selectedInstanceIndex).status === "starting"
                                      || instanceModel.get(selectedInstanceIndex).status === "ready")
                                  ? "Stop" : "Launch"
                            enabled: selectedInstanceIndex >= 0
                            onClicked: root.launchSelected()
                            background: Rectangle {
                                radius: Theme.radiusMedium
                                color: enabled ? (ThemeSettings.theme === "dark" ? "#2a5dce" : "#2f80ed") : (ThemeSettings.theme === "dark" ? "#1e2d46" : "#dfe7f0")
                            }
                            contentItem: Row {
                                spacing: 8
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                ThemeAwareIcon {
                                    iconName: launchButton.text === "Stop" ? "stop" : "start"
                                    size: 18
                                }
                                Label {
                                    text: launchButton.text
                                    color: launchButton.enabled ? "#ffffff"
                                                                : (ThemeSettings.theme === "dark"
                                                                   ? "#94a3b8" : "#7a8aa2")
                                    verticalAlignment: Text.AlignVCenter
                                }
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
                Loader {
                    id: contentPageLoader
                    anchors.fill: parent
                    z: root.selectedPage === "Content" ? 2 : 1
                    visible: root.selectedPage === "Content" || opacity > 0
                    opacity: root.selectedPage === "Content" ? 1 : 0
                    x: root.selectedPage === "Content" ? 0 : -14
                    Behavior on opacity {
                        NumberAnimation {
                            duration: Motion.normalDuration
                            easing.type: Motion.standardEasing
                            easing.bezierCurve: Motion.standardCurve
                        }
                    }
                    Behavior on x {
                        NumberAnimation {
                            duration: Motion.normalDuration
                            easing.type: Motion.standardEasing
                            easing.bezierCurve: Motion.standardCurve
                        }
                    }
                    sourceComponent: createDialogContent.contentPageComponent
                }
                Loader {
                    id: serversPageLoader
                    objectName: "serversPageLoader"
                    anchors.fill: parent
                    z: root.selectedPage === "Servers" ? 2 : 1
                    visible: root.selectedPage === "Servers" || opacity > 0
                    opacity: root.selectedPage === "Servers" ? 1 : 0
                    x: root.selectedPage === "Servers" ? 0 : -14
                    Behavior on opacity {
                        NumberAnimation {
                            duration: Motion.normalDuration
                            easing.type: Motion.standardEasing
                            easing.bezierCurve: Motion.standardCurve
                        }
                    }
                    Behavior on x {
                        NumberAnimation {
                            duration: Motion.normalDuration
                            easing.type: Motion.standardEasing
                            easing.bezierCurve: Motion.standardCurve
                        }
                    }
                    sourceComponent: root.serversPageComponent
                }
                Loader {
                    id: homePageLoader
                    objectName: "homePageLoader"
                    anchors.fill: parent
                    z: root.selectedPage === "Home" ? 2 : 1
                    visible: root.selectedPage === "Home" || opacity > 0
                    opacity: root.selectedPage === "Home" ? 1 : 0
                    x: root.selectedPage === "Home" ? 0 : -14
                    Behavior on opacity {
                        NumberAnimation {
                            duration: Motion.normalDuration
                            easing.type: Motion.standardEasing
                            easing.bezierCurve: Motion.standardCurve
                        }
                    }
                    Behavior on x {
                        NumberAnimation {
                            duration: Motion.normalDuration
                            easing.type: Motion.standardEasing
                            easing.bezierCurve: Motion.standardCurve
                        }
                    }
                    sourceComponent: root.homePageComponent
                }
                Loader {
                    id: aboutPageLoader
                    objectName: "aboutPageLoader"
                    anchors.fill: parent
                    z: root.selectedPage === "About" ? 2 : 1
                    visible: root.selectedPage === "About" || opacity > 0
                    opacity: root.selectedPage === "About" ? 1 : 0
                    x: root.selectedPage === "About" ? 0 : -14
                    Behavior on opacity {
                        NumberAnimation {
                            duration: Motion.normalDuration
                            easing.type: Motion.standardEasing
                            easing.bezierCurve: Motion.standardCurve
                        }
                    }
                    Behavior on x {
                        NumberAnimation {
                            duration: Motion.normalDuration
                            easing.type: Motion.standardEasing
                            easing.bezierCurve: Motion.standardCurve
                        }
                    }
                    sourceComponent: root.aboutPageComponent
                }
            }
        }
    }

    Popup {
        id: serverCreatePopup
        objectName: "serverCreatePopup"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(root.width - 48, 480)
        modal: true
        dim: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        background: Rectangle {
            radius: Theme.radiusLarge
            color: ThemeSettings.theme === "dark" ? "#101927" : "#ffffff"
            border.color: ThemeSettings.theme === "dark" ? "#293d58" : "#dce4ef"
        }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                text: "Create server"
                font.family: "Outfit"
                font.pixelSize: 24
                color: Theme.textPrimary
            }
            StrataTextField {
                id: serverCreateName
                Layout.fillWidth: true
                placeholderText: "Server name"
            }
            StrataComboBox {
                id: serverCreateVersionBox
                objectName: "serverCreateVersionBox"
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
            StrataComboBox {
                id: serverCreateDistribution
                Layout.fillWidth: true
                model: [
                    { "label": "Vanilla", "id": "VANILLA" },
                    { "label": "Fabric", "id": "FABRIC" },
                    { "label": "Paper", "id": "PAPER" }
                ]
                textRole: "label"
                valueRole: "id"
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                StrataButton {
                    text: "Cancel"
                    onClicked: serverCreatePopup.close()
                }
                StrataButton {
                    text: "Create"
                    enabled: !LauncherController.serverBusy
                             && serverCreateName.text.trim().length > 0
                             && serverCreateVersionBox.currentIndex >= 0
                    onClicked: {
                        LauncherController.createServer(serverCreateName.text,
                                                        serverCreateVersionBox.currentValue,
                                                        serverCreateDistribution.currentValue)
                        serverCreatePopup.close()
                        serverCreateName.clear()
                    }
                }
            }
        }
    }

    Popup {
        id: serverDeletePopup
        objectName: "serverDeletePopup"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(root.width - 48, 520)
        modal: true
        dim: true
        focus: true
        closePolicy: Popup.NoAutoClose
        visible: root.pendingServerDeletionId.length > 0
        background: Rectangle {
            radius: Theme.radiusLarge
            color: ThemeSettings.theme === "dark" ? "#101927" : "#ffffff"
            border.color: ThemeSettings.theme === "dark" ? "#293d58" : "#dce4ef"
        }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                text: "Type DELETE to permanently remove this server and its files."
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                color: Theme.textPrimary
            }
            StrataTextField {
                id: serverDeleteConfirmation
                objectName: "serverDeleteConfirmation"
                Layout.fillWidth: true
                placeholderText: "DELETE"
                ContextMenu.menu: null
                Keys.priority: Keys.BeforeItem
                Keys.onPressed: function(event) {
                    const pasteModifier = (event.modifiers & Qt.ControlModifier) ||
                            (event.modifiers & Qt.MetaModifier)
                    if ((event.key === Qt.Key_V && pasteModifier) ||
                            (event.key === Qt.Key_Insert &&
                             (event.modifiers & Qt.ShiftModifier)))
                        event.accepted = true
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton | Qt.MiddleButton
                    onPressed: function(mouse) { mouse.accepted = true }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                StrataButton {
                    text: "Cancel"
                    onClicked: {
                        root.pendingServerDeletionId = ""
                        serverDeleteConfirmation.clear()
                    }
                }
                StrataButton {
                    text: "Remove"
                    dangerous: true
                    enabled: !LauncherController.serverBusy
                             && serverDeleteConfirmation.text === "DELETE"
                    onClicked: {
                        LauncherController.deleteServer(root.pendingServerDeletionId)
                        root.pendingServerDeletionId = ""
                        serverDeleteConfirmation.clear()
                    }
                }
            }
        }
        onVisibleChanged: {
            if (visible)
                Qt.callLater(function() { serverDeleteConfirmation.forceActiveFocus() })
        }
    }

    Popup {
        id: dependencyConfirmationPopup
        objectName: "dependencyConfirmationPopup"
        visible: root.selectedPage === "Content"
                 && contentController.dependencyConfirmationVisible
        modal: true
        dim: true
        focus: true
        closePolicy: Popup.NoAutoClose
        parent: Overlay.overlay
        width: Math.min(560, parent.width - 48)
        height: Math.min(430, parent.height - 48)
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        padding: 22

        background: Rectangle {
            objectName: "dependencyConfirmationBackground"
            radius: Theme.radiusLarge
            color: ThemeSettings.theme === "dark" ? "#111b2a" : "#ffffff"
            border.color: ThemeSettings.theme === "dark" ? "#2a405b" : "#d7e0ef"
        }

        contentItem: ColumnLayout {
            spacing: 14

            Label {
                objectName: "dependencyConfirmationTitle"
                text: "To install this, you need the following dependencies:"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.family: "Outfit"
                font.pixelSize: 21
                color: Theme.textPrimary
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                model: contentController.dependenciesToInstall
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 54
                    radius: Theme.radiusSmall
                    color: ThemeSettings.theme === "dark" ? "#172437" : "#f4f7fc"
                    border.color: ThemeSettings.theme === "dark" ? "#263a54" : "#e2e8f0"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 9
                        spacing: 2
                        Label {
                            text: modelData.title
                            font.family: "DM Sans"
                            font.pixelSize: 14
                            color: Theme.textPrimary
                        }
                        Label {
                            text: modelData.projectType + " · " + modelData.versionNumber
                            font.family: "DM Sans"
                            font.pixelSize: 11
                            color: Theme.textSecondary
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                StrataButton {
                    objectName: "dependencyConfirmationCancel"
                    text: "Cancel"
                    enabled: !contentController.busy
                    onClicked: contentController.cancelDependencyInstall()
                }
                StrataButton {
                    objectName: "dependencyConfirmationInstall"
                    text: "Install"
                    enabled: !contentController.busy
                    onClicked: contentController.confirmDependencyInstall()
                }
            }
        }
    }

    Popup {
        id: settingsFlyout
        objectName: "instanceSettingsPopup"
        modal: true
        dim: true
        focus: true
        closePolicy: Popup.NoAutoClose
        parent: Overlay.overlay
        width: Math.min(960, parent.width - 48)
        height: Math.min(680, parent.height - 48)
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        padding: 18

        background: Rectangle {
            objectName: "instanceSettingsBackground"
            radius: Theme.radiusLarge
            color: ThemeSettings.theme === "dark" ? "#111b2a" : "#ffffff"
            border.color: ThemeSettings.theme === "dark" ? "#1d2c3d" : "#dfe6f5"
        }

        function populateForm() {
            const options = instanceSettingsController.commonOptions
            fovInput.value = Number(options.fov || 70)
            gammaInput.value = Number(options.gamma || 0)
            renderDistanceInput.value = Number(options.renderDistance || 12)
            simulationDistanceInput.value = Number(options.simulationDistance || 12)
            fpsInput.value = Number(options.maxFps || 120)
            unlimitedFpsInput.checked = Boolean(options.unlimitedFps)
            guiScaleInput.value = Number(options.guiScale || 0)
            fullscreenInput.checked = Boolean(options.fullscreen)
            vsyncInput.checked = options.enableVsync !== false
            javaFlagsOverrideInput.checked = instanceSettingsController.javaFlagsOverridden
            javaFlagsInput.text = instanceSettingsController.javaFlags
            serverList.currentIndex = -1
            serverNameInput.clear()
            serverAddressInput.clear()
            const bindings = {}
            for (let index = 0; index < instanceSettingsController.controls.length; ++index) {
                const binding = instanceSettingsController.controls[index]
                bindings[binding.key] = binding.value
            }
            root.controlDraft = bindings
        }

        Connections {
            target: instanceSettingsController
            function onChanged() {
                if (!instanceSettingsController.busy)
                    settingsFlyout.populateForm()
            }
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: instanceSettingsController.instanceName.length > 0
                          ? instanceSettingsController.instanceName + " settings"
                          : "Instance settings"
                    font.family: "Outfit"
                    font.pixelSize: 26
                    color: Theme.textPrimary
                }
                Item { Layout.fillWidth: true }
                StrataButton {
                    text: "Close"
                    onClicked: settingsFlyout.close()
                }
            }

            TabBar {
                id: settingsTabs
                Layout.fillWidth: true
                padding: 8
                background: Rectangle { color: "transparent" }
                StrataTabButton { text: "Game options" }
                StrataTabButton { text: "Controls"; iconName: "controls" }
                StrataTabButton { text: "Servers" }
                StrataTabButton { text: "Java" }
                StrataTabButton { text: "Dangerous"; iconName: "dangerous"; dangerous: true }
                onCurrentIndexChanged: Qt.callLater(
                                           () => root.staggerPageItems(settingsTabContent.children[currentIndex]))
            }

            StackLayout {
                id: settingsTabContent
                currentIndex: settingsTabs.currentIndex
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    spacing: 12
                    Label {
                        text: "These settings apply only to " + instanceSettingsController.instanceName
                              + " and preserve other entries in options.txt."
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.textSecondary
                    }
                    GridLayout {
                        columns: 2
                        columnSpacing: 18
                        rowSpacing: 12
                        Layout.fillWidth: true

                        Label { text: "Field of view"; color: Theme.textPrimary }
                        StrataSpinBox { id: fovInput; from: 30; to: 110; value: 70; editable: true }
                        Label { text: "Gamma / brightness"; color: Theme.textPrimary }
                        Slider { id: gammaInput; from: 0; to: 1; stepSize: 0.01; value: 0; Layout.fillWidth: true }
                        Label { text: "Render distance (chunks)"; color: Theme.textPrimary }
                        StrataSpinBox { id: renderDistanceInput; from: 2; to: 128; value: 12; editable: true }
                        Label { text: "Simulation distance (chunks)"; color: Theme.textPrimary }
                        StrataSpinBox { id: simulationDistanceInput; from: 2; to: 128; value: 12; editable: true }
                        Label { text: "Maximum frame rate"; color: Theme.textPrimary }
                        RowLayout {
                            StrataSpinBox { id: fpsInput; from: 10; to: 500; value: 120; editable: true; enabled: !unlimitedFpsInput.checked }
                            StrataCheckBox { id: unlimitedFpsInput; text: "Unlimited" }
                        }
                        Label { text: "GUI scale (0 = Auto)"; color: Theme.textPrimary }
                        StrataSpinBox { id: guiScaleInput; from: 0; to: 16; value: 0; editable: true }
                        Label { text: "Display"; color: Theme.textPrimary }
                        ColumnLayout {
                            StrataCheckBox { id: fullscreenInput; text: "Start in fullscreen" }
                            StrataCheckBox { id: vsyncInput; text: "Vertical synchronization" }
                        }
                    }
                    StrataButton {
                        text: "Save game options"
                        iconName: "save"
                        enabled: !instanceSettingsController.busy
                        onClicked: instanceSettingsController.saveCommonOptions({
                            "fov": fovInput.value,
                            "gamma": gammaInput.value,
                            "renderDistance": renderDistanceInput.value,
                            "simulationDistance": simulationDistanceInput.value,
                            "maxFps": fpsInput.value,
                            "unlimitedFps": unlimitedFpsInput.checked,
                            "guiScale": guiScaleInput.value,
                            "fullscreen": fullscreenInput.checked,
                            "enableVsync": vsyncInput.checked
                        })
                    }
                    Label {
                        text: "Advanced"
                        font.family: "Outfit"
                        font.pixelSize: 20
                        color: Theme.textPrimary
                    }
                    Label {
                        text: "Edit an additional Minecraft setting using its raw options.txt key and value."
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.textSecondary
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        StrataTextField { id: advancedOptionKey; placeholderText: "Option key" }
                        StrataTextField { id: advancedOptionValue; placeholderText: "Raw value" }
                        StrataButton {
                            text: "Save option"
                            iconName: "save"
                            enabled: !instanceSettingsController.busy
                            onClicked: instanceSettingsController.saveOption(
                                           advancedOptionKey.text, advancedOptionValue.text)
                        }
                    }
                    Item { Layout.fillHeight: true }
                }

                ColumnLayout {
                    spacing: 10
                    Label {
                        text: root.capturingControlKey.length > 0
                              ? "Press a key to bind "
                                + root.capturingControlKey.replace("key_key.", "").replace(".", " ")
                                + ". Press Escape to cancel."
                              : "Select Rebind, then press the keyboard key you want to use."
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.textSecondary
                    }
                    ListView {
                        id: controlBindingsList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Keys.onPressed: event => root.bindControlKey(event)
                        clip: true
                        spacing: 6
                        model: instanceSettingsController.controls
                        delegate: Rectangle {
                            required property var modelData
                            width: controlBindingsList.width
                            height: 48
                            radius: Theme.radiusSmall
                            color: ThemeSettings.theme === "dark"
                                   ? (index % 2 === 0 ? "#172437" : "#1d2b3d")
                                   : (index % 2 === 0 ? "#eef2f7" : "#e2e8f0")
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    color: Theme.textPrimary
                                }
                                StrataButton {
                                    text: root.capturingControlKey === modelData.key
                                          ? "Press a key…"
                                          : (root.controlDraft[modelData.key] || modelData.value)
                                                .replace("key.keyboard.", "")
                                                .replace("key.mouse.", "Mouse ")
                                                .toUpperCase()
                                    onClicked: {
                                        root.capturingControlKey = modelData.key
                                        controlBindingsList.forceActiveFocus()
                                    }
                                }
                                StrataButton {
                                    text: "Unassign"
                                    iconName: "close"
                                    onClicked: {
                                        const updated = Object.assign({}, root.controlDraft)
                                        updated[modelData.key] = "key.keyboard.unknown"
                                        root.controlDraft = updated
                                    }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: instanceSettingsController.status
                            color: Theme.textSecondary
                            wrapMode: Text.WordWrap
                        }
                        StrataButton {
                            text: "Save controls"
                            iconName: "save"
                            enabled: !instanceSettingsController.busy
                            onClicked: {
                                root.capturingControlKey = ""
                                instanceSettingsController.saveControls(root.controlDraft)
                            }
                        }
                    }
                }

                ColumnLayout {
                    spacing: 10
                    Label {
                        text: "Saved multiplayer servers for this instance."
                        color: Theme.textSecondary
                    }
                    ListView {
                        id: serverList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 6
                        model: instanceSettingsController.servers
                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            height: 48
                            radius: Theme.radiusSmall
                            color: serverList.currentIndex === index
                                   ? (ThemeSettings.theme === "dark" ? "#243852" : "#e4edfa")
                                   : (ThemeSettings.theme === "dark" ? "#172437" : "#f4f7fc")
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                Label {
                                    text: modelData.name
                                    font.bold: true
                                    color: Theme.textPrimary
                                }
                                Label {
                                    text: modelData.address
                                    Layout.fillWidth: true
                                    color: Theme.textSecondary
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    serverList.currentIndex = index
                                    serverNameInput.text = modelData.name
                                    serverAddressInput.text = modelData.address
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        StrataTextField { id: serverNameInput; placeholderText: "Server name"; Layout.fillWidth: true }
                        StrataTextField { id: serverAddressInput; placeholderText: "Server address"; Layout.fillWidth: true }
                    }
                    RowLayout {
                        StrataButton {
                            text: "Add server"
                            enabled: !instanceSettingsController.busy
                            onClicked: instanceSettingsController.addServer(serverNameInput.text,
                                                                             serverAddressInput.text)
                        }
                        StrataButton {
                            text: "Save selected"
                            enabled: serverList.currentIndex >= 0 && !instanceSettingsController.busy
                            onClicked: instanceSettingsController.updateServer(
                                           serverList.currentIndex, serverNameInput.text,
                                           serverAddressInput.text)
                        }
                        StrataButton {
                            text: "Remove selected"
                            enabled: serverList.currentIndex >= 0 && !instanceSettingsController.busy
                            onClicked: instanceSettingsController.removeServer(serverList.currentIndex)
                        }
                    }
                }

                ColumnLayout {
                    spacing: 10
                    Label {
                        text: selectedInstanceIndex >= 0
                              ? "Current runtime: "
                                + (instanceModel.get(selectedInstanceIndex).runtimeMode === "MANAGED"
                                   ? "Managed · " + instanceModel.get(selectedInstanceIndex).runtimeComponent
                                   : "System Java")
                              : "Select an instance to configure its Java runtime."
                        color: Theme.textSecondary
                    }
                    Label {
                        text: "Use launcher Java flags or define an instance override. One JVM option per line."
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.textSecondary
                    }
                    StrataCheckBox {
                        id: javaFlagsOverrideInput
                        text: "Use instance-specific Java flags"
                        enabled: !instanceSettingsController.busy
                    }
                    StrataTextArea {
                        id: javaFlagsInput
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        enabled: javaFlagsOverrideInput.checked && !instanceSettingsController.busy
                        selectByMouse: true
                        wrapMode: TextEdit.NoWrap
                        font.family: "JetBrains Mono"
                        placeholderText: "-Xmx4G\n-XX:+UseG1GC"
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        StrataButton {
                            text: "Save Java flags"
                            enabled: !instanceSettingsController.busy
                            onClicked: instanceSettingsController.saveJavaFlags(
                                           javaFlagsOverrideInput.checked, javaFlagsInput.text)
                        }
                        Item { Layout.fillWidth: true }
                        StrataButton {
                            text: "Install and use " + LauncherController.runtimeVendor + " Java"
                            enabled: selectedInstanceIndex >= 0 && !LauncherController.installingRuntime
                            onClicked: LauncherController.installInstanceRuntime(
                                           instanceModel.get(selectedInstanceIndex).id)
                        }
                        StrataButton {
                            text: "Use System Java"
                            enabled: selectedInstanceIndex >= 0
                            onClicked: LauncherController.useSystemJava(
                                           instanceModel.get(selectedInstanceIndex).id)
                        }
                    }
                }

                ColumnLayout {
                    spacing: 12
                    Label {
                        text: "Dangerous instance actions"
                        font.family: "Outfit"
                        font.pixelSize: 20
                        color: "#ef4444"
                    }
                    Label {
                        text: "Deleting permanently removes this instance and its local files."
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.textSecondary
                    }
                    StrataTextField {
                        id: instanceDeleteConfirmation
                        Layout.fillWidth: true
                        placeholderText: "Type DELETE to remove this instance"
                        enabled: !LauncherController.deletingInstance
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: LauncherController.deletingInstance
                                  ? "Deleting instance…"
                                  : instanceSettingsController.instanceName
                        }
                        StrataButton {
                            text: "Delete instance"
                            iconName: "delete"
                            dangerous: true
                            enabled: selectedInstanceIndex >= 0
                                     && instanceDeleteConfirmation.text === "DELETE"
                                     && !LauncherController.deletingInstance
                            onClicked: {
                                const id = instanceModel.get(selectedInstanceIndex).id
                                LauncherController.deleteInstance(id)
                                instanceDeleteConfirmation.clear()
                                settingsFlyout.close()
                            }
                        }
                    }
                }

            }

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: instanceSettingsController.status
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textSecondary
                }
                BusyIndicator { running: instanceSettingsController.busy; visible: running }
            }
        }
    }

    Popup {
        id: runtimeDeletePopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(root.width - 48, 560)
        modal: true
        dim: true
        focus: true
        closePolicy: Popup.NoAutoClose
        background: Rectangle {
            radius: Theme.radiusLarge
            color: ThemeSettings.theme === "dark" ? "#111b2a" : "#ffffff"
            border.color: ThemeSettings.theme === "dark" ? "#1d2c3d" : "#dfe6f5"
        }
        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12
            Label {
                text: "Delete Java runtime"
                font.family: "Outfit"
                font.pixelSize: 22
                color: Theme.textPrimary
            }
            Label {
                Layout.fillWidth: true
                text: root.pendingRuntimeDeletion
                      ? root.pendingRuntimeDeletion.vendor + " Java "
                        + root.pendingRuntimeDeletion.version
                      : ""
                color: Theme.textPrimary
            }
            Label {
                Layout.fillWidth: true
                text: root.pendingRuntimeDeletion && root.pendingRuntimeDeletion.users.length > 0
                      ? "Currently used by:\n" + root.pendingRuntimeDeletion.users.join("\n")
                      : "No instances or servers currently use this runtime."
                wrapMode: Text.WordWrap
                color: Theme.textSecondary
            }
            Label {
                Layout.fillWidth: true
                visible: root.pendingRuntimeDeletion
                         && root.pendingRuntimeDeletion.inUse
                text: "Stop using this runtime in every listed instance or server before deleting it."
                wrapMode: Text.WordWrap
                color: "#ef4444"
            }
            StrataTextField {
                id: runtimeDeleteConfirmation
                Layout.fillWidth: true
                placeholderText: "Type DELETE to confirm"
                enabled: !root.pendingRuntimeDeletion || !root.pendingRuntimeDeletion.inUse
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                StrataButton {
                    text: "Cancel"
                    iconName: "close"
                    onClicked: runtimeDeletePopup.close()
                }
                StrataButton {
                    text: "Delete runtime"
                    iconName: "delete"
                    dangerous: true
                    enabled: root.pendingRuntimeDeletion
                             && !root.pendingRuntimeDeletion.inUse
                             && runtimeDeleteConfirmation.text === "DELETE"
                    onClicked: {
                        LauncherController.deleteRuntime(root.pendingRuntimeDeletion.directory)
                        runtimeDeletePopup.close()
                    }
                }
            }
        }
        onVisibleChanged: {
            if (visible) {
                runtimeDeleteConfirmation.clear()
                Qt.callLater(() => runtimeDeleteConfirmation.forceActiveFocus())
            }
        }
    }

    ShaderEffectSource {
        id: settingsBackdropCapture
        objectName: "settingsBackdropCapture"
        anchors.fill: parent
        sourceItem: shell
        visible: runtimeManagerVisible
        live: true
        hideSource: runtimeManagerVisible
        opacity: 0
    }

    MultiEffect {
        anchors.fill: parent
        source: settingsBackdropCapture
        visible: runtimeManagerVisible
        blurEnabled: true
        blur: 0.65
        blurMax: 32
        z: 41
    }

    Rectangle {
        anchors.fill: parent
        visible: runtimeManagerVisible
        color: "#66000000"
        z: 42
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onClicked: {}
        }
    }

    Rectangle {
        id: consolePanel
        visible: LauncherController.consoleInstanceName.length > 0
        anchors.centerIn: parent
        width: Math.min(parent.width - 80, 900)
        height: Math.min(parent.height - 80, 560)
        radius: Theme.radiusLarge
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
                    color: Theme.textPrimary
                }
                Item { Layout.fillWidth: true }
                StrataButton {
                    text: "Stop"
                    onClicked: LauncherController.stopConsoleInstance()
                }
                StrataButton {
                    text: "Close"
                    onClicked: LauncherController.closeConsole()
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                StrataTextArea {
                    id: instanceConsoleTextArea
                    objectName: "instanceConsoleTextArea"
                    text: LauncherController.consoleText
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.NoWrap
                    font.family: "JetBrains Mono"
                    font.pixelSize: 12
                    color: ThemeSettings.theme === "dark" ? "#dbeafe" : "#1e293b"
                    onTextChanged: cursorPosition = length
                    background: Rectangle {
                        color: ThemeSettings.theme === "dark" ? "#090f19" : "#f7f9fc"
                        radius: Theme.radiusSmall
                    }
                }
            }
        }
    }

    Rectangle {
        id: runtimeManagerPanel
        objectName: "launcherSettingsPanel"
        visible: runtimeManagerVisible
        anchors.centerIn: parent
        width: Math.min(parent.width - 80, 900)
        height: Math.min(parent.height - 80, 620)
        radius: Theme.radiusLarge
        color: ThemeSettings.theme === "dark" ? "#f0111827" : "#f2ffffff"
        border.color: ThemeSettings.theme === "dark" ? "#1d2c3d" : "#dfe6f5"
        z: 43

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "Launcher Settings"
                    font.family: "Outfit"
                    font.pixelSize: 28
                    color: Theme.textPrimary
                }
                Item { Layout.fillWidth: true }
                StrataButton {
                    text: "General"
                    checkable: true
                    checked: settingsTab === "General"
                    onClicked: {
                        settingsTab = "General"
                        Qt.callLater(() => root.staggerPageItems(generalSettings))
                    }
                }
                StrataButton {
                    text: "Runtime Manager"
                    checkable: true
                    checked: settingsTab === "Runtimes"
                    onClicked: {
                        settingsTab = "Runtimes"
                        Qt.callLater(() => root.staggerPageItems(runtimeManagerSettings))
                    }
                }
                StrataButton {
                    text: "Refresh runtimes"
                    visible: settingsTab === "Runtimes"
                    onClicked: LauncherController.refreshRuntimes()
                }
                StrataButton {
                    text: "Close"
                    onClicked: root.closeLauncherSettings()
                }
            }

            ColumnLayout {
                id: generalSettings
                objectName: "generalLauncherSettings"
                visible: settingsTab === "General"
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    StrataSwitch {
                        text: "Dark mode"
                        checked: root.pendingDarkMode
                        onClicked: root.pendingDarkMode = checked
                    }
                    StrataSwitch {
                        text: "Reduce motion"
                        checked: root.pendingReduceMotion
                        onClicked: root.pendingReduceMotion = checked
                    }
                    Item { Layout.fillWidth: true }
                }

                StrataCheckBox {
                    text: "Don't track Instance launches"
                    checked: !root.pendingTrackInstanceLaunches
                    onClicked: root.pendingTrackInstanceLaunches = !checked
                }

                RowLayout {
                    Layout.fillWidth: true
                    StrataCheckBox {
                        text: "VSync"
                        checked: ThemeSettings.vsyncEnabled
                        onClicked: {
                            ThemeSettings.vsyncEnabled = checked
                            root.showToast("VSync will change after restarting the launcher.")
                        }
                    }
                    StrataCheckBox {
                        text: "Show frame-time overlay (debug)"
                        checked: root.pendingFrameTimeOverlay
                        enabled: !AppDebugFrameTimeForced
                        onClicked: root.pendingFrameTimeOverlay = checked
                    }
                }
                Label {
                    text: "VSync takes effect after a restart. The debug overlay reports frame times; press Ctrl+F9 to clear its samples."
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }

                Label {
                    text: "Azure client ID"
                    font.family: "DM Sans"
                    color: Theme.textPrimary
                }
                RowLayout {
                    Layout.fillWidth: true
                    StrataTextField {
                        id: launcherClientIdInput
                        Layout.fillWidth: true
                        placeholderText: "Azure application (client) ID"
                        text: LauncherController.microsoftClientId
                        Accessible.name: "Azure client ID"
                    }
                }

                Label {
                    text: "Global Java launch flags"
                    font.family: "DM Sans"
                    color: Theme.textPrimary
                }
                StrataTextArea {
                    id: globalJavaFlagsField
                    objectName: "globalJavaFlagsField"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    placeholderText: LauncherController.recommendedJavaArguments.join("\n")
                    wrapMode: TextEdit.NoWrap
                    font.family: "JetBrains Mono"
                    font.pixelSize: 12
                    selectByMouse: true
                    Accessible.name: "Global Java launch flags, one argument per line"
                    Component.onCompleted: text = LauncherController.globalJavaArguments.join("\n")
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: "These JVM arguments apply to instances without their own Java-flag override. Unsupported values can cause crashes or prevent Minecraft from starting; most players should leave the defaults unchanged."
                        wrapMode: Text.WordWrap
                        color: Theme.textSecondary
                    }
                    StrataButton {
                        text: "Use recommended"
                        onClicked: globalJavaFlagsField.text =
                                       LauncherController.recommendedJavaArguments.join("\n")
                    }
                }
            }

            ColumnLayout {
                id: runtimeManagerSettings
                visible: settingsTab === "Runtimes"
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                Label {
                    text: "Preferred Java runtime provider"
                    color: Theme.textPrimary
                }

                StrataComboBox {
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
                            if (model[index].value === root.pendingRuntimeVendor)
                                return index
                        return 0
                    }
                    onActivated: root.pendingRuntimeVendor = currentValue
                }

                StrataCheckBox {
                    id: redundantRuntimeSwitch
                    text: "Keep a separate copy for each instance or server"
                    checked: root.pendingRedundantRuntimes
                    onClicked: root.pendingRedundantRuntimes = checked
                }
            }

            Label {
                visible: settingsTab === "Runtimes"
                text: "Shared storage is the default. Changing this preference affects new installs; existing instances keep their saved Java component."
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                color: Theme.textSecondary
            }

            RowLayout {
                visible: settingsTab === "Runtimes"
                Layout.fillWidth: true
                Label {
                    text: "Installed runtimes"
                    font.family: "Outfit"
                    font.pixelSize: 20
                    color: Theme.textPrimary
                }
                Item { Layout.fillWidth: true }
            }

            ListView {
                visible: settingsTab === "Runtimes"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                model: LauncherController.installedRuntimes
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: Math.max(74, 54 + Math.min(modelData.users.length, 2) * 18)
                    radius: Theme.radiusMedium
                    color: ThemeSettings.theme === "dark" ? "#991e293b" : "#ccffffff"
                    border.color: ThemeSettings.theme === "dark" ? "#233349" : "#dfe5f3"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 14
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Label {
                                Layout.fillWidth: true
                                text: modelData.vendor + " Java " + modelData.version
                                font.family: "Outfit"
                                font.pixelSize: 17
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.users.length > 0
                                      ? "Used by: " + modelData.users.join(", ")
                                      : "Not currently used"
                                font.family: "DM Sans"
                                font.pixelSize: 12
                                color: Theme.textSecondary
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }
                        }
                        StrataButton {
                            text: "Review delete"
                            iconName: "delete"
                            onClicked: {
                                root.pendingRuntimeDeletion = modelData
                                runtimeDeletePopup.open()
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                StrataButton {
                    text: "Cancel"
                    iconName: "close"
                    objectName: "launcherSettingsCancelButton"
                    onClicked: root.closeLauncherSettings()
                }
                StrataButton {
                    text: "Save"
                    iconName: "save"
                    objectName: "launcherSettingsSaveButton"
                    onClicked: root.saveLauncherSettings()
                }
            }
        }
    }
    }

    Rectangle {
        id: accountFlyout
        objectName: "accountFlyout"
        visible: accountFlyoutVisible
        anchors.centerIn: parent
        width: Math.min(parent.width - 80, 620)
        height: Math.min(parent.height - 80, 560)
        radius: Theme.radiusLarge
        color: ThemeSettings.theme === "dark" ? "#111b2a" : "#ffffff"
        border.color: ThemeSettings.theme === "dark" ? "#1d2c3d" : "#dfe6f5"
        z: 34

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "Accounts"
                    font.family: "Outfit"
                    font.pixelSize: 28
                    color: Theme.textPrimary
                }
                Item { Layout.fillWidth: true }
                StrataButton {
                    text: "Close"
                    iconName: "close"
                    onClicked: root.accountFlyoutVisible = false
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                clip: true
                spacing: 6
                model: LauncherController.accounts
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 48
                    radius: Theme.radiusSmall
                    color: ThemeSettings.theme === "dark" ? "#991e293b" : "#ccffffff"
                    border.color: ThemeSettings.theme === "dark" ? "#233349" : "#dfe5f3"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        Label {
                            text: modelData.name + " (" + modelData.type + ")" +
                                  (modelData.active ? " - Active" : "")
                            Layout.fillWidth: true
                            color: Theme.textPrimary
                        }
                        StrataButton {
                            text: "Use"
                            iconName: "secure"
                            enabled: !modelData.active
                            onClicked: LauncherController.selectAccount(modelData.accountId)
                        }
                        StrataButton {
                            text: "Remove"
                            iconName: "delete"
                            onClicked: root.requestAccountRemoval(modelData.accountId)
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                StrataTextField {
                    id: offlineNameInput
                    Layout.fillWidth: true
                    placeholderText: "Offline name (1-16 letters, numbers, underscores)"
                    text: LauncherController.offlineUsername
                    maximumLength: 16
                }
                StrataButton {
                    text: "Add offline"
                    iconName: "account-add"
                    onClicked: LauncherController.addOfflineAccount(offlineNameInput.text)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "Microsoft app client ID"
                    color: Theme.textPrimary
                }
                StrataTextField {
                    id: accountClientIdInput
                    Layout.fillWidth: true
                    placeholderText: "Azure public-client application ID"
                    text: LauncherController.microsoftClientId
                    onEditingFinished: LauncherController.setMicrosoftClientId(text)
                }
            }
            Label {
                text: "Register a public-client app with device-code flow enabled in Microsoft Entra ID."
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                color: Theme.textSecondary
            }
            RowLayout {
                Layout.fillWidth: true
                StrataButton {
                    text: LauncherController.microsoftAuthInProgress ? "Waiting for Microsoft..." : "Sign in with Microsoft"
                    iconName: "account-add"
                    enabled: !LauncherController.microsoftAuthInProgress
                    onClicked: LauncherController.signInMicrosoft(accountClientIdInput.text)
                }
                StrataButton {
                    text: "Cancel"
                    visible: LauncherController.microsoftAuthInProgress
                    onClicked: LauncherController.cancelMicrosoftSignIn()
                }
            }
            Label {
                visible: LauncherController.microsoftUserCode.length > 0
                text: "Visit " + LauncherController.microsoftVerificationUrl + " and enter code: " + LauncherController.microsoftUserCode
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                color: ThemeSettings.theme === "dark" ? "#9ed8ff" : "#1d4ed8"
            }

        }

        Rectangle {
            objectName: "accountDeleteConfirmationPanel"
            visible: pendingRemoveAccountId.length > 0
            anchors.centerIn: parent
            width: parent.width - 80
            height: 170
            radius: Theme.radiusMedium
            color: ThemeSettings.theme === "dark" ? "#182638" : "#ffffff"
            border.color: ThemeSettings.theme === "dark" ? "#41658a" : "#a8bfdc"
            z: 2

            onVisibleChanged: {
                if (visible)
                    Qt.callLater(function() { accountDeleteConfirmation.forceActiveFocus() })
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10
                Label {
                    text: "Type DELETE to remove this account"
                    Layout.fillWidth: true
                    color: Theme.textPrimary
                }
                StrataTextField {
                    id: accountDeleteConfirmation
                    objectName: "accountDeleteConfirmation"
                    Layout.fillWidth: true
                    placeholderText: "DELETE"
                    ContextMenu.menu: null
                    Keys.priority: Keys.BeforeItem
                    Keys.onPressed: function(event) {
                        const pasteModifier = (event.modifiers & Qt.ControlModifier) ||
                                (event.modifiers & Qt.MetaModifier)
                        const pasteShortcut = (event.key === Qt.Key_V && pasteModifier) ||
                                (event.key === Qt.Key_Insert &&
                                 (event.modifiers & Qt.ShiftModifier))
                        if (pasteShortcut)
                            event.accepted = true
                    }
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton | Qt.MiddleButton
                        onPressed: function(mouse) { mouse.accepted = true }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    StrataButton {
                        text: "Keep account"
                        onClicked: root.pendingRemoveAccountId = ""
                    }
                    StrataButton {
                        text: "Confirm removal"
                        enabled: accountDeleteConfirmation.text === "DELETE"
                        onClicked: {
                            LauncherController.removeAccount(root.pendingRemoveAccountId)
                            root.pendingRemoveAccountId = ""
                            accountDeleteConfirmation.clear()
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
        width: Math.min(parent.width * 0.45, Math.max(280, toastText.implicitWidth + 36))
        height: Math.max(56, toastText.implicitHeight + 24)
        radius: Theme.radiusLarge
        color: ThemeSettings.theme === "dark" ? "#182a3e" : "#ffffff"
        border.color: ThemeSettings.theme === "dark" ? "#293d58" : "#e2e8f0"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        z: 40

        Text {
            id: toastText
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            anchors.topMargin: 12
            anchors.bottomMargin: 12
            text: "Launching …"
            color: Theme.textPrimary
            font.family: "DM Sans"
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
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
