package net.stratalauncher.ui;

import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.Button;
import javafx.scene.control.CheckBox;
import javafx.scene.control.ComboBox;
import javafx.scene.control.Label;
import javafx.scene.control.ListCell;
import javafx.scene.control.TextField;
import javafx.scene.control.TextArea;
import javafx.scene.control.ListView;
import javafx.scene.control.Tab;
import javafx.scene.control.TabPane;
import javafx.scene.shape.SVGPath;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;
import javafx.application.Platform;
import javafx.animation.FadeTransition;
import javafx.animation.ParallelTransition;
import javafx.animation.Timeline;
import javafx.animation.TranslateTransition;
import javafx.animation.KeyFrame;
import javafx.animation.KeyValue;
import javafx.scene.control.ScrollPane;
import javafx.scene.Node;
import javafx.scene.paint.Color;
import net.stratalauncher.core.instance.InstanceManager;
import net.stratalauncher.core.instance.Instance;
import net.stratalauncher.core.auth.AccountManager;
import net.stratalauncher.core.auth.LauncherSettings;
import net.stratalauncher.core.auth.MicrosoftAuthService;
import net.stratalauncher.core.loader.LoaderInstaller;
import net.stratalauncher.core.launch.GameLauncher;
import net.stratalauncher.core.mojang.MojangManifestClient;
import net.stratalauncher.core.modrinth.ContentManager;
import net.stratalauncher.core.modrinth.ModrinthClient;
import net.stratalauncher.core.server.DevServerManager;
import net.stratalauncher.core.server.ServerModrinthManager;
import net.stratalauncher.core.runtime.JavaRuntimeManager;
import net.stratalauncher.ui.FlyoutManager;
import net.stratalauncher.ui.auth.AccountSelectorView;
import net.stratalauncher.ui.animations.PageTransition;
import net.stratalauncher.ui.content.ContentView;
import net.stratalauncher.ui.instances.InstanceListView;
import net.stratalauncher.ui.settings.InstanceSettingsView;
import net.stratalauncher.ui.server.ServerPanelView;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Path;
import java.time.Instant;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.logging.Level;
import java.util.logging.Logger;

public class MainLayout extends BorderPane {
    private static final Logger LOGGER = Logger.getLogger(MainLayout.class.getName());
    private final ThemeManager themeManager;
    private final InstanceManager instanceManager;
    private final MojangManifestClient manifestClient;
    private final DevServerManager serverManager;
    private final AccountManager accountManager;
    private final FlyoutManager flyouts;
    private final InstanceListView instanceListView;
    private final AccountSelectorView accountSelectorView;
    private final ContentView contentView;
    private final InstanceSettingsView instanceSettingsView;
    private final ScrollPane instanceSettingsFlyoutContent;
    private final ServerPanelView serverPanelView;
    private final JavaRuntimeManager javaRuntimeManager;
    private final LauncherSettings settings;
    private final FrameTimeOverlay frameTimeOverlay;
    private VBox homeContent;
    private VBox sidebar;
    private SVGPath menuIcon;
    private Button menuButton;
    private boolean sidebarVisible = true;
    private ParallelTransition sidebarAnimation;
    private StartupOverlay startupOverlay;
    private TabPane benchmarkSettingsTabs;
    private Instance benchmarkInstance;
    private TabPane cachedLauncherSettingsTabs;
    private FlyoutManager.Action cachedLauncherSettingsCancelAction;
    private FlyoutManager.Action cachedLauncherSettingsSaveAction;
    private Runnable resetCachedLauncherSettings = () -> { };

