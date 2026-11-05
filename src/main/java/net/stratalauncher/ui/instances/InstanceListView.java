package net.stratalauncher.ui.instances;

import javafx.application.Platform;
import javafx.animation.ScaleTransition;
import javafx.collections.FXCollections;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.ListCell;
import javafx.scene.control.ListView;
import javafx.scene.control.SelectionMode;
import javafx.scene.control.TabPane;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;
import javafx.scene.Node;
import javafx.scene.input.MouseEvent;
import javafx.scene.transform.Scale;
import net.stratalauncher.core.auth.Account;
import net.stratalauncher.core.auth.AccountManager;
import net.stratalauncher.core.auth.LauncherSettings;
import net.stratalauncher.core.auth.MicrosoftAuthService;
import net.stratalauncher.core.instance.Instance;
import net.stratalauncher.core.instance.InstanceManager;
import net.stratalauncher.core.launch.GameLauncher;
import net.stratalauncher.core.loader.LoaderInstaller;
import net.stratalauncher.core.loader.LoaderSelection;
import net.stratalauncher.core.loader.LoaderType;
import net.stratalauncher.core.mojang.MojangManifestClient;
import net.stratalauncher.ui.FlyoutManager;
import net.stratalauncher.ui.FrameTimeOverlay;
import net.stratalauncher.ui.IconAssets;
import net.stratalauncher.ui.MotionConstants;
import net.stratalauncher.ui.components.StatusIndicator;
import net.stratalauncher.ui.install.InstallingView;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Consumer;

public final class InstanceListView extends BorderPane {
    private final InstanceManager instanceManager;
    private final AccountManager accountManager;
    private final MicrosoftAuthService microsoftAuthService;
    private final LauncherSettings settings;
    private final LoaderInstaller loaderInstaller;
    private final MojangManifestClient manifestClient;
    private final GameLauncher gameLauncher;
    private final FlyoutManager flyouts;
    private final ListView<Instance> instanceList = new ListView<>();
    private final Label statusLabel = new Label("Preparing launcher...");
    private final Button launchSelectedButton = new Button("Launch");
    private final Map<String, StatusIndicator.Status> runtimeStates = new ConcurrentHashMap<>();
    private final Map<String, RunningGame> runningGames = new ConcurrentHashMap<>();
    private final java.util.Set<String> requestedStops = ConcurrentHashMap.newKeySet();
    private VBox content;
    private RunningInstanceView runningInstanceView;
    private MojangManifestClient.VersionManifest manifest;
    private Consumer<Instance> instanceSettingsHandler = ignored -> { };

    public InstanceListView(InstanceManager instanceManager, MojangManifestClient manifestClient,
                            GameLauncher gameLauncher, AccountManager accountManager,
                            MicrosoftAuthService microsoftAuthService, LauncherSettings settings,
                            LoaderInstaller loaderInstaller, FlyoutManager flyouts) {
        this.instanceManager = instanceManager;
        this.manifestClient = manifestClient;
        this.gameLauncher = gameLauncher;
        this.accountManager = accountManager;
        this.microsoftAuthService = microsoftAuthService;
        this.settings = settings;
        this.loaderInstaller = loaderInstaller;
        this.flyouts = flyouts;
        setPadding(new Insets(10));
        getStyleClass().add("instances-view");

        Label title = new Label("Instances");
        title.getStyleClass().add("hero-title");
        Button createButton = new Button("Create");
        IconAssets.apply(createButton, "add", settings.darkMode());
        createButton.getStyleClass().add("primary-button");
        createButton.setOnAction(event -> showCreateDialog());
        Button duplicateButton = new Button("Duplicate");
        IconAssets.apply(duplicateButton, "duplicate", settings.darkMode());
        duplicateButton.getStyleClass().add("action-button");
        duplicateButton.setOnAction(event -> duplicateSelected());
        HBox actions = new HBox(10, createButton, duplicateButton);
        actions.setAlignment(Pos.CENTER_LEFT);
        VBox header = new VBox(10, title, actions);

        instanceList.getSelectionModel().setSelectionMode(SelectionMode.SINGLE);
        instanceList.getStyleClass().add("card-list");
        instanceList.addEventFilter(MouseEvent.MOUSE_CLICKED, this::deselectWhenContainerClicked);
        instanceList.setCellFactory(ignored -> new InstanceCardCell());
        instanceList.setPlaceholder(new Label("No instances yet. Create one to get started."));
        launchSelectedButton.getStyleClass().add("primary-button");
        launchSelectedButton.setDisable(true);
        launchSelectedButton.setOnAction(event -> launchSelected());
        IconAssets.apply(launchSelectedButton, "start", settings.darkMode());
        instanceList.getSelectionModel().selectedItemProperty().addListener((observable, oldValue, selected) ->
                updateLaunchSelectedButton());

        statusLabel.setWrapText(true);
        statusLabel.getStyleClass().add("hero-subtitle");
        HBox footer = new HBox(14, launchSelectedButton, statusLabel);
        footer.setAlignment(Pos.CENTER_LEFT);
        HBox.setHgrow(statusLabel, Priority.ALWAYS);
        content = new VBox(14, header, instanceList, footer);
        VBox.setVgrow(instanceList, Priority.ALWAYS);
        setCenter(content);
    }

