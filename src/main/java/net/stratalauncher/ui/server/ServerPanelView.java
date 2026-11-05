package net.stratalauncher.ui.server;

import javafx.application.Platform;
import javafx.animation.FadeTransition;
import javafx.animation.ParallelTransition;
import javafx.animation.TranslateTransition;
import javafx.animation.KeyFrame;
import javafx.animation.KeyValue;
import javafx.animation.Timeline;
import javafx.collections.FXCollections;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.Button;
import javafx.scene.control.CheckBox;
import javafx.scene.control.ComboBox;
import javafx.scene.control.Label;
import javafx.scene.control.ListCell;
import javafx.scene.control.ListView;
import javafx.scene.Node;
import javafx.scene.control.ProgressBar;
import javafx.scene.control.Separator;
import javafx.scene.control.ScrollPane;
import javafx.scene.control.Spinner;
import javafx.scene.control.Tab;
import javafx.scene.control.TabPane;
import javafx.scene.control.TextArea;
import javafx.scene.control.TextField;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;
import javafx.scene.input.MouseEvent;
import javafx.scene.transform.Scale;
import net.stratalauncher.core.auth.LauncherSettings;
import net.stratalauncher.core.modrinth.ModrinthClient;
import net.stratalauncher.core.server.DevServerManager;
import net.stratalauncher.core.server.ServerDistribution;
import net.stratalauncher.core.server.ServerModrinthManager;
import net.stratalauncher.core.mojang.MojangManifestClient;
import net.stratalauncher.ui.FlyoutManager;
import net.stratalauncher.ui.FrameTimeOverlay;
import net.stratalauncher.ui.IconAssets;
import net.stratalauncher.ui.ToggleSwitchStyle;
import net.stratalauncher.ui.MotionConstants;
import net.stratalauncher.ui.animations.PageTransition;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.atomic.AtomicLong;

public final class ServerPanelView extends BorderPane {
    private static final int PAGE_SIZE = 20;
    private static final double SELECTED_SERVER_LIST_WIDTH = 310;

    private final DevServerManager serverManager;
    private final ServerModrinthManager modrinthManager;
    private final MojangManifestClient manifestClient;
    private final FlyoutManager flyouts;
    private final LauncherSettings launcherSettings;
    private final ListView<DevServerManager.ServerConfig> servers = new ListView<>();
    private VBox serverSidebar;
    private HBox serverContentLayout;
    private TabPane detailTabs;
    private ParallelTransition detailTransition;
    private final Label status = new Label("Create a server to get started.");
    private final ProgressBar progress = new ProgressBar();
    private final TextField nameField = new TextField();
    private final Label distributionLabel = new Label();
    private final Label serverLocation = new Label();
    private final Spinner<Integer> viewDistance = new Spinner<>(2, 32, 10);
    private final CheckBox onlineMode = new CheckBox("Require players to sign in with a Minecraft account");
    private final CheckBox managedJavaRuntime = new CheckBox("Use a server-specific managed Java runtime");
    private final Button installServerRuntimeButton = new Button("Install Java for this server");
    private final Label javaRuntimeStatus = new Label();
    private final TextArea javaFlagsField = new TextArea();
    private final TextArea serverPropertiesEditor = new TextArea();
    private final Button savePropertiesButton = new Button("Save properties");
    private final Button saveSettingsButton = new Button("Save settings");
    private final Button installButton = new Button("Install / Repair");
    private final Button startButton = new Button("Start");
    private final Button stopButton = new Button("Stop");
    private final Button deleteServerButton = new Button("Delete server");
    private final ServerConsoleView consoleView;
    private final Label detailsName = new Label("—");
    private final Label detailsId = new Label("—");
    private final Label detailsVersion = new Label("—");
    private final Label detailsDistribution = new Label("—");
    private final Label detailsCreated = new Label("—");
    private final Label detailsLocation = new Label("—");
    private final Label detailsInstallation = new Label("—");
    private final Label detailsRuntime = new Label("—");
    private final Label detailsNetwork = new Label("—");
    private final Label detailsJavaFlags = new Label("—");
    private DevServerManager.ServerConfig selectedServer;
    private final TextField searchField = new TextField();
    private final ListView<ModrinthClient.Project> projects = new ListView<>();
    private final ComboBox<ModrinthClient.ProjectVersion> versions = new ComboBox<>();
    private final ListView<DevServerManager.InstalledServerContent> installed = new ListView<>();
    private final Label contentStatus = new Label("Search Modrinth for server content.");
    private final AtomicLong searchSequence = new AtomicLong();
    private final AtomicLong versionSequence = new AtomicLong();
    private int searchOffset;
    private Runnable refreshHandler = this::refresh;

    public ServerPanelView(DevServerManager serverManager, ServerModrinthManager modrinthManager,
                           MojangManifestClient manifestClient, FlyoutManager flyouts,
                           LauncherSettings launcherSettings) {
        this.serverManager = serverManager;
        this.modrinthManager = modrinthManager;
        this.manifestClient = manifestClient;
        this.flyouts = flyouts;
        this.launcherSettings = launcherSettings;
        ToggleSwitchStyle.apply(onlineMode);
        ToggleSwitchStyle.apply(managedJavaRuntime);
        onlineMode.selectedProperty().addListener((observable, oldValue, selected) ->
                ToggleSwitchStyle.setIcon(onlineMode, selected ? "secure" : "secure-off",
                        launcherSettings.darkMode()));
        ToggleSwitchStyle.setIcon(onlineMode, onlineMode.isSelected() ? "secure" : "secure-off",
                launcherSettings.darkMode());
        consoleView = new ServerConsoleView(serverManager, launcherSettings, this::startServer,
                this::stopServer, this::confirmTerminateServer);
        consoleView.setSnapshotListener(snapshot -> {
            if (isSelectedId(snapshot.serverId())) {
                servers.refresh();
                startButton.setDisable(snapshot.running());
                stopButton.setDisable(!snapshot.running());
                deleteServerButton.setDisable(snapshot.running());
                status.setText(snapshot.running() ? "Server is running." : "Server stopped.");
            }
        });
        getStyleClass().add("instances-view");
        setPadding(new Insets(10));
        setMaxHeight(Double.MAX_VALUE);
        setTop(createHeader());
        serverSidebar = createServerSidebar();
        detailTabs = createDetails();
        serverSidebar.setPrefWidth(900);
        serverSidebar.setMinWidth(0);
        serverSidebar.setMaxWidth(Double.MAX_VALUE);
        HBox.setHgrow(serverSidebar, Priority.ALWAYS);
        HBox.setHgrow(detailTabs, Priority.ALWAYS);
        detailTabs.setMaxWidth(Double.MAX_VALUE);
        serverContentLayout = new HBox(12, serverSidebar, detailTabs);
        serverContentLayout.setFillHeight(true);
        setCenter(serverContentLayout);
        PageTransition.attachTabTransitions(detailTabs);
        servers.getSelectionModel().selectedItemProperty().addListener((observable, oldValue, selected) ->
                showServer(selected));
        servers.addEventFilter(MouseEvent.MOUSE_CLICKED, this::deselectWhenContainerClicked);
    }