    public MainLayout(ThemeManager themeManager, InstanceManager instanceManager,
                      MojangManifestClient manifestClient, GameLauncher gameLauncher,
                      AccountManager accountManager, MicrosoftAuthService microsoftAuthService,
                      LauncherSettings settings, LoaderInstaller loaderInstaller,
                      ModrinthClient modrinthClient, ContentManager contentManager,
                      DevServerManager serverManager, ServerModrinthManager serverModrinthManager,
                      JavaRuntimeManager javaRuntimeManager, FrameTimeOverlay frameTimeOverlay,
                      FlyoutManager flyouts) {
        this.themeManager = themeManager;
        this.instanceManager = instanceManager;
        this.manifestClient = manifestClient;
        this.serverManager = serverManager;
        this.accountManager = accountManager;
        this.settings = settings;
        this.flyouts = flyouts;
        this.javaRuntimeManager = javaRuntimeManager;
        this.frameTimeOverlay = frameTimeOverlay;
        flyouts.setDarkMode(settings.darkMode());
        instanceListView = new InstanceListView(instanceManager, manifestClient, gameLauncher,
                accountManager, microsoftAuthService, settings, loaderInstaller, flyouts);
        accountSelectorView = new AccountSelectorView(accountManager, microsoftAuthService, settings, flyouts);
        contentView = new ContentView(instanceManager, loaderInstaller, modrinthClient, contentManager, settings);
        instanceSettingsView = new InstanceSettingsView(settings, gameLauncher, instanceManager,
                javaRuntimeManager, flyouts);
        instanceSettingsFlyoutContent = new ScrollPane(instanceSettingsView);
        instanceSettingsFlyoutContent.setFitToWidth(true);
        instanceSettingsFlyoutContent.setPrefViewportWidth(940);
        instanceSettingsFlyoutContent.setPrefViewportHeight(540);
        instanceSettingsFlyoutContent.setPannable(true);
        instanceSettingsView.setInstallHandler(instance -> {
            flyouts.close();
            navigate(instanceListView);
            instanceListView.installFromSettings(instance);
        });
        instanceSettingsView.setDeleteHandler(() -> {
            flyouts.close();
            instanceListView.refresh();
            navigate(instanceListView);
        });
        serverPanelView = new ServerPanelView(serverManager, serverModrinthManager, manifestClient, flyouts, settings);
        serverPanelView.setRefreshHandler(this::refreshStartupData);
        instanceListView.setInstanceSettingsHandler(instance -> {
            instanceSettingsView.showInstance(instance);
            flyouts.show("Instance settings", "Configure settings for " + instance.name() + ".",
                    instanceSettingsFlyoutContent, new FlyoutManager.Action("Close", "action-button", () -> true));
        });
        getStyleClass().add("launcher-root");
        setPadding(new Insets(18));
        sidebar = createSidebar();
        setLeft(sidebar);
        setTop(createMenuBar());
        homeContent = createMainContent();
        setCenter(homeContent);
    }

    private VBox createSidebar() {
        VBox sidebar = new VBox(18);
        sidebar.setPadding(new Insets(18, 16, 18, 16));
        sidebar.setPrefWidth(220);
        sidebar.setMinWidth(0);
        sidebar.setMaxWidth(220);
        sidebar.getStyleClass().add("sidebar");

        Button homeButton = createNavButton("Home", "home");
        homeButton.setOnAction(event -> navigate(homeContent));
        Button instancesButton = createNavButton("Instances", "instances");
        instancesButton.setOnAction(event -> {
            navigate(instanceListView);
        });
        Button contentButton = createNavButton("Content", "download");
        contentButton.setOnAction(event -> {
            navigate(contentView);
        });
        Button serversButton = createNavButton("Servers", "server");
        serversButton.setOnAction(event -> {
            navigate(serverPanelView);
        });
        Button settingsButton = createNavButton("Settings", "settings");
        Button accountsButton = createNavButton("Accounts", "account-add");
        accountsButton.setOnAction(event -> {
            navigate(accountSelectorView);
        });
        settingsButton.setOnAction(event -> showSettingsDialog());

        VBox navGroup = new VBox(10, homeButton, instancesButton, serversButton, accountsButton,
                contentButton, settingsButton);
        navGroup.setAlignment(Pos.TOP_LEFT);

        sidebar.getChildren().add(navGroup);
        VBox.setVgrow(navGroup, Priority.ALWAYS);
        return sidebar;
    }

    private HBox createMenuBar() {
        menuIcon = new SVGPath();
        menuIcon.getStyleClass().add("menu-icon");
        menuButton = new Button();
        menuButton.setGraphic(menuIcon);
        menuButton.getStyleClass().add("menu-button");
        menuButton.setAlignment(Pos.CENTER);
        menuButton.setContentDisplay(javafx.scene.control.ContentDisplay.GRAPHIC_ONLY);
        menuButton.setGraphicTextGap(0);
        menuButton.setAccessibleText("Collapse navigation menu");
        menuButton.setOnAction(event -> toggleSidebar());
        Label pageTitle = new Label("StrataLauncher");
        pageTitle.getStyleClass().add("launcher-title");
        HBox bar = new HBox(12, menuButton, pageTitle);
        bar.setAlignment(Pos.CENTER_LEFT);
        bar.setPadding(new Insets(0, 0, 12, 0));
        updateMenuIcon();
        return bar;
    }