    private void stopInstance(Instance instance) {
        RunningGame running = runningGames.get(instance.id());
        if (running == null || !running.process().isAlive()) {
            return;
        }
        showStatus("Closing " + instance.name() + "...");
        runInBackground(() -> {
            Process process = running.process();
                requestedStops.add(instance.id());
                process.destroy();
            try {
                if (!process.waitFor(8, java.util.concurrent.TimeUnit.SECONDS)) {
                    process.destroyForcibly();
                    process.waitFor();
                }
            } catch (InterruptedException exception) {
                Thread.currentThread().interrupt();
                process.destroyForcibly();
            }
        });
    }

    public void setInstanceSettingsHandler(Consumer<Instance> handler) {
        instanceSettingsHandler = handler == null ? ignored -> { } : handler;
    }

    public void installFromSettings(Instance selected) {
        try {
            installInstance(selected, loaderInstaller.installedSelection(selected));
        } catch (IOException exception) {
            flyouts.showError("Could not read the selected instance loader", exception.getMessage());
        }
    }

    public void refreshIndicators() {
        instanceList.refresh();
        updateLaunchSelectedButton();
    }

    private void deselectWhenContainerClicked(MouseEvent event) {
        if (!(event.getTarget() instanceof Node target)) {
            return;
        }
        Node current = target;
        while (current != null && current != instanceList) {
            if (current.getStyleClass().contains("instance-card")) {
                return;
            }
            current = current.getParent();
        }
        instanceList.getSelectionModel().clearSelection();
    }

    public void initialize(List<Instance> instances, MojangManifestClient.VersionManifest versionManifest) {
        manifest = versionManifest;
        instanceList.setItems(FXCollections.observableArrayList(instances));
        instanceList.getSelectionModel().clearSelection();
        runtimeStates.keySet().retainAll(instances.stream().map(Instance::id).toList());
        if (instances.isEmpty()) {
            statusLabel.setText("No instances yet. Create one to get started.");
        } else if (versionManifest == null) {
            statusLabel.setText("Minecraft version data is unavailable. Restart the launcher to retry.");
        } else {
            statusLabel.setText("Latest Minecraft release: " + versionManifest.latestRelease());
        }
        instanceList.refresh();
    }

    public void benchmarkHoverCard(boolean hovered) {
        Node card = lookupAll(".instance-card").stream().filter(Node::isVisible).findFirst()
                .orElseThrow(() -> new IllegalStateException("No visible instance card is available to benchmark."));
        if (hovered) {
            if (card.getOnMouseEntered() == null) {
                throw new IllegalStateException("The instance card has no hover handler.");
            }
            card.getOnMouseEntered().handle(null);
        } else if (card.getOnMouseExited() != null) {
            card.getOnMouseExited().handle(null);
        }
    }