    private void installServerRuntime() {
        DevServerManager.ServerConfig selected = servers.getSelectionModel().getSelectedItem();
        if (selected == null) {
            return;
        }
        installServerRuntimeButton.setDisable(true);
        javaRuntimeStatus.setText("Preparing Java for Minecraft " + selected.minecraftVersion() + "...");
        Thread.ofVirtual().start(() -> {
            try {
                DevServerManager.ServerConfig updated = serverManager.installManagedJavaRuntime(selected,
                        update -> Platform.runLater(() -> {
                            if (isSelectedId(selected.id())) {
                                javaRuntimeStatus.setText(update.currentItem());
                            }
                        }));
                Platform.runLater(() -> {
                    if (isSelectedId(selected.id())) {
                        replaceSelected(updated);
                        javaRuntimeStatus.setText("Managed Java runtime is installed and selected for this server.");
                    }
                    installServerRuntimeButton.setDisable(false);
                });
            } catch (IOException | InterruptedException | IllegalArgumentException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                Platform.runLater(() -> {
                    if (isSelectedId(selected.id())) {
                        javaRuntimeStatus.setText("Could not install managed Java runtime: "
                                + exception.getMessage());
                    }
                    installServerRuntimeButton.setDisable(false);
                });
            }
        });
    }

    public void initialize(List<DevServerManager.ServerConfig> initialServers) {
        servers.setItems(FXCollections.observableArrayList(initialServers));
        servers.getSelectionModel().clearSelection();
        showServer(null);
    }

    public void setRefreshHandler(Runnable handler) {
        refreshHandler = handler == null ? this::refresh : handler;
    }

    public void refresh() {
        Thread.ofVirtual().start(() -> {
            try {
                List<DevServerManager.ServerConfig> available = serverManager.listServers();
                Platform.runLater(() -> {
                    String selectedId = servers.getSelectionModel().getSelectedItem() == null
                            ? null : servers.getSelectionModel().getSelectedItem().id();
                    servers.setItems(FXCollections.observableArrayList(available));
                    DevServerManager.ServerConfig selected = selectedId == null ? null : available.stream()
                            .filter(server -> server.id().equals(selectedId)).findFirst().orElse(null);
                    servers.getSelectionModel().select(selected);
                });
            } catch (IOException exception) {
                showStatus("Could not load servers: " + exception.getMessage());
            }
        });
    }

    private VBox createHeader() {
        Label title = new Label("Local servers");
        title.getStyleClass().add("hero-title");
        Label description = new Label("Install and run Vanilla, Fabric, or Paper servers locally.");
        description.getStyleClass().add("hero-subtitle");
        description.setWrapText(true);
        Button create = new Button("Create server");
        IconAssets.apply(create, "add", launcherSettings.darkMode());
        create.getStyleClass().add("primary-button");
        create.setOnAction(event -> showCreateDialog());
        HBox heading = new HBox(12, title, create);
        heading.setAlignment(Pos.CENTER_LEFT);
        VBox header = new VBox(8, heading, description);
        header.setPadding(new Insets(8, 8, 16, 8));
        return header;
    }

    private VBox createServerSidebar() {
        servers.setPrefWidth(SELECTED_SERVER_LIST_WIDTH);
        servers.getStyleClass().add("card-list");
        servers.setPlaceholder(new Label("No servers yet."));
        servers.setCellFactory(ignored -> new ListCell<>() {
            private final Label name = new Label();
            private final Label details = new Label();
            private final VBox card = new VBox(8, name, details);
            private final Scale scale = new Scale(1, 1);
            private final javafx.animation.ScaleTransition hoverTransition =
                    new javafx.animation.ScaleTransition(MotionConstants.FAST, card);

            {
                card.getStyleClass().addAll("instance-card", "server-card");
                card.getTransforms().add(scale);
                card.setMaxWidth(Double.MAX_VALUE);
                card.prefWidthProperty().bind(widthProperty().subtract(24));
                scale.setX(0.995);
                scale.setY(0.995);
                hoverTransition.setInterpolator(MotionConstants.STANDARD);
                card.setPadding(new Insets(12));
                name.getStyleClass().add("instance-card-name");
                details.getStyleClass().add("instance-card-version");
                card.layoutBoundsProperty().addListener((observable, oldBounds, bounds) -> {
                    scale.setPivotX(bounds.getWidth() / 2);
                    scale.setPivotY(bounds.getHeight() / 2);
                });
                card.setOnMouseEntered(event -> animateHover(true));
                card.setOnMouseExited(event -> animateHover(false));
                selectedProperty().addListener((observable, oldValue, selected) ->
                        updateSelection(selected && getItem() != null));
                servers.getSelectionModel().selectedItemProperty().addListener((observable, oldValue, selected) ->
                        updateSelection(getItem() != null && selected != null
                                && getItem().id().equals(selected.id())));
            }

            @Override
            protected void updateItem(DevServerManager.ServerConfig server, boolean empty) {
                super.updateItem(server, empty);
                if (empty || server == null) {
                    card.getStyleClass().remove("instance-card-hovered");
                    animateHover(false);
                    setText(null);
                    setGraphic(null);
                    return;
                }
                name.setText(server.name());
                details.setWrapText(true);
                boolean running = serverManager.snapshot(server.id())
                        .map(DevServerManager.ServerSnapshot::running).orElse(false);
                details.setText(server.distribution() + " · Minecraft " + server.minecraftVersion()
                        + (running ? " · Running" : server.installed() ? " · Installed" : " · Not installed"));
                updateSelection(servers.getSelectionModel().getSelectedItem() != null
                        && server.id().equals(servers.getSelectionModel().getSelectedItem().id()));
                setText(null);
                setGraphic(card);
            }

            private void updateSelection(boolean selected) {
                card.getStyleClass().remove("instance-card-selected");
                if (selected) {
                    card.getStyleClass().add("instance-card-selected");
                }
            }

            private void animateHover(boolean hovered) {
                if (hovered && !MotionConstants.reduceMotion) {
                    FrameTimeOverlay.measure("Card hover", MotionConstants.FAST);
                }
                if (hovered) {
                    if (!card.getStyleClass().contains("instance-card-hovered")) {
                        card.getStyleClass().add("instance-card-hovered");
                    }
                } else {
                    card.getStyleClass().remove("instance-card-hovered");
                }
                if (MotionConstants.reduceMotion) {
                    scale.setX(0.995);
                    scale.setY(0.995);
                    return;
                }
                hoverTransition.stop();
                double target = hovered ? 1 : 0.995;
                hoverTransition.setToX(target);
                hoverTransition.setToY(target);
                hoverTransition.playFromStart();
            }
        });
        Button reload = new Button("Refresh");
        IconAssets.apply(reload, "refresh", launcherSettings.darkMode());
        reload.setMaxWidth(Double.MAX_VALUE);
        reload.setOnAction(event -> refreshHandler.run());
        deleteServerButton.setMaxWidth(Double.MAX_VALUE);
        deleteServerButton.getStyleClass().add("danger-button");
        IconAssets.apply(deleteServerButton, "delete", launcherSettings.darkMode());
        deleteServerButton.setOnAction(event -> deleteSelectedServer());
        VBox sidebar = new VBox(10, new Label("Your servers"), servers, reload, deleteServerButton);
        sidebar.setPadding(new Insets(8, 14, 8, 8));
        VBox.setVgrow(servers, Priority.ALWAYS);
        return sidebar;
    }