    private void toggleSidebar() {
        if (sidebarAnimation != null) {
            sidebarAnimation.stop();
        }
        sidebarVisible = !sidebarVisible;
        updateMenuIcon();
        if (MotionConstants.reduceMotion) {
            setLeft(sidebarVisible ? sidebar : null);
            sidebar.setPrefWidth(220);
            sidebar.setOpacity(1);
            sidebar.setTranslateX(0);
            return;
        }
        if (sidebarVisible) {
            sidebar.setPrefWidth(0);
            sidebar.setOpacity(0);
            sidebar.setTranslateX(-22);
            setLeft(sidebar);
        }
        double fromWidth = sidebarVisible ? 0 : sidebar.getWidth();
        if (!sidebarVisible && fromWidth <= 0) {
            fromWidth = 220;
        }
        double toWidth = sidebarVisible ? 220 : 0;
        Timeline width = new Timeline(new KeyFrame(MotionConstants.SLOW,
                new KeyValue(sidebar.prefWidthProperty(), toWidth, MotionConstants.EMPHASIS)));
        FadeTransition fade = new FadeTransition(MotionConstants.SLOW, sidebar);
        fade.setFromValue(sidebarVisible ? 0 : sidebar.getOpacity());
        fade.setToValue(sidebarVisible ? 1 : 0);
        fade.setInterpolator(MotionConstants.STANDARD);
        TranslateTransition translate = new TranslateTransition(MotionConstants.SLOW, sidebar);
        translate.setFromX(sidebarVisible ? -22 : sidebar.getTranslateX());
        translate.setToX(sidebarVisible ? 0 : -22);
        translate.setInterpolator(MotionConstants.EMPHASIS);
        sidebar.prefWidthProperty().set(fromWidth);
        sidebarAnimation = new ParallelTransition(width, fade, translate);
        sidebarAnimation.setOnFinished(event -> {
            if (!sidebarVisible) {
                setLeft(null);
            }
            sidebar.setPrefWidth(220);
            sidebar.setOpacity(1);
            sidebar.setTranslateX(0);
        });
        sidebarAnimation.playFromStart();
    }

    private void updateMenuIcon() {
        String iconName = sidebarVisible ? "close" : "menu";
        String variant = themeManager.isDarkMode() ? "light" : "dark";
        IconData icon = loadIcon(iconName + "-" + variant + ".svg");
        menuIcon.setContent(icon.path());
        menuIcon.setFill(Color.web(icon.fill()));
        menuButton.setAccessibleText(sidebarVisible ? "Collapse navigation menu" : "Open navigation menu");
    }

    public void refreshIconTheme() {
        boolean darkMode = settings.darkMode();
        updateMenuIcon();
        IconAssets.refresh(this, darkMode);
        ToggleSwitchStyle.refresh(this, darkMode);
        instanceListView.refreshIndicators();
        contentView.refreshIcons();
        accountSelectorView.refreshIcons();
        instanceSettingsView.refreshIcons();
        serverPanelView.refreshIcons();
    }

    private static IconData loadIcon(String fileName) {
        String resourcePath = "/net/stratalauncher/ui/icons/" + fileName;
        try (InputStream stream = MainLayout.class.getResourceAsStream(resourcePath)) {
            if (stream == null) {
                throw new IllegalStateException("Launcher icon is missing: " + resourcePath);
            }
            String svg = new String(stream.readAllBytes(), StandardCharsets.UTF_8);
            Matcher path = Pattern.compile("<path[^>]*d=\"([^\"]+)\"").matcher(svg);
            Matcher fill = Pattern.compile("fill=\"(#[A-Fa-f0-9]{6})\"").matcher(svg);
            if (!path.find() || !fill.find()) {
                throw new IllegalStateException("Launcher icon is invalid: " + resourcePath);
            }
            return new IconData(path.group(1), fill.group(1));
        } catch (IOException exception) {
            throw new IllegalStateException("Could not read launcher icon: " + resourcePath, exception);
        }
    }

    private record IconData(String path, String fill) {
    }