    public void refresh() {
        runInBackground(() -> {
            try {
                List<Instance> instances = instanceManager.listInstances();
                Platform.runLater(() -> {
                    String selectedId = instanceList.getSelectionModel().getSelectedItem() == null
                            ? null : instanceList.getSelectionModel().getSelectedItem().id();
                    instanceList.setItems(FXCollections.observableArrayList(instances));
                    instances.stream().map(Instance::id).forEach(id ->
                            runtimeStates.putIfAbsent(id, StatusIndicator.Status.GREY));
                    if (selectedId != null) {
                        instances.stream().filter(instance -> instance.id().equals(selectedId)).findFirst()
                                .ifPresent(instance -> instanceList.getSelectionModel().select(instance));
                    }
                    instanceList.refresh();
                });
            } catch (IOException exception) {
                showStatus("Could not load instances: " + exception.getMessage());
            }
        });
    }

    public void showCreateDialog() {
        if (manifest == null) {
            flyouts.showToast("Minecraft version data is not available yet.", FlyoutManager.ToastKind.WARNING);
            return;
        }
        javafx.scene.control.TextField nameField = new javafx.scene.control.TextField(
                "Minecraft " + manifest.latestRelease());
        nameField.setPromptText("Instance name");
        javafx.scene.control.ComboBox<MojangManifestClient.Version> versionBox = new javafx.scene.control.ComboBox<>(
                FXCollections.observableArrayList(manifest.versions()));
        versionBox.getSelectionModel().select(manifest.latestReleaseVersion());
        versionBox.setMaxWidth(Double.MAX_VALUE);
        javafx.scene.control.ComboBox<LoaderType> loaderBox = new javafx.scene.control.ComboBox<>(
                FXCollections.observableArrayList(LoaderType.values()));
        loaderBox.getSelectionModel().select(LoaderType.VANILLA);
        loaderBox.setMaxWidth(Double.MAX_VALUE);
        javafx.scene.control.ComboBox<String> loaderVersionBox = new javafx.scene.control.ComboBox<>();
        loaderVersionBox.setPromptText("Select a Minecraft version and loader");
        loaderVersionBox.setMaxWidth(Double.MAX_VALUE);
        loaderVersionBox.setDisable(true);
        Label loaderStatus = new Label("Vanilla uses no mod loader.");
        loaderStatus.setWrapText(true);
        loaderBox.valueProperty().addListener((observable, previous, selected) ->
                refreshLoaderVersions(loaderBox, versionBox, loaderVersionBox, loaderStatus));
        versionBox.valueProperty().addListener((observable, previous, selected) ->
                refreshLoaderVersions(loaderBox, versionBox, loaderVersionBox, loaderStatus));
        VBox fields = new VBox(10, new Label("Name"), nameField,
                new Label("Minecraft version"), versionBox,
                new Label("Mod loader"), loaderBox,
                new Label("Loader version"), loaderVersionBox, loaderStatus);
        fields.setPadding(new Insets(8));
        flyouts.show("Create instance", "Choose a name, Minecraft version, and optional mod loader.", fields,
                new FlyoutManager.Action("Cancel", "action-button", () -> true),
                new FlyoutManager.Action("Create", "primary-button", () -> {
                    if (nameField.getText().isBlank() || versionBox.getValue() == null || loaderBox.getValue() == null) {
                        flyouts.showToast("Enter a name and choose a Minecraft version.", FlyoutManager.ToastKind.WARNING);
                        return false;
                    }
                    LoaderSelection selection;
                    try {
                        selection = loaderBox.getValue() == LoaderType.VANILLA
                                ? LoaderSelection.vanilla()
                                : new LoaderSelection(loaderBox.getValue(), loaderVersionBox.getValue());
                    } catch (IllegalArgumentException exception) {
                        flyouts.showToast("Choose a compatible loader version before continuing.",
                                FlyoutManager.ToastKind.WARNING);
                        return false;
                    }
                    String name = nameField.getText();
                    String versionId = versionBox.getValue().id();
                    runInBackground(() -> {
                        try {
                            Instance instance = instanceManager.create(name, versionId);
                            Platform.runLater(() -> {
                                refresh();
                                installInstance(instance, selection);
                            });
                        } catch (IOException | IllegalArgumentException exception) {
                            Platform.runLater(() ->
                                    flyouts.showError("Could not create instance", exception.getMessage()));
                        }
                    });
                    return true;
                }));
    }