    private void deselectWhenContainerClicked(MouseEvent event) {
        if (!(event.getTarget() instanceof Node target)) {
            return;
        }
        Node current = target;
        while (current != null && current != servers) {
            if (current.getStyleClass().contains("server-card")) {
                return;
            }
            current = current.getParent();
        }
        servers.getSelectionModel().clearSelection();
    }

    private TabPane createDetails() {
        Tab settings = new Tab("Settings", createSettingsPanel());
        settings.setGraphic(IconAssets.graphic("settings", launcherSettings.darkMode()));
        settings.setClosable(false);
        Tab console = new Tab("Console", consoleView);
        console.setGraphic(IconAssets.graphic("console", launcherSettings.darkMode()));
        console.setClosable(false);
        Tab content = new Tab("Modrinth content", createContentPanel());
        content.setGraphic(IconAssets.graphic("download", launcherSettings.darkMode()));
        content.setClosable(false);
        detailTabs = new TabPane(settings, console, content);
        detailTabs.setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);
        detailTabs.setMinWidth(0);
        detailTabs.setPrefWidth(560);
        detailTabs.setOpacity(0);
        detailTabs.setVisible(false);
        detailTabs.setManaged(false);
        return detailTabs;
    }

    private VBox createSettingsPanel() {
        distributionLabel.setId("server-distribution");
        javaFlagsField.setPromptText("-Xms2G\n-Xmx4G");
        javaFlagsField.setPrefRowCount(6);
        Label javaHelp = new Label("Advanced settings. Unsupported options can stop the server from starting or "
                + "cause instability; most server owners should leave these unchanged.");
        javaHelp.setWrapText(true);
        serverLocation.setWrapText(true);
        viewDistance.setEditable(false);
        viewDistance.setMaxWidth(Double.MAX_VALUE);
        javaRuntimeStatus.setWrapText(true);
        installServerRuntimeButton.getStyleClass().add("action-button");
        IconAssets.apply(installServerRuntimeButton, "runtime", launcherSettings.darkMode());
        installServerRuntimeButton.setOnAction(event -> installServerRuntime());
        saveSettingsButton.getStyleClass().add("action-button");
        IconAssets.apply(saveSettingsButton, "save", launcherSettings.darkMode());
        saveSettingsButton.setOnAction(event -> saveSettings());
        installButton.getStyleClass().add("action-button");
        installButton.getStyleClass().add("danger-button");
        IconAssets.apply(installButton, "download", launcherSettings.darkMode());
        installButton.setOnAction(event -> confirmInstallServer());
        startButton.getStyleClass().add("primary-button");
        IconAssets.apply(startButton, "start", launcherSettings.darkMode());
        startButton.setOnAction(event -> startServer());
        stopButton.getStyleClass().add("action-button");
        IconAssets.apply(stopButton, "stop", launcherSettings.darkMode());
        stopButton.setOnAction(event -> stopServer());
        HBox actions = new HBox(8, installButton, startButton, stopButton, saveSettingsButton);
        actions.setAlignment(Pos.CENTER_LEFT);
        progress.setMaxWidth(Double.MAX_VALUE);
        progress.setVisible(false);
        progress.setManaged(false);
        status.setWrapText(true);
        serverPropertiesEditor.setPromptText("server.properties");
        serverPropertiesEditor.setWrapText(false);
        serverPropertiesEditor.setPrefRowCount(18);
        VBox.setVgrow(serverPropertiesEditor, Priority.ALWAYS);
        savePropertiesButton.getStyleClass().add("primary-button");
        IconAssets.apply(savePropertiesButton, "save", launcherSettings.darkMode());
        savePropertiesButton.setOnAction(event -> saveServerProperties());
        Tab management = new Tab("Management", createManagementPanel(javaHelp, actions));
        Tab properties = new Tab("Properties", new VBox(10,
                new Label("Edit server.properties"),
                new Label("Changes apply the next time this server starts. Stop the server before saving."),
                serverPropertiesEditor, savePropertiesButton));
        Tab details = new Tab("Details", createDetailsPanel());
        for (Tab tab : List.of(management, properties, details)) {
            tab.setClosable(false);
        }
        TabPane settingsTabs = new TabPane(management, properties, details);
        settingsTabs.setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);
        settingsTabs.setMinHeight(0);
        VBox.setVgrow(settingsTabs, Priority.ALWAYS);
        VBox panel = new VBox(settingsTabs);
        panel.setPadding(new Insets(8));
        VBox.setVgrow(panel, Priority.ALWAYS);
        return panel;
    }

    private VBox createManagementPanel(Label javaHelp, HBox actions) {
        VBox.setVgrow(javaFlagsField, Priority.SOMETIMES);
        VBox panel = new VBox(12,
                new Label("Server name"), nameField,
                new Label("Distribution and Minecraft version"), distributionLabel,
                managedJavaRuntime, installServerRuntimeButton, javaRuntimeStatus,
                new Label("Java launch flags"), javaFlagsField, javaHelp,
                new Separator(), installButton, actions, progress, status);
        panel.setPadding(new Insets(14));
        panel.setFillWidth(true);
        return panel;
    }

    private VBox createDetailsPanel() {
        VBox groups = new VBox(14,
                detailGroup("SERVER", detailRow("Name", detailsName), detailRow("Server ID", detailsId),
                        detailRow("Location", detailsLocation)),
                detailGroup("GAME", detailRow("Distribution", detailsDistribution),
                        detailRow("Minecraft version", detailsVersion),
                        detailRow("Installation", detailsInstallation),
                        detailRow("Created", detailsCreated)),
                detailGroup("RUNTIME AND NETWORK", detailRow("Java runtime", detailsRuntime),
                        detailRow("Online mode / view distance", detailsNetwork),
                        detailRow("Java launch flags", detailsJavaFlags)));
        groups.setPadding(new Insets(14));
        ScrollPane scroll = new ScrollPane(groups);
        scroll.setFitToWidth(true);
        scroll.setPannable(true);
        VBox.setVgrow(scroll, Priority.ALWAYS);
        return new VBox(scroll);
    }

    private static VBox detailGroup(String title, javafx.scene.Node... rows) {
        Label heading = new Label(title);
        heading.getStyleClass().add("detail-group-heading");
        VBox group = new VBox(8);
        group.getStyleClass().add("server-detail-group");
        group.getChildren().add(heading);
        group.getChildren().add(new Separator());
        group.getChildren().addAll(rows);
        return group;
    }

    private static HBox detailRow(String title, Label value) {
        Label heading = new Label(title);
        heading.getStyleClass().add("detail-row-label");
        value.setWrapText(true);
        HBox row = new HBox(14, heading, value);
        HBox.setHgrow(value, Priority.ALWAYS);
        return row;
    }

    private VBox createContentPanel() {
        Label explanation = new Label("Fabric servers can use Modrinth mods. Paper servers can use plugins.");
        explanation.setWrapText(true);
        searchField.setPromptText("Search Modrinth");
        searchField.setOnAction(event -> searchContent());
        Button search = new Button("Search");
        IconAssets.apply(search, "refresh", launcherSettings.darkMode());
        search.setOnAction(event -> searchContent());
        Button previous = new Button("Previous");
        previous.setOnAction(event -> {
            if (searchOffset > 0) {
                searchOffset = Math.max(0, searchOffset - PAGE_SIZE);
                searchContent();
            }
        });
        Button next = new Button("Next");
        next.setOnAction(event -> {
            searchOffset += PAGE_SIZE;
            searchContent();
        });
        HBox searchRow = new HBox(8, searchField, search, previous, next);
        HBox.setHgrow(searchField, Priority.ALWAYS);
        projects.setPlaceholder(new Label("Search compatible Modrinth projects."));
        projects.setCellFactory(ignored -> new ListCell<>() {
            @Override
            protected void updateItem(ModrinthClient.Project project, boolean empty) {
                super.updateItem(project, empty);
                setText(empty || project == null ? null
                        : project.title() + "\n" + project.description()
                        + "\n" + String.format(java.util.Locale.ROOT, "%,d downloads", project.downloads()));
            }
        });
        projects.getSelectionModel().selectedItemProperty().addListener((observable, oldValue, selected) ->
                loadVersions(selected));
        versions.setMaxWidth(Double.MAX_VALUE);
        versions.setPromptText("Select a compatible version");
        versions.setCellFactory(ignored -> new ListCell<>() {
            @Override
            protected void updateItem(ModrinthClient.ProjectVersion version, boolean empty) {
                super.updateItem(version, empty);
                setText(empty || version == null ? null : version.versionNumber());
            }
        });
        versions.setButtonCell(new ListCell<>() {
            @Override
            protected void updateItem(ModrinthClient.ProjectVersion version, boolean empty) {
                super.updateItem(version, empty);
                setText(empty || version == null ? null : version.versionNumber());
            }
        });
        Button installVersion = new Button("Install selected");
        IconAssets.apply(installVersion, "download", launcherSettings.darkMode());
        installVersion.setOnAction(event -> installContent());
        installed.setPlaceholder(new Label("No server content installed."));
        installed.setCellFactory(ignored -> new ListCell<>() {
            @Override
            protected void updateItem(DevServerManager.InstalledServerContent content, boolean empty) {
                super.updateItem(content, empty);
                setText(empty || content == null ? null : content.filename());
            }
        });
        Button remove = new Button("Remove selected");
        IconAssets.apply(remove, "delete", launcherSettings.darkMode());
        remove.setOnAction(event -> removeContent());
        contentStatus.setWrapText(true);
        VBox browse = new VBox(8, new Label("Available projects"), searchRow, projects, versions, installVersion);
        VBox installedPanel = new VBox(8, new Label("Installed content"), installed, remove);
        VBox.setVgrow(projects, Priority.ALWAYS);
        VBox.setVgrow(installed, Priority.ALWAYS);
        HBox panels = new HBox(12, browse, installedPanel);
        HBox.setHgrow(browse, Priority.ALWAYS);
        HBox.setHgrow(installedPanel, Priority.ALWAYS);
        VBox content = new VBox(10, explanation, panels, contentStatus);
        content.setPadding(new Insets(14));
        VBox.setVgrow(panels, Priority.ALWAYS);
        return content;
    }

    private void showCreateDialog() {
        TextField name = new TextField("My server");
        name.setPromptText("Server name");
        ComboBox<String> version = new ComboBox<>();
        version.setPromptText("Loading Minecraft versions...");
        version.setMaxWidth(Double.MAX_VALUE);
        ComboBox<ServerDistribution> distribution = new ComboBox<>(
                FXCollections.observableArrayList(ServerDistribution.values()));
        distribution.getSelectionModel().select(ServerDistribution.VANILLA);
        VBox fields = new VBox(10, new Label("Name"), name, new Label("Minecraft version"), version,
                new Label("Distribution"), distribution);
        fields.setPadding(new Insets(12));
        flyouts.show("Create local server", "Choose a server distribution and Minecraft version.", fields,
                new FlyoutManager.Action("Cancel", "action-button", () -> true),
                new FlyoutManager.Action("Create", "primary-button", () -> {
            if (version.getValue() == null || distribution.getValue() == null) {
                flyouts.showToast("Choose a Minecraft version and server distribution.",
                        FlyoutManager.ToastKind.WARNING);
                return false;
            }
            try {
                DevServerManager.ServerConfig created = serverManager.create(name.getText(),
                        version.getValue(), distribution.getValue());
                refresh();
                showStatus("Created " + created.name() + ". Accept the EULA and install the server to continue.");
                return true;
            } catch (IOException | IllegalArgumentException exception) {
                showError("Could not create server", exception);
                return false;
            }
        }));
        CompletableFuture.runAsync(() -> {
            try {
                MojangManifestClient.VersionManifest manifest = manifestClient.fetchManifest();
                List<String> versionIds = manifest.versions().stream()
                        .map(MojangManifestClient.Version::id).toList();
                Platform.runLater(() -> {
                    version.setItems(FXCollections.observableArrayList(versionIds));
                    version.setValue(manifest.latestRelease());
                });
            } catch (IOException | InterruptedException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                Platform.runLater(() -> {
                    version.setPromptText("Could not load versions");
                    flyouts.showError("Could not load Minecraft versions", exception.getMessage());
                });
            }
        });
    }

    private void showServer(DevServerManager.ServerConfig selected) {
        boolean available = selected != null;
        selectedServer = selected;
        if (available && !detailTabs.isManaged()) {
            double currentWidth = Math.max(serverSidebar.getWidth(), SELECTED_SERVER_LIST_WIDTH);
            serverSidebar.setPrefWidth(currentWidth);
            detailTabs.setVisible(true);
            detailTabs.setManaged(true);
            HBox.setHgrow(serverSidebar, Priority.NEVER);
            animateDetailsFromRight();
        } else if (!available && detailTabs.isManaged()) {
            searchSequence.incrementAndGet();
            animateDetailsToRight();
        } else if (!available) {
            searchSequence.incrementAndGet();
        }
        nameField.setDisable(!available);
        javaFlagsField.setDisable(!available);
        viewDistance.setDisable(!available);
        onlineMode.setDisable(!available);
        managedJavaRuntime.setDisable(!available);
        installServerRuntimeButton.setDisable(!available);
        saveSettingsButton.setDisable(!available);
        savePropertiesButton.setDisable(!available);
        installButton.setDisable(!available);
        startButton.setDisable(!available);
        stopButton.setDisable(!available);
        deleteServerButton.setDisable(!available);
        if (!available) {
            nameField.clear();
            javaFlagsField.clear();
            managedJavaRuntime.setSelected(false);
            javaRuntimeStatus.setText("");
            serverPropertiesEditor.clear();
            serverPropertiesEditor.setEditable(false);
            detailsName.setText("—");
            detailsId.setText("—");
            detailsVersion.setText("—");
            detailsDistribution.setText("—");
            detailsCreated.setText("—");
            detailsLocation.setText("—");
            detailsInstallation.setText("—");
            detailsRuntime.setText("—");
            detailsNetwork.setText("—");
            detailsJavaFlags.setText("—");
            status.setText("Create a server to get started.");
            consoleView.showServer(null);
            installed.getItems().clear();
            projects.getItems().clear();
            versions.getItems().clear();
            return;
        }
        nameField.setText(selected.name());
        javaFlagsField.setText(String.join("\n", selected.javaFlags()));
        viewDistance.getValueFactory().setValue(selected.viewDistance());
        onlineMode.setSelected(selected.onlineMode());
        managedJavaRuntime.setSelected(selected.useManagedJavaRuntime());
        javaRuntimeStatus.setText(selected.javaRuntimeComponent() == null
                ? "This server currently uses System Java."
                : "Managed Java runtime: " + selected.javaRuntimeComponent());
        distributionLabel.setText(selected.distribution() + " · Minecraft " + selected.minecraftVersion());
        detailsName.setText(selected.name());
        detailsId.setText(selected.id());
        detailsVersion.setText(selected.minecraftVersion());
        detailsDistribution.setText(selected.distribution().toString());
        detailsCreated.setText(selected.createdAt());
        detailsInstallation.setText(selected.installed() ? "Installed" : "Not installed");
        detailsRuntime.setText(selected.useManagedJavaRuntime()
                ? "Managed Java · " + selected.javaRuntimeComponent() : "System Java");
        detailsNetwork.setText((selected.onlineMode() ? "Online" : "Offline")
                + " · view distance " + selected.viewDistance() + " chunks");
        detailsJavaFlags.setText(selected.javaFlags().isEmpty()
                ? "No custom flags" : String.join(" ", selected.javaFlags()));
        try {
            serverLocation.setText(serverManager.serverDirectoryPath(selected).toString());
            detailsLocation.setText(serverLocation.getText());
            serverPropertiesEditor.setText(serverManager.readServerProperties(selected));
        } catch (IOException exception) {
            serverLocation.setText("Could not read server folder: " + exception.getMessage());
            detailsLocation.setText(serverLocation.getText());
            serverPropertiesEditor.clear();
        }
        boolean running = serverManager.snapshot(selected.id()).map(DevServerManager.ServerSnapshot::running)
                .orElse(false);
        startButton.setDisable(running);
        stopButton.setDisable(!running);
        deleteServerButton.setDisable(running);
        serverPropertiesEditor.setEditable(!running);
        savePropertiesButton.setDisable(running);
        status.setText(running ? "Server is running." : selected.installed()
                ? "Installed · stopped" : "Not installed · stopped");
        consoleView.showServer(selected);
        refreshInstalledContent(selected);
        searchSequence.incrementAndGet();
        projects.getItems().clear();
        versions.getItems().clear();
        searchField.setDisable(selected.distribution() == ServerDistribution.VANILLA);
        contentStatus.setText(selected.distribution() == ServerDistribution.VANILLA
                ? "Install Fabric to manage mods, or Paper to manage plugins."
                : "Search compatible Modrinth " + (selected.distribution() == ServerDistribution.PAPER
                ? "plugins." : "mods."));
    }

    private void animateDetailsFromRight() {
        if (detailTabs == null) {
            return;
        }
        if (detailTransition != null) {
            detailTransition.stop();
        }
        if (MotionConstants.reduceMotion) {
            detailTabs.setOpacity(1);
            detailTabs.setTranslateX(0);
            serverSidebar.setPrefWidth(SELECTED_SERVER_LIST_WIDTH);
            HBox.setHgrow(serverSidebar, Priority.NEVER);
            HBox.setHgrow(detailTabs, Priority.ALWAYS);
            return;
        }
        double targetDetailWidth = Math.max(0,
                serverContentLayout.getWidth() - serverContentLayout.getSpacing() - SELECTED_SERVER_LIST_WIDTH);
        double startSidebarWidth = Math.max(SELECTED_SERVER_LIST_WIDTH, serverSidebar.getWidth());
        serverSidebar.setPrefWidth(startSidebarWidth);
        detailTabs.setPrefWidth(0);
        HBox.setHgrow(detailTabs, Priority.NEVER);
        FadeTransition fade = new FadeTransition(MotionConstants.NORMAL, detailTabs);
        fade.setFromValue(0);
        fade.setToValue(1);
        fade.setInterpolator(MotionConstants.STANDARD);
        TranslateTransition translate = new TranslateTransition(MotionConstants.NORMAL, detailTabs);
        translate.setFromX(28);
        translate.setToX(0);
        translate.setInterpolator(MotionConstants.EMPHASIS);
        Timeline width = new Timeline(new KeyFrame(MotionConstants.NORMAL,
                new KeyValue(serverSidebar.prefWidthProperty(), SELECTED_SERVER_LIST_WIDTH, MotionConstants.EMPHASIS),
                new KeyValue(detailTabs.prefWidthProperty(), targetDetailWidth, MotionConstants.EMPHASIS)));
        detailTransition = new ParallelTransition(fade, translate, width);
        detailTabs.setVisible(true);
        detailTabs.setManaged(true);
        detailTransition.setOnFinished(event -> {
            HBox.setHgrow(detailTabs, Priority.ALWAYS);
            detailTabs.setPrefWidth(560);
        });
        detailTransition.playFromStart();
    }

    private void animateDetailsToRight() {
        if (detailTabs == null || MotionConstants.reduceMotion) {
            if (detailTabs != null) {
                detailTabs.setOpacity(0);
                detailTabs.setTranslateX(0);
                detailTabs.setVisible(false);
                detailTabs.setManaged(false);
            }
            HBox.setHgrow(serverSidebar, Priority.ALWAYS);
            HBox.setHgrow(detailTabs, Priority.ALWAYS);
            serverSidebar.setPrefWidth(900);
            detailTabs.setPrefWidth(560);
            return;
        }
        if (detailTransition != null) {
            detailTransition.stop();
        }
        FadeTransition fade = new FadeTransition(MotionConstants.NORMAL, detailTabs);
        fade.setFromValue(detailTabs.getOpacity());
        fade.setToValue(0);
        fade.setInterpolator(MotionConstants.STANDARD);
        TranslateTransition translate = new TranslateTransition(MotionConstants.NORMAL, detailTabs);
        translate.setFromX(detailTabs.getTranslateX());
        translate.setToX(28);
        translate.setInterpolator(MotionConstants.EMPHASIS);
        double targetWidth = Math.max(0, serverContentLayout.getWidth() - serverContentLayout.getSpacing());
        HBox.setHgrow(detailTabs, Priority.NEVER);
        HBox.setHgrow(serverSidebar, Priority.NEVER);
        Timeline width = new Timeline(new KeyFrame(MotionConstants.NORMAL,
                new KeyValue(serverSidebar.prefWidthProperty(), targetWidth, MotionConstants.EMPHASIS),
                new KeyValue(detailTabs.prefWidthProperty(), 0, MotionConstants.EMPHASIS)));
        detailTransition = new ParallelTransition(fade, translate, width);
        detailTransition.setOnFinished(event -> {
            detailTabs.setVisible(false);
            detailTabs.setManaged(false);
            detailTabs.setTranslateX(0);
            HBox.setHgrow(serverSidebar, Priority.ALWAYS);
            HBox.setHgrow(detailTabs, Priority.ALWAYS);
            serverSidebar.setPrefWidth(900);
            detailTabs.setPrefWidth(560);
        });
        detailTransition.playFromStart();
    }

    private void saveSettings() {
        DevServerManager.ServerConfig selected = servers.getSelectionModel().getSelectedItem();
        if (selected == null) {
            return;
        }
        try {
            DevServerManager.ServerConfig updated = serverManager.updateSettings(selected, nameField.getText(),
                    selected.eulaAccepted(), javaFlags(javaFlagsField.getText()), viewDistance.getValue(),
                    onlineMode.isSelected(), managedJavaRuntime.isSelected());
            replaceSelected(updated);
            serverPropertiesEditor.setText(serverManager.readServerProperties(updated));
            showStatus("Server settings saved.");
        } catch (IOException | IllegalArgumentException exception) {
            showError("Could not save server settings", exception);
        }
    }

    private void saveServerProperties() {
        if (selectedServer == null) {
            return;
        }
        try {
            DevServerManager.ServerConfig updated = serverManager.saveServerProperties(
                    selectedServer, serverPropertiesEditor.getText());
            replaceSelected(updated);
            showStatus("server.properties saved.");
        } catch (IOException | IllegalArgumentException exception) {
            showError("Could not save server.properties", exception);
        }
    }

    private void installServer() {
        DevServerManager.ServerConfig selected = servers.getSelectionModel().getSelectedItem();
        if (selected == null) {
            return;
        }
        setBusy(true, "Installing " + selected.distribution() + " server...");
        Thread.ofVirtual().start(() -> {
            try {
                serverManager.install(selected, update -> Platform.runLater(() -> {
                    progress.setProgress(update.fraction());
                    status.setText(update.currentItem());
                }));
                DevServerManager.ServerConfig installedServer = findServer(selected.id());
                Platform.runLater(() -> {
                    setBusy(false, "Installation complete. Accept the EULA in settings to start the server.");
                    if (installedServer != null) {
                        replaceSelected(installedServer);
                    }
                });
            } catch (IOException | InterruptedException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                Platform.runLater(() -> setBusy(false, "Server installation failed: " + exception.getMessage()));
            }
        });
    }

    private void confirmInstallServer() {
        DevServerManager.ServerConfig selected = servers.getSelectionModel().getSelectedItem();
        if (selected == null) {
            return;
        }
        Label warning = new Label("Install or repair " + selected.name() + " (" + selected.distribution()
                + ", Minecraft " + selected.minecraftVersion() + ")? Existing server files may be replaced. "
                + "Back up worlds and configuration before repairing.");
        warning.setWrapText(true);
        flyouts.show("Install / Repair server", "", warning,
                new FlyoutManager.Action("Cancel", "action-button", () -> true),
                new FlyoutManager.Action("Install / Repair", "danger-button", () -> {
                    installServer();
                    return true;
                }));
    }

    private void startServer() {
        DevServerManager.ServerConfig selected = servers.getSelectionModel().getSelectedItem();
        if (selected == null) {
            return;
        }
        if (!selected.installed()) {
            flyouts.showToast("Install the server before starting it.", FlyoutManager.ToastKind.WARNING);
            return;
        }
        if (!selected.eulaAccepted()) {
            Label notice = new Label("Starting this server requires accepting the Minecraft End User License "
                    + "Agreement at minecraft.net/eula. Accepting will save your decision for this server.");
            notice.setWrapText(true);
            flyouts.show("Accept the Minecraft EULA", "", notice,
                    new FlyoutManager.Action("Decline", "action-button", () -> {
                        status.setText("Server start cancelled because the EULA was declined.");
                        return true;
                    }),
                    new FlyoutManager.Action("Accept EULA", "primary-button", () -> {
                        acceptEulaAndStart(selected);
                        return true;
                    }));
            return;
        }
        startAcceptedServer(selected);
    }

    private void acceptEulaAndStart(DevServerManager.ServerConfig selected) {
        try {
            DevServerManager.ServerConfig accepted = serverManager.updateSettings(selected, selected.name(),
                    true, selected.javaFlags());
            replaceSelected(accepted);
            startAcceptedServer(accepted);
        } catch (IOException exception) {
            showError("Could not accept the EULA or start the server", exception);
        }
    }

    private void startAcceptedServer(DevServerManager.ServerConfig selected) {
        try {
            serverManager.start(selected);
            servers.refresh();
            showServer(selected);
            status.setText("Server process started.");
        } catch (IOException exception) {
            showError("Could not start server", exception);
        }
    }

    private void stopServer() {
        DevServerManager.ServerConfig selected = servers.getSelectionModel().getSelectedItem();
        if (selected == null) {
            return;
        }
        setBusy(true, "Stopping server...");
        Thread.ofVirtual().start(() -> {
            try {
                serverManager.stop(selected.id());
                Platform.runLater(() -> {
                    setBusy(false, "Server stopped.");
                    showServer(selected);
                });
            } catch (IOException exception) {
                Platform.runLater(() -> setBusy(false, "Could not stop server: " + exception.getMessage()));
            }
        });
    }

    private void confirmTerminateServer() {
        DevServerManager.ServerConfig selected = servers.getSelectionModel().getSelectedItem();
        if (selected == null) {
            return;
        }
        flyouts.showConfirmation("Terminate server immediately",
                "Forcefully stop " + selected.name() + " now? Unsaved progress may be lost.", confirmed -> {
            if (confirmed) {
                Thread.ofVirtual().start(() -> {
                    try {
                        serverManager.terminate(selected.id());
                        Platform.runLater(() -> {
                            setBusy(false, "Server terminated.");
                            showServer(selected);
                        });
                    } catch (IOException exception) {
                        Platform.runLater(() -> setBusy(false,
                                "Could not terminate server: " + exception.getMessage()));
                    }
                });
            }
        });
    }

    public void refreshIcons() {
        IconAssets.refresh(this, launcherSettings.darkMode());
        ToggleSwitchStyle.refresh(this, launcherSettings.darkMode());
        for (Tab tab : detailTabs.getTabs()) {
            String icon = switch (tab.getText()) {
                case "Settings" -> "settings";
                case "Console" -> "console";
                default -> "download";
            };
            tab.setGraphic(IconAssets.graphic(icon, launcherSettings.darkMode()));
        }
        consoleView.refreshIcons();
    }

    private void deleteSelectedServer() {
        DevServerManager.ServerConfig selected = servers.getSelectionModel().getSelectedItem();
        if (selected == null) {
            return;
        }
        flyouts.showTypedDeleteConfirmation("Delete server",
                "Permanently delete " + selected.name() + " and all of its server files, worlds, and settings? "
                        + "This cannot be undone.", confirmed -> {
            if (confirmed) {
            try {
                serverManager.delete(selected);
                servers.getItems().removeIf(server -> server.id().equals(selected.id()));
                if (servers.getItems().isEmpty()) {
                    showServer(null);
                } else {
                    servers.getSelectionModel().selectFirst();
                }
                status.setText("Deleted server " + selected.name() + ".");
            } catch (IOException exception) {
                showError("Could not delete server", exception);
            }
            }
        });
    }

    private void searchContent() {
        DevServerManager.ServerConfig selected = servers.getSelectionModel().getSelectedItem();
        if (selected == null || selected.distribution() == ServerDistribution.VANILLA) {
            contentStatus.setText("Choose a Fabric or Paper server before browsing Modrinth content.");
            return;
        }
        long sequence = searchSequence.incrementAndGet();
        String query = searchField.getText();
        contentStatus.setText("Searching Modrinth...");
        Thread.ofVirtual().start(() -> {
            try {
                ModrinthClient.SearchPage page = modrinthManager.search(selected, query, searchOffset, PAGE_SIZE);
                Platform.runLater(() -> {
                    if (sequence == searchSequence.get() && isSelected(selected)) {
                        projects.setItems(FXCollections.observableArrayList(page.projects()));
                        contentStatus.setText("Showing " + page.projects().size() + " of " + page.total()
                                + " compatible projects.");
                    }
                });
            } catch (IOException | InterruptedException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                Platform.runLater(() -> {
                    if (sequence == searchSequence.get()) {
                        contentStatus.setText("Modrinth search failed: " + exception.getMessage());
                    }
                });
            }
        });
    }

    private void loadVersions(ModrinthClient.Project project) {
        DevServerManager.ServerConfig selected = servers.getSelectionModel().getSelectedItem();
        long sequence = versionSequence.incrementAndGet();
        versions.getItems().clear();
        if (project == null || selected == null) {
            return;
        }
        Thread.ofVirtual().start(() -> {
            try {
                List<ModrinthClient.ProjectVersion> compatible = modrinthManager.versions(selected, project);
                Platform.runLater(() -> {
                    if (sequence == versionSequence.get() && isSelected(selected)) {
                        versions.setItems(FXCollections.observableArrayList(compatible));
                        if (!compatible.isEmpty()) {
                            versions.getSelectionModel().selectFirst();
                        }
                    }
                });
            } catch (IOException | InterruptedException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                Platform.runLater(() -> {
                    if (sequence == versionSequence.get()) {
                        contentStatus.setText("Could not load project versions: " + exception.getMessage());
                    }
                });
            }
        });
    }

    private void installContent() {
        DevServerManager.ServerConfig selected = servers.getSelectionModel().getSelectedItem();
        ModrinthClient.Project project = projects.getSelectionModel().getSelectedItem();
        ModrinthClient.ProjectVersion version = versions.getValue();
        if (selected == null || project == null || version == null) {
            contentStatus.setText("Select a project and compatible version first.");
            return;
        }
        contentStatus.setText("Installing " + project.title() + "...");
        Thread.ofVirtual().start(() -> {
            try {
                modrinthManager.install(selected, project, version, ignored -> { });
                List<DevServerManager.InstalledServerContent> entries = modrinthManager.installed(selected);
                Platform.runLater(() -> {
                    if (isSelected(selected)) {
                        installed.setItems(FXCollections.observableArrayList(entries));
                        contentStatus.setText("Installed " + project.title() + ".");
                    }
                });
            } catch (IOException | InterruptedException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                Platform.runLater(() -> contentStatus.setText("Could not install content: "
                        + exception.getMessage()));
            }
        });
    }

    private void removeContent() {
        DevServerManager.ServerConfig selected = servers.getSelectionModel().getSelectedItem();
        DevServerManager.InstalledServerContent content = installed.getSelectionModel().getSelectedItem();
        if (selected == null || content == null) {
            contentStatus.setText("Select an installed file to remove.");
            return;
        }
        flyouts.showConfirmation("Remove server content",
                "Remove " + content.filename() + " from this server?", confirmed -> {
            if (confirmed) {
            try {
                modrinthManager.remove(selected, content);
                installed.getItems().remove(content);
                contentStatus.setText("Removed " + content.filename() + ".");
            } catch (IOException exception) {
                contentStatus.setText("Could not remove content: " + exception.getMessage());
            }
            }
        });
    }

    private void refreshInstalledContent(DevServerManager.ServerConfig selected) {
        Thread.ofVirtual().start(() -> {
            try {
                List<DevServerManager.InstalledServerContent> entries = selected.distribution()
                        == ServerDistribution.VANILLA ? List.of() : modrinthManager.installed(selected);
                Platform.runLater(() -> {
                    if (isSelected(selected)) {
                        installed.setItems(FXCollections.observableArrayList(entries));
                    }
                });
            } catch (IOException exception) {
                Platform.runLater(() -> contentStatus.setText("Could not load installed content: "
                        + exception.getMessage()));
            }
        });
    }

    private DevServerManager.ServerConfig findServer(String id) throws IOException {
        return serverManager.listServers().stream().filter(server -> server.id().equals(id)).findFirst().orElse(null);
    }

    private void replaceSelected(DevServerManager.ServerConfig updated) {
        int index = -1;
        for (int i = 0; i < servers.getItems().size(); i++) {
            if (servers.getItems().get(i).id().equals(updated.id())) {
                index = i;
                break;
            }
        }
        if (index >= 0) {
            servers.getItems().set(index, updated);
            servers.getSelectionModel().select(index);
        } else {
            refresh();
        }
    }

    private boolean isSelected(DevServerManager.ServerConfig config) {
        DevServerManager.ServerConfig selected = servers.getSelectionModel().getSelectedItem();
        return selected != null && selected.id().equals(config.id());
    }

    private boolean isSelectedId(String serverId) {
        DevServerManager.ServerConfig selected = servers.getSelectionModel().getSelectedItem();
        return selected != null && selected.id().equals(serverId);
    }

    private void setBusy(boolean busy, String message) {
        progress.setVisible(busy);
        progress.setManaged(busy);
        progress.setProgress(busy ? -1 : 0);
        installButton.setDisable(busy || servers.getSelectionModel().getSelectedItem() == null);
        startButton.setDisable(busy || servers.getSelectionModel().getSelectedItem() == null);
        stopButton.setDisable(busy || servers.getSelectionModel().getSelectedItem() == null);
        deleteServerButton.setDisable(busy || servers.getSelectionModel().getSelectedItem() == null
                || serverManager.snapshot(servers.getSelectionModel().getSelectedItem().id())
                .map(DevServerManager.ServerSnapshot::running).orElse(false));
        status.setText(message);
    }

    private void showStatus(String message) {
        Platform.runLater(() -> status.setText(message));
    }

    private void showError(String title, Exception exception) {
        flyouts.showError(title, exception.getMessage());
    }

    private static List<String> javaFlags(String text) {
        return text.lines().filter(line -> !line.isBlank()).map(String::trim).toList();
    }
}