    private void showSettingsDialog() {
        if (cachedLauncherSettingsTabs != null) {
            resetCachedLauncherSettings.run();
            flyouts.show("Launcher settings", "Configure launcher-wide defaults.",
                    cachedLauncherSettingsTabs, cachedLauncherSettingsCancelAction,
                    cachedLauncherSettingsSaveAction);
            return;
        }
        TextField clientIdField = new TextField(settings.azureClientId());
        clientIdField.setPromptText("Azure application (client) ID");
        ComboBox<String> themeChoice = new ComboBox<>(
                javafx.collections.FXCollections.observableArrayList("Dark", "Light"));
        themeChoice.setValue(settings.darkMode() ? "Dark" : "Light");
        CheckBox reduceMotion = new CheckBox("Reduce motion");
        IconAssets.apply(reduceMotion, "reduce-motion", settings.darkMode());
        ToggleSwitchStyle.apply(reduceMotion);
        reduceMotion.setSelected(settings.reduceMotion());
        CheckBox vsync = new CheckBox("VSync");
        ToggleSwitchStyle.apply(vsync);
        vsync.setSelected(settings.vsyncEnabled());
        java.util.concurrent.atomic.AtomicBoolean revertingVsync = new java.util.concurrent.atomic.AtomicBoolean();
        vsync.selectedProperty().addListener((observable, previous, selected) -> {
            if (revertingVsync.get()) {
                return;
            }
            try {
                settings.setDiagnosticsPreferences(selected, settings.frameTimeOverlayEnabled());
                flyouts.showToast("VSync will change after restarting the launcher.");
            } catch (java.io.IOException exception) {
                revertingVsync.set(true);
                try {
                    vsync.setSelected(previous);
                } finally {
                    revertingVsync.set(false);
                }
                flyouts.showToast("Could not save VSync setting: " + exception.getMessage(),
                        FlyoutManager.ToastKind.ERROR);
            }
        });
        CheckBox frameTimeOverlayEnabled = new CheckBox("Show frame-time overlay (debug)");
        ToggleSwitchStyle.apply(frameTimeOverlayEnabled);
        frameTimeOverlayEnabled.setSelected(settings.frameTimeOverlayEnabled());
        boolean forcedFrameTimeOverlay = Boolean.getBoolean("stratalauncher.debugFrameTime");
        frameTimeOverlayEnabled.setDisable(forcedFrameTimeOverlay);
        TextArea javaFlagsField = new TextArea(String.join("\n", settings.globalJavaLaunchFlags()));
        javaFlagsField.setPromptText(String.join("\n", LauncherSettings.RECOMMENDED_JAVA_FLAGS));
        javaFlagsField.setPrefRowCount(8);
        VBox themeSetting = setting("Theme", themeChoice, "Choose the launcher color theme.");
        VBox motionSetting = setting("Reduce motion", reduceMotion,
                "Reduces or disables decorative movement and pulsing effects.");
        VBox vsyncSetting = setting("VSync", vsync,
                "Restart required. Synchronizes rendering to the display refresh.");
        VBox frameTimeSetting = setting("Frame-time overlay (debug)", frameTimeOverlayEnabled,
                "Shows measured frame-time statistics for recent tab, flyout, card-hover, and startup animations. "
                        + (forcedFrameTimeOverlay
                        ? "This diagnostic run forces the overlay on."
                        : "Press Ctrl+F9 to clear the current run's samples."));
        Label accountLabel = new Label("Azure client ID");
        IconAssets.apply(accountLabel, "secure", settings.darkMode());
        VBox accountSetting = setting(accountLabel, clientIdField,
                "Use a public-client application ID. No client secret is needed.");
        VBox flagsSetting = setting("Global Java launch flags", javaFlagsField,
                "Advanced options used by games without their own settings. Unsupported values can cause crashes or "
                        + "prevent Minecraft from starting. Most players should leave the defaults unchanged.");
        VBox generalSettings = new VBox(14, themeSetting, motionSetting, vsyncSetting, frameTimeSetting,
                accountSetting, flagsSetting);
        generalSettings.setPadding(new Insets(10));
        ScrollPane generalScroll = new ScrollPane(generalSettings);
        generalScroll.setFitToWidth(true);
        generalScroll.setHbarPolicy(ScrollPane.ScrollBarPolicy.NEVER);
        Tab generalTab = new Tab("General", generalScroll);
        generalTab.setClosable(false);

        ComboBox<LauncherSettings.RuntimeVendor> runtimeVendor = new ComboBox<>(
                javafx.collections.FXCollections.observableArrayList(LauncherSettings.RuntimeVendor.values()));
        runtimeVendor.setValue(settings.runtimeVendor());
        runtimeVendor.setMaxWidth(Double.MAX_VALUE);
        runtimeVendor.setCellFactory(ignored -> new ListCell<>() {
            @Override
            protected void updateItem(LauncherSettings.RuntimeVendor vendor, boolean empty) {
                super.updateItem(vendor, empty);
                setText(empty || vendor == null ? null : vendor.label());
            }
        });
        runtimeVendor.setButtonCell(new ListCell<>() {
            @Override
            protected void updateItem(LauncherSettings.RuntimeVendor vendor, boolean empty) {
                super.updateItem(vendor, empty);
                setText(empty || vendor == null ? null : vendor.label());
            }
        });
        CheckBox redundantRuntimes = new CheckBox("Keep a separate copy for each instance or server");
        ToggleSwitchStyle.apply(redundantRuntimes);
        redundantRuntimes.setSelected(settings.redundantJavaRuntimes());
        VBox vendorSetting = setting("Preferred Java runtime provider", runtimeVendor,
                "Choose where managed Java runtimes are downloaded from. This applies when installing a runtime.");
        VBox redundantSetting = setting("Redundant Java Runtimes", redundantRuntimes,
                "By default, matching Java runtimes are shared to save disk space.");

        ListView<JavaRuntimeManager.InstalledRuntime> runtimes = new ListView<>();
        runtimes.setCellFactory(ignored -> new ListCell<>() {
            @Override
            protected void updateItem(JavaRuntimeManager.InstalledRuntime runtime, boolean empty) {
                super.updateItem(runtime, empty);
                setText(empty || runtime == null ? null : runtime.displayName());
            }
        });
        runtimes.setPrefHeight(220);
        Label runtimeStatus = new Label();
        runtimeStatus.setWrapText(true);
        Button refreshRuntimes = new Button("Refresh runtimes");
        IconAssets.apply(refreshRuntimes, "refresh", settings.darkMode());
        refreshRuntimes.setOnAction(event ->
                refreshRuntimeList(runtimes, runtimeStatus));
        Button deleteRuntime = new Button("Delete unused runtime");
        IconAssets.apply(deleteRuntime, "delete", settings.darkMode());
        deleteRuntime.getStyleClass().add("danger-button");
        deleteRuntime.setDisable(true);
        runtimes.getSelectionModel().selectedItemProperty().addListener((observable, oldValue, selected) ->
                deleteRuntime.setDisable(selected == null || !selected.users().isEmpty()));
        deleteRuntime.setOnAction(event -> {
            JavaRuntimeManager.InstalledRuntime selected = runtimes.getSelectionModel().getSelectedItem();
            if (selected == null || !selected.users().isEmpty()) {
                return;
            }
            flyouts.showTypedDeleteConfirmation("Delete Java runtime",
                    "This permanently removes " + selected.displayName() + " from this launcher.",
                    confirmed -> {
                        if (!confirmed) {
                            return;
                        }
                        if (!refreshRuntimeList(runtimes, runtimeStatus)) {
                            return;
                        }
                        JavaRuntimeManager.InstalledRuntime current = runtimes.getItems().stream()
                                .filter(runtime -> runtime.directory().equals(selected.directory()))
                                .findFirst().orElse(null);
                        if (current == null) {
                            runtimeStatus.setText("This runtime is no longer installed.");
                            return;
                        }
                        if (!current.users().isEmpty()) {
                            runtimeStatus.setText("This runtime is now in use by "
                                    + String.join(", ", current.users()) + " and cannot be deleted.");
                            return;
                        }
                        try {
                            javaRuntimeManager.deleteRuntime(current);
                            refreshRuntimeList(runtimes, runtimeStatus);
                        } catch (IOException exception) {
                            runtimeStatus.setText("Could not delete the runtime: " + exception.getMessage());
                        }
                    });
        });
        HBox runtimeActions = new HBox(8, refreshRuntimes, deleteRuntime);
        VBox runtimeSettings = new VBox(12, vendorSetting, redundantSetting,
                new Label("Installed runtimes"), runtimes, runtimeActions, runtimeStatus);
        runtimeSettings.setPadding(new Insets(10));
        Tab runtimeTab = new Tab("Runtime Manager", runtimeSettings);
        runtimeTab.setClosable(false);
        TabPane tabs = new TabPane(generalTab, runtimeTab);
        benchmarkSettingsTabs = tabs;
        PageTransition.attachTabTransitions(tabs);
        tabs.setPrefHeight(500);
        refreshRuntimeList(runtimes, runtimeStatus);

        resetCachedLauncherSettings = () -> {
            clientIdField.setText(settings.azureClientId());
            themeChoice.setValue(settings.darkMode() ? "Dark" : "Light");
            reduceMotion.setSelected(settings.reduceMotion());
            vsync.setSelected(settings.vsyncEnabled());
            frameTimeOverlayEnabled.setSelected(settings.frameTimeOverlayEnabled());
            javaFlagsField.setText(String.join("\n", settings.globalJavaLaunchFlags()));
            runtimeVendor.setValue(settings.runtimeVendor());
            redundantRuntimes.setSelected(settings.redundantJavaRuntimes());
            runtimes.getSelectionModel().clearSelection();
            refreshRuntimeList(runtimes, runtimeStatus);
        };
        cachedLauncherSettingsTabs = tabs;
        cachedLauncherSettingsCancelAction = new FlyoutManager.Action("Cancel", "action-button", () -> true);
        cachedLauncherSettingsSaveAction = new FlyoutManager.Action("Save", "primary-button", () -> {
            try {
                if (runtimeVendor.getValue() == null) {
                    throw new IllegalArgumentException("Choose a Java runtime provider.");
                }
                boolean darkMode = "Dark".equals(themeChoice.getValue());
                settings.setPreferences(clientIdField.getText(), javaFlags(javaFlagsField.getText()),
                        darkMode, reduceMotion.isSelected(), runtimeVendor.getValue(),
                        redundantRuntimes.isSelected(), vsync.isSelected(),
                        frameTimeOverlayEnabled.isSelected());
                frameTimeOverlay.setEnabled(frameTimeOverlayEnabled.isSelected() || forcedFrameTimeOverlay);
                MotionConstants.reduceMotion = reduceMotion.isSelected();
                themeManager.apply(null, darkMode);
                flyouts.setDarkMode(darkMode);
                updateMenuIcon();
                IconAssets.refresh(this, darkMode);
                ToggleSwitchStyle.refresh(this, darkMode);
                IconAssets.refresh(reduceMotion, darkMode);
                ToggleSwitchStyle.refresh(reduceMotion, darkMode);
                instanceListView.refreshIndicators();
                contentView.refreshIcons();
                accountSelectorView.refreshIcons();
                instanceSettingsView.refreshIcons();
                serverPanelView.refreshIcons();
                flyouts.showToast("Launcher settings saved.");
                return true;
            } catch (java.io.IOException | IllegalArgumentException exception) {
                flyouts.showToast("Could not save launcher settings: " + exception.getMessage(),
                        FlyoutManager.ToastKind.ERROR);
                return false;
            }
        });
        flyouts.show("Launcher settings", "Configure launcher-wide defaults.", tabs,
                cachedLauncherSettingsCancelAction, cachedLauncherSettingsSaveAction);
    }