    private void refreshLoaderVersions(javafx.scene.control.ComboBox<LoaderType> loaderBox,
                                      javafx.scene.control.ComboBox<MojangManifestClient.Version> minecraftVersionBox,
                                      javafx.scene.control.ComboBox<String> loaderVersionBox, Label loaderStatus) {
        LoaderType type = loaderBox.getValue();
        MojangManifestClient.Version minecraftVersion = minecraftVersionBox.getValue();
        loaderVersionBox.getItems().clear();
        if (type == null || type == LoaderType.VANILLA) {
            loaderVersionBox.setDisable(true);
            loaderStatus.setText("Vanilla uses no mod loader.");
            return;
        }
        if (minecraftVersion == null) {
            loaderVersionBox.setDisable(true);
            loaderStatus.setText("Choose a Minecraft version first.");
            return;
        }
        loaderVersionBox.setDisable(true);
        loaderStatus.setText("Fetching compatible " + type + " versions...");
        runInBackground(() -> {
            try {
                List<String> versions = loaderInstaller.availableVersions(type, minecraftVersion.id());
                Platform.runLater(() -> {
                    if (loaderBox.getValue() != type || loaderVersionBox.getScene() == null
                            || minecraftVersionBox.getValue() == null
                            || !minecraftVersion.id().equals(minecraftVersionBox.getValue().id())) {
                        return;
                    }
                    loaderVersionBox.setItems(FXCollections.observableArrayList(versions));
                    if (!versions.isEmpty()) {
                        loaderVersionBox.getSelectionModel().selectFirst();
                        loaderStatus.setText(versions.size() + " compatible versions available.");
                        loaderVersionBox.setDisable(false);
                    } else {
                        loaderStatus.setText("No compatible " + type + " versions are available for Minecraft "
                                + minecraftVersion.id() + ".");
                    }
                });
            } catch (IOException | InterruptedException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                Platform.runLater(() -> {
                    if (loaderBox.getValue() == type && loaderVersionBox.getScene() != null
                            && minecraftVersionBox.getValue() != null
                            && minecraftVersion.id().equals(minecraftVersionBox.getValue().id())) {
                        loaderStatus.setText("Could not fetch " + type + " versions: " + exception.getMessage());
                    }
                });
            }
        });
    }

    private void duplicateSelected() {
        Instance selected = instanceList.getSelectionModel().getSelectedItem();
        if (selected == null) {
            flyouts.showToast("Select an instance to duplicate.", FlyoutManager.ToastKind.WARNING);
            return;
        }
        flyouts.showPrompt("Duplicate instance", "New instance name", "Copy of " + selected.name(), name ->
                runInBackground(() -> {
                    try {
                        Instance duplicate = instanceManager.duplicate(selected, name);
                        Platform.runLater(() -> {
                            refresh();
                            showStatus("Duplicated as " + duplicate.name());
                        });
                    } catch (IOException | IllegalArgumentException exception) {
                        Platform.runLater(() ->
                                flyouts.showError("Could not duplicate instance", exception.getMessage()));
                    }
                }));
    }

    private void launchSelected() {
        Instance selected = instanceList.getSelectionModel().getSelectedItem();
        if (selected != null) {
            StatusIndicator.Status state = runtimeStates.getOrDefault(selected.id(), StatusIndicator.Status.GREY);
            if (state == StatusIndicator.Status.GREEN) {
                stopInstance(selected);
            } else {
                launch(selected);
            }
        }
    }

    private void launch(Instance selected) {
        StatusIndicator.Status state = runtimeStates.getOrDefault(selected.id(), StatusIndicator.Status.GREY);
        if (state == StatusIndicator.Status.BLUE || state == StatusIndicator.Status.GREEN) {
            flyouts.showToast(selected.name() + " is already running.", FlyoutManager.ToastKind.WARNING);
            return;
        }
        Account active = accountManager.activeAccount();
        if (active == null) {
            flyouts.showToast("Add an account and set it active before launching Minecraft.",
                    FlyoutManager.ToastKind.WARNING);
            return;
        }
        showStatus("Preparing to launch " + selected.name() + " as " + active.name() + "...");
        runInBackground(() -> {
            try {
                AccountManager.LaunchProfile profile =
                        accountManager.activeLaunchProfile(microsoftAuthService, settings.azureClientId());
                Path log = selected.gameDirectory().resolve("launcher.log");
                long logOffset = Files.isRegularFile(log) ? Files.size(log) : 0;
                Process process = gameLauncher.launchAccount(selected, profile, progress ->
                        showStatus(progress.currentItem()));
                runningGames.put(selected.id(), new RunningGame(process, logOffset));
                setRuntimeState(selected, StatusIndicator.Status.BLUE);
                Platform.runLater(() -> showStatus("Minecraft is starting. Output: " + log));
                monitorLaunch(selected, process, logOffset);
            } catch (IOException | InterruptedException | IllegalArgumentException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                Platform.runLater(() -> {
                    setRuntimeState(selected, StatusIndicator.Status.GREY);
                    flyouts.showError("Minecraft launch failed", exception.getMessage());
                });
            }
        });
    }

    private void installInstance(Instance instance, LoaderSelection loaderSelection) {
        InstallingView installingView = new InstallingView(instance.name());
        setCenter(installingView);
        runInBackground(() -> {
            try {
                gameLauncher.install(instance, loaderSelection,
                        progress -> Platform.runLater(() -> installingView.update(progress)));
                Platform.runLater(() -> {
                    setCenter(content);
                    refresh();
                    showStatus("Installed " + instance.name());
                });
            } catch (IOException | InterruptedException | IllegalArgumentException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                Platform.runLater(() -> {
                    setCenter(content);
                    flyouts.showError("Minecraft installation failed",
                            "Could not install " + instance.name() + ": " + exception.getMessage());
                });
            }
        });
    }

    private void monitorLaunch(Instance instance, Process process, long initialLogOffset) {
        runInBackground(() -> {
            Path log = instance.gameDirectory().resolve("launcher.log");
            try (RandomAccessFile reader = new RandomAccessFile(log.toFile(), "r")) {
                long offset = Math.min(initialLogOffset, reader.length());
                boolean ready = false;
                while (process.isAlive()) {
                    long length = reader.length();
                    if (length < offset) {
                        offset = 0;
                    }
                    reader.seek(offset);
                    String line;
                    while ((line = reader.readLine()) != null) {
                        offset = reader.getFilePointer();
                        if (!ready && isGameReadyMarker(line)) {
                            ready = true;
                            setRuntimeState(instance, StatusIndicator.Status.GREEN);
                            Platform.runLater(() -> showStatus(instance.name() + " is ready."));
                            break;
                        }
                    }
                    if (!process.isAlive()) {
                        break;
                    }
                    Thread.sleep(250);
                }
                int exitCode = process.waitFor();
                runningGames.remove(instance.id());
                boolean requestedStop = requestedStops.remove(instance.id());
                if (exitCode == 0 || requestedStop) {
                    setRuntimeState(instance, StatusIndicator.Status.GREY);
                    Platform.runLater(() -> showStatus("Minecraft closed normally."));
                    return;
                }
                setRuntimeState(instance, StatusIndicator.Status.RED);
                Path crashReport = latestCrashReport(instance.gameDirectory());
                String detail = crashReport == null
                        ? "See " + log + "." : "Crash report: " + crashReport;
                String message = "Minecraft exited with code " + exitCode + ". " + detail;
                Platform.runLater(() -> {
                    showStatus(message);
                    flyouts.showError("Minecraft crashed", message);
                });
            } catch (IOException exception) {
                runningGames.remove(instance.id());
                setRuntimeState(instance, StatusIndicator.Status.RED);
                Platform.runLater(() ->
                        flyouts.showError("Could not monitor Minecraft", exception.getMessage()));
            } catch (InterruptedException exception) {
                Thread.currentThread().interrupt();
                runningGames.remove(instance.id());
                setRuntimeState(instance, StatusIndicator.Status.GREY);
                Platform.runLater(() -> showStatus("Stopped monitoring Minecraft process."));
            }
        });
    }

    static boolean isGameReadyMarker(String line) {
        return line.contains("Sound engine started")
                || line.contains("Reload complete")
                || line.contains("Minecraft is ready");
    }