    private boolean refreshRuntimeList(ListView<JavaRuntimeManager.InstalledRuntime> runtimes, Label status) {
        try {
            List<JavaRuntimeManager.RuntimeOwner> owners = new java.util.ArrayList<>();
            for (Instance instance : instanceManager.listInstances()) {
                net.stratalauncher.core.settings.InstanceSettings instanceSettings =
                        new net.stratalauncher.core.settings.InstanceSettings(instance);
                if (instanceSettings.javaRuntimeMode()
                        == net.stratalauncher.core.settings.InstanceSettings.JavaRuntimeMode.MANAGED) {
                    String component = instanceSettings.javaRuntimeComponent();
                    if (component == null || component.isBlank()) {
                        component = javaRuntimeManager.runtimeComponent(instance,
                                LauncherSettings.RuntimeVendor.MOJANG);
                    }

                    owners.add(new JavaRuntimeManager.RuntimeOwner("Instance: " + instance.name(),
                            instance.directory(), true, component));
                }
            }
            for (DevServerManager.ServerConfig server : serverManager.listServers()) {
                if (server.useManagedJavaRuntime()) {
                    owners.add(new JavaRuntimeManager.RuntimeOwner("Server: " + server.name(),
                            serverManager.serverDirectoryPath(server), true, server.javaRuntimeComponent()));
                }
            }
            runtimes.getItems().setAll(javaRuntimeManager.listInstalledRuntimes(owners));
            status.setText(runtimes.getItems().isEmpty()
                    ? "No managed Java runtimes are installed."
                    : runtimes.getItems().size() + " managed runtime(s) found.");
            return true;
        } catch (IOException exception) {
            status.setText("Could not load installed runtimes: " + exception.getMessage());
            return false;
        }
    }

    void prepareBenchmarkScenario(String scenario) {
        flyouts.close();
        flyouts.clearBenchmarkToasts();
        benchmarkInstance = new Instance("00000000-0000-0000-0000-000000000001",
                "Benchmark Instance", "1.21.1", Instant.now(),
                Path.of(System.getProperty("java.io.tmpdir"), "stratalauncher-benchmark-instance"));
        switch (scenario) {
            case "screen-switch" -> {
                instanceListView.initialize(List.of(benchmarkInstance), null);
                setCenter(homeContent);
            }
            case "flyout-instance-settings", "flyout-launcher-settings", "toast", "startup-animation" ->
                    setCenter(homeContent);
            case "flyout-accounts" -> setCenter(accountSelectorView);
            case "tab-switch" -> {
                setCenter(homeContent);
                showSettingsDialog();
            }
            case "card-hover" -> {
                instanceListView.initialize(List.of(benchmarkInstance), null);
                setCenter(instanceListView);
            }
            default -> throw new IllegalArgumentException("Unknown benchmark scenario: " + scenario);
        }
    }

    void benchmarkToggleScreen() {
        Node current = getCenter();
        PageTransition.show(this, current == homeContent ? instanceListView : homeContent);
    }

    void benchmarkOpenInstanceSettings() {
        instanceSettingsView.showInstance(benchmarkInstance);
        flyouts.show("Instance settings", "Configure settings for " + benchmarkInstance.name(),
                instanceSettingsFlyoutContent, new FlyoutManager.Action("Close", "action-button", () -> true));
    }