    private void setRuntimeState(Instance instance, StatusIndicator.Status state) {
        runtimeStates.put(instance.id(), state);
        Platform.runLater(() -> {
            instanceList.refresh();
            updateLaunchSelectedButton();
        });
    }

    private void updateLaunchSelectedButton() {
        Instance selected = instanceList.getSelectionModel().getSelectedItem();
        if (selected == null) {
            launchSelectedButton.setText("Launch");
            IconAssets.apply(launchSelectedButton, "start", settings.darkMode());
            launchSelectedButton.setDisable(true);
            return;
        }
        StatusIndicator.Status state = runtimeStates.getOrDefault(selected.id(), StatusIndicator.Status.GREY);
        boolean running = state == StatusIndicator.Status.GREEN;
        launchSelectedButton.setText(running ? "Stop" : "Launch");
        IconAssets.apply(launchSelectedButton, running ? "stop" : "start", settings.darkMode());
        launchSelectedButton.setDisable(state == StatusIndicator.Status.BLUE);
    }

    private static Path latestCrashReport(Path gameDirectory) throws IOException {
        Path crashReports = gameDirectory.resolve("crash-reports");
        if (!Files.isDirectory(crashReports)) {
            return null;
        }
        try (var reports = Files.list(crashReports)) {
            Path newest = null;
            java.nio.file.attribute.FileTime newestTime = null;
            for (Path report : reports.filter(Files::isRegularFile).toList()) {
                java.nio.file.attribute.FileTime modified = Files.getLastModifiedTime(report);
                if (newestTime == null || modified.compareTo(newestTime) > 0) {
                    newest = report;
                    newestTime = modified;
                }
            }
            return newest;
        }
    }

    private void setSettings(Instance instance) {
        instanceSettingsHandler.accept(instance);
    }

    private void showRunningInstance(Instance instance) {
        RunningGame running = runningGames.get(instance.id());
        if (running == null || !running.process().isAlive()) {
            flyouts.showToast("This Minecraft instance is not currently running.", FlyoutManager.ToastKind.WARNING);
            return;
        }
        if (runningInstanceView != null) {
            runningInstanceView.close();
        }
        runningInstanceView = new RunningInstanceView(instance, running.logOffset(), () -> {
            if (runningInstanceView != null) {
                runningInstanceView.close();
                runningInstanceView = null;
            }
            setCenter(content);
        });
        TabPane tabs = (TabPane) runningInstanceView.getCenter();
        net.stratalauncher.ui.animations.PageTransition.attachTabTransitions(tabs);
        setCenter(runningInstanceView);
    }

    public void closeRunningInstanceScreen() {
        if (runningInstanceView != null) {
            runningInstanceView.close();
            runningInstanceView = null;
            setCenter(content);
        }
    }

    private void showStatus(String message) {
        Platform.runLater(() -> statusLabel.setText(message));
    }

    private static void runInBackground(Runnable action) {
        Thread.ofVirtual().start(action);
    }

    private record RunningGame(Process process, long logOffset) {
    }

    private final class InstanceCardCell extends ListCell<Instance> {
        private final StatusIndicator indicator = new StatusIndicator();
        private final Label name = new Label();
        private final Label version = new Label();
        private final Button launchButton = new Button("Launch");
        private final Button settingsButton = new Button("Settings");
        private final Button consoleButton = new Button("Console");
        private final Button favoriteButton = new Button("Favorite");
        private final VBox labels = new VBox(8, name, version);
        private final HBox card = new HBox(14, indicator, labels, launchButton, settingsButton,
                consoleButton, favoriteButton);
        private final Scale scale = new Scale(1, 1);
        private final ScaleTransition hoverTransition = new ScaleTransition(MotionConstants.FAST, card);

        private InstanceCardCell() {
            card.getStyleClass().add("instance-card");
            card.getTransforms().add(scale);
            scale.setX(0.995);
            scale.setY(0.995);
            hoverTransition.setInterpolator(MotionConstants.STANDARD);
            card.setAlignment(Pos.CENTER_LEFT);
            card.setPadding(new Insets(16));
            labels.setAlignment(Pos.CENTER_LEFT);
            HBox.setHgrow(labels, Priority.ALWAYS);
            name.getStyleClass().add("instance-card-name");
            version.getStyleClass().add("instance-card-version");
            launchButton.getStyleClass().add("primary-button");
            settingsButton.getStyleClass().add("action-button");
            IconAssets.apply(settingsButton, "settings", settings.darkMode());
            IconAssets.apply(consoleButton, "console", settings.darkMode());
            IconAssets.apply(favoriteButton, "favorite", settings.darkMode());
            favoriteButton.getStyleClass().add("action-button");
            consoleButton.setOnAction(event -> {
                Instance selected = getItem();
                if (selected != null) {
                    showRunningInstance(selected);
                }
                event.consume();
            });
            favoriteButton.setOnAction(event -> {
                Instance selected = getItem();
                if (selected != null) {
                    try {
                        instanceManager.setFavorite(selected, !selected.favorite());
                        refresh();
                    } catch (IOException exception) {
                        flyouts.showError("Could not update favorite", exception.getMessage());
                    }
                }
                event.consume();
            });
            launchButton.setOnAction(event -> {
                Instance selected = getItem();
                if (selected != null) {
                    instanceList.getSelectionModel().select(selected);
                    if (runtimeStates.getOrDefault(selected.id(), StatusIndicator.Status.GREY)
                            == StatusIndicator.Status.GREEN) {
                        stopInstance(selected);
                    } else {
                        launch(selected);
                    }
                }
                event.consume();
            });
            settingsButton.setOnAction(event -> {
                Instance selected = getItem();
                if (selected != null) {
                    instanceList.getSelectionModel().select(selected);
                    setSettings(selected);
                }
                event.consume();
            });
            card.setOnMouseClicked(event -> {
                if (getItem() != null) {
                    instanceList.getSelectionModel().select(getItem());
                }
                event.consume();
            });
            instanceList.getSelectionModel().selectedItemProperty().addListener((observable, oldValue, selected) ->
                    updateSelection(getItem() != null && selected != null
                            && getItem().id().equals(selected.id())));
            selectedProperty().addListener((observable, oldValue, selected) ->
                    updateSelection(selected && getItem() != null));
            card.layoutBoundsProperty().addListener((observable, oldBounds, bounds) -> {
                scale.setPivotX(bounds.getWidth() / 2);
                scale.setPivotY(bounds.getHeight() / 2);
            });
            card.setOnMouseEntered(event -> animateHover(true));
            card.setOnMouseExited(event -> animateHover(false));
        }

        @Override
        protected void updateItem(Instance instance, boolean empty) {
            super.updateItem(instance, empty);
            if (empty || instance == null) {
                animateHover(false);
                setGraphic(null);
                setText(null);
                return;
            }
            name.setText(instance.name());
            version.setText("Minecraft " + instance.versionId());
            indicator.setStatus(runtimeStates.getOrDefault(instance.id(), StatusIndicator.Status.GREY));
            StatusIndicator.Status state = runtimeStates.getOrDefault(instance.id(), StatusIndicator.Status.GREY);
            boolean running = state == StatusIndicator.Status.GREEN;
            IconAssets.apply(launchButton, running ? "stop" : "start", settings.darkMode());
            launchButton.setText(running ? "Stop" : "Launch");
            launchButton.setAccessibleText(running ? "Stop " + instance.name() : "Launch " + instance.name());
            launchButton.setTooltip(new javafx.scene.control.Tooltip(
                    running ? "Stop Minecraft" : "Launch Minecraft"));
            launchButton.setDisable(state == StatusIndicator.Status.BLUE);
            consoleButton.setDisable(state != StatusIndicator.Status.BLUE && state != StatusIndicator.Status.GREEN);
            favoriteButton.setText(instance.favorite() ? "Favorited" : "Favorite");
            updateSelection(instanceList.getSelectionModel().getSelectedItem() != null
                    && instance.id().equals(instanceList.getSelectionModel().getSelectedItem().id()));
            setGraphic(card);
            setText(null);
        }

        private void updateSelection(boolean selected) {
            card.getStyleClass().remove("instance-card-selected");
            if (selected && !card.getStyleClass().contains("instance-card-selected")) {
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
    }
}