    void benchmarkOpenLauncherSettings() {
        showSettingsDialog();
    }

    void benchmarkOpenAccountsFlyout() {
        flyouts.showPrompt("Add offline account", "Player name (1-16 letters, numbers, or underscores)",
                "", ignored -> { });
    }

    void benchmarkSelectSettingsTab(int index) {
        if (benchmarkSettingsTabs == null || benchmarkSettingsTabs.getTabs().size() <= index) {
            throw new IllegalStateException("Launcher settings tabs are not available for benchmarking.");
        }
        benchmarkSettingsTabs.getSelectionModel().select(index);
    }

    void benchmarkHoverInstanceCard(boolean hovered) {
        instanceListView.benchmarkHoverCard(hovered);
    }

    private static VBox setting(String title, javafx.scene.Node control, String description) {
        Label label = new Label(title);
        return setting(label, control, description);
    }

    private static VBox setting(Label label, javafx.scene.Node control, String description) {
        label.getStyleClass().add("setting-label");
        Label detail = new Label(description);
        detail.getStyleClass().add("setting-description");
        detail.setWrapText(true);
        return new VBox(8, label, control, detail);
    }

    private void navigate(javafx.scene.Node page) {
        instanceListView.closeRunningInstanceScreen();
        if (page instanceof javafx.scene.layout.Region region) {
            region.setMaxHeight(Double.MAX_VALUE);
            region.setMaxWidth(Double.MAX_VALUE);
        }
        PageTransition.show(this, page);
    }

    public CompletableFuture<Void> initializeStartup() {
        return CompletableFuture.supplyAsync(() -> {
            List<Instance> instances = List.of();
            MojangManifestClient.VersionManifest versions = null;
            List<net.stratalauncher.core.server.DevServerManager.ServerConfig> servers = List.of();
            String instanceError = null;
            String versionError = null;
            String serverError = null;
            try {
                instances = instanceManager.listInstances();
            } catch (IOException exception) {
                LOGGER.log(Level.WARNING, "Could not load saved instances during startup.", exception);
                instanceError = exception.getMessage();
            }
            try {
                versions = manifestClient.fetchManifest();
            } catch (IOException | InterruptedException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                LOGGER.log(Level.WARNING, "Could not load the Minecraft version manifest during startup.", exception);
                versionError = exception.getMessage();
            }
            try {
                servers = serverManager.listServers();
            } catch (IOException exception) {
                LOGGER.log(Level.WARNING, "Could not load saved servers during startup.", exception);
                serverError = exception.getMessage();
            }
            return new StartupData(instances, versions, servers,
                    instanceError, versionError, serverError);
        }).thenCompose(data -> {
            CompletableFuture<Void> applied = new CompletableFuture<>();
            Platform.runLater(() -> {
                try {
                    instanceListView.initialize(data.instances(), data.versions());
                    contentView.initialize(data.instances());
                    accountSelectorView.refreshAccounts();
                    serverPanelView.initialize(data.servers());
                    if (data.instanceError() != null) {
                        flyouts.showToast("Could not load instances: " + data.instanceError(),
                                FlyoutManager.ToastKind.ERROR);
                    }
                    if (data.versionError() != null) {
                        flyouts.showToast("Could not load Minecraft versions: " + data.versionError(),
                                FlyoutManager.ToastKind.ERROR);
                    }
                    if (data.serverError() != null) {
                        flyouts.showToast("Could not load servers: " + data.serverError(),
                                FlyoutManager.ToastKind.ERROR);
                    }
                    applied.complete(null);
                } catch (RuntimeException exception) {
                    applied.completeExceptionally(exception);
                }
            });
            return applied;
        });
    }

    public void setStartupOverlay(StartupOverlay startupOverlay) {
        this.startupOverlay = startupOverlay;
    }

    public void refreshStartupData() {
        if (startupOverlay == null) {
            initializeStartup();
            return;
        }
        startupOverlay.setVisible(true);
        startupOverlay.playEntrance();
        initializeStartup().whenComplete((ignored, failure) -> Platform.runLater(() -> {
            if (failure != null) {
                LOGGER.log(Level.SEVERE, "Launcher refresh failed.", failure);
                Throwable cause = failure;
                while (cause.getCause() != null && cause.getCause() != cause) {
                    cause = cause.getCause();
                }
                String message = cause.getMessage() == null ? cause.toString() : cause.getMessage();
                flyouts.showError("Launcher refresh failed", message);
            }
            startupOverlay.fadeOut(() -> { });
        }));
    }

    private record StartupData(List<Instance> instances,
                               MojangManifestClient.VersionManifest versions,
                               List<net.stratalauncher.core.server.DevServerManager.ServerConfig> servers,
                               String instanceError, String versionError, String serverError) {
    }

    private static java.util.List<String> javaFlags(String text) {
        return text.lines().filter(line -> !line.isBlank()).map(String::trim).toList();
    }

    private Button createNavButton(String label, String icon) {
        Button button = new Button(label);
        IconAssets.apply(button, icon, themeManager.isDarkMode());
        button.getStyleClass().add("nav-button");
        button.setMaxWidth(Double.MAX_VALUE);
        button.setAlignment(Pos.CENTER_LEFT);
        return button;
    }

    private VBox createMainContent() {
        VBox content = new VBox(20);
        content.setPadding(new Insets(10));

        HBox hero = new HBox(20);
        hero.setPadding(new Insets(20));
        hero.setAlignment(Pos.CENTER_LEFT);
        hero.getStyleClass().add("hero-panel");

        VBox intro = new VBox(10);
        Label welcome = new Label("Your launcher home");
        welcome.getStyleClass().add("hero-title");
        Label summary = new Label("Launch, manage, and organize your Minecraft instances from a single shell.");
        summary.setWrapText(true);
        summary.getStyleClass().add("hero-subtitle");
        intro.getChildren().addAll(welcome, summary);

        Button primaryAction = new Button("Create instance");
        IconAssets.apply(primaryAction, "add", themeManager.isDarkMode());
        primaryAction.getStyleClass().add("primary-button");
        primaryAction.setOnAction(event -> {
            navigate(instanceListView);
            instanceListView.showCreateDialog();
        });
        hero.getChildren().addAll(intro, primaryAction);

        content.getChildren().add(hero);
        return content;
    }
}
