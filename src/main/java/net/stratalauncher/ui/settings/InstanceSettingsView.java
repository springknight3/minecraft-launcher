package net.stratalauncher.ui.settings;

import javafx.collections.FXCollections;
import javafx.application.Platform;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.Button;
import javafx.scene.control.CheckBox;
import javafx.scene.control.ComboBox;
import javafx.scene.control.Label;
import javafx.scene.control.ListCell;
import javafx.scene.control.ListView;
import javafx.scene.control.Slider;
import javafx.scene.control.ScrollPane;
import javafx.scene.control.Spinner;
import javafx.scene.control.SpinnerValueFactory;
import javafx.scene.control.Tab;
import javafx.scene.control.TabPane;
import javafx.scene.control.TextArea;
import javafx.scene.control.TextField;
import javafx.scene.input.KeyCode;
import javafx.scene.input.KeyEvent;
import javafx.scene.input.MouseButton;
import javafx.scene.input.MouseEvent;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.GridPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;
import net.stratalauncher.core.auth.LauncherSettings;
import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.instance.Instance;
import net.stratalauncher.core.instance.InstanceManager;
import net.stratalauncher.core.runtime.JavaRuntimeManager;
import net.stratalauncher.core.launch.GameLauncher;
import net.stratalauncher.core.settings.InstanceSettings;
import net.stratalauncher.core.settings.MinecraftControls;
import net.stratalauncher.core.settings.MinecraftOptionValues;
import net.stratalauncher.core.settings.OptionsParser;
import net.stratalauncher.core.settings.ServerListStore;
import net.stratalauncher.ui.FlyoutManager;
import net.stratalauncher.ui.IconAssets;
import net.stratalauncher.ui.ToggleSwitchStyle;
import net.stratalauncher.ui.animations.PageTransition;

import java.io.IOException;
import java.nio.file.Path;
import java.util.LinkedHashMap;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.function.Consumer;

public final class InstanceSettingsView extends BorderPane {
    private final LauncherSettings launcherSettings;
    private final GameLauncher gameLauncher;
    private final InstanceManager instanceManager;
    private final JavaRuntimeManager javaRuntimeManager;
    private final FlyoutManager flyouts;
    private final Label title = new Label("Instance settings");
    private final TextField instanceName = new TextField();
    private final Label status = new Label();
    private final Label optionValueFormat = new Label();
    private final Label optionsStatus = new Label();
    private final Label advancedOptionsStatus = new Label();
    private final ComboBox<String> optionKeys = new ComboBox<>();
    private final TextField optionKey = new TextField();
    private final TextField optionValue = new TextField();
    private final Slider fovSlider = new Slider(30, 110, 70);
    private final Label fovValue = new Label("70°");
    private final Slider gammaSlider = new Slider(0, 1, 0);
    private final Label gammaValue = new Label("0.00");
    private final Spinner<Integer> renderDistance = integerSpinner(2, 128, 12);
    private final Spinner<Integer> simulationDistance = integerSpinner(2, 128, 12);
    private final Spinner<Integer> maximumFps = integerSpinner(10, 500, 120);
    private final Spinner<Integer> guiScale = integerSpinner(0, 16, 0);
    private final CheckBox unlimitedFps = new CheckBox("Unlimited");
    private final CheckBox fullscreen = new CheckBox();
    private final CheckBox vsync = new CheckBox();
    private final ListView<ServerListStore.Server> serverList = new ListView<>();
    private final VBox controlRows = new VBox(8);
    private final Label controlsStatus = new Label();
    private final Map<String, Button> controlButtons = new LinkedHashMap<>();
    private final CheckBox overrideFlags = new CheckBox("Use instance-specific Java flags");
    private final TextArea javaFlags = new TextArea();
    private final ComboBox<RuntimeChoice> javaRuntimeChoice = new ComboBox<>();
    private final Label javaRuntimeStatus = new Label();
    private final Button installRuntime = new Button("Install managed runtime for this version");
    private Instance instance;
    private InstanceSettings instanceSettings;
    private ServerListStore serverStore;
    private Map<String, String> options = Map.of();
    private Map<String, String> controls = Map.of();
    private final Set<String> changedControls = new HashSet<>();
    private String capturingBinding;
    private TabPane settingsTabs;
    private Consumer<Instance> installHandler = ignored -> { };
    private Runnable deleteHandler = () -> { };

    public InstanceSettingsView(LauncherSettings launcherSettings, GameLauncher gameLauncher,
                                InstanceManager instanceManager, JavaRuntimeManager javaRuntimeManager,
                                FlyoutManager flyouts) {
        this.launcherSettings = launcherSettings;
        this.gameLauncher = gameLauncher;
        this.instanceManager = instanceManager;
        this.javaRuntimeManager = javaRuntimeManager;
        this.flyouts = flyouts;
        ToggleSwitchStyle.apply(unlimitedFps);
        ToggleSwitchStyle.apply(overrideFlags);
        setPadding(new Insets(10));
        getStyleClass().add("instances-view");
        title.getStyleClass().add("hero-title");
        status.setWrapText(true);
        status.getStyleClass().add("hero-subtitle");

        Tab optionsTab = new Tab("Game Options", createOptionsTab());
        optionsTab.setGraphic(IconAssets.graphic("options", launcherSettings.darkMode()));
        Tab controlsTab = new Tab("Controls", createControlsTab());
        controlsTab.setGraphic(IconAssets.graphic("controls", launcherSettings.darkMode()));
        Tab serversTab = new Tab("Servers", createServersTab());
        serversTab.setGraphic(IconAssets.graphic("server", launcherSettings.darkMode()));
        Tab runtimeTab = new Tab("Java Runtime", createJavaRuntimeTab());
        runtimeTab.setGraphic(IconAssets.graphic("runtime", launcherSettings.darkMode()));
        Tab flagsTab = new Tab("Java Flags", createJavaFlagsTab());
        flagsTab.setGraphic(IconAssets.graphic("flags", launcherSettings.darkMode()));
        Tab dangerousTab = new Tab("Dangerous", createDangerousTab());
        dangerousTab.setGraphic(IconAssets.graphic("dangerous", launcherSettings.darkMode()));
        settingsTabs = new TabPane(optionsTab, controlsTab, serversTab, runtimeTab, flagsTab, dangerousTab);
        for (Tab tab : settingsTabs.getTabs()) {
            tab.setClosable(false);
        }
        PageTransition.attachTabTransitions(settingsTabs);
        Button rename = new Button("Save name");
        IconAssets.apply(rename, "save", launcherSettings.darkMode());
        rename.getStyleClass().add("action-button");
        rename.setOnAction(event -> renameInstance());
        HBox nameRow = new HBox(10, new Label("Instance name"), instanceName, rename);
        nameRow.setAlignment(Pos.CENTER_LEFT);
        HBox.setHgrow(instanceName, Priority.ALWAYS);
        VBox content = new VBox(12, title, nameRow, settingsTabs, status);
        VBox.setVgrow(settingsTabs, Priority.ALWAYS);
        setCenter(content);
        addEventFilter(KeyEvent.KEY_PRESSED, this::captureKeyboardBinding);
        addEventFilter(MouseEvent.MOUSE_PRESSED, this::captureMouseBinding);
    }

    public void setInstallHandler(Consumer<Instance> handler) {
        installHandler = handler == null ? ignored -> { } : handler;
    }

    public void setDeleteHandler(Runnable handler) {
        deleteHandler = handler == null ? () -> { } : handler;
    }

    public void refreshIcons() {
        IconAssets.refresh(this, launcherSettings.darkMode());
        ToggleSwitchStyle.refresh(this, launcherSettings.darkMode());
        settingsTabs.getTabs().get(0).setGraphic(IconAssets.graphic("options", launcherSettings.darkMode()));
        settingsTabs.getTabs().get(1).setGraphic(IconAssets.graphic("controls", launcherSettings.darkMode()));
        settingsTabs.getTabs().get(2).setGraphic(IconAssets.graphic("server", launcherSettings.darkMode()));
        settingsTabs.getTabs().get(3).setGraphic(IconAssets.graphic("runtime", launcherSettings.darkMode()));
        settingsTabs.getTabs().get(4).setGraphic(IconAssets.graphic("flags", launcherSettings.darkMode()));
        settingsTabs.getTabs().get(5).setGraphic(IconAssets.graphic("dangerous", launcherSettings.darkMode()));
    }

    public void showInstance(Instance selected) {
        instance = selected;
        changedControls.clear();
        title.setText(selected.name() + " settings");
        instanceName.setText(selected.name());
        try {
            instanceSettings = new InstanceSettings(selected);
            loadFlags();
            loadJavaRuntime();
            loadOptions();
            loadServers();
            status.setText("Editing settings for " + selected.name() + ".");
        } catch (IOException exception) {
            status.setText("Could not load instance settings: " + exception.getMessage());
        }
    }

    private void renameInstance() {
        if (instance == null) {
            return;
        }
        try {
            instance = instanceManager.rename(instance, instanceName.getText());
            instanceName.setText(instance.name());
            title.setText(instance.name() + " settings");
            status.setText("Instance renamed to " + instance.name() + ".");
        } catch (IOException | IllegalArgumentException exception) {
            status.setText("Could not rename instance: " + exception.getMessage());
        }
    }

    private VBox createJavaFlagsTab() {
        Label description = new Label("Advanced launch settings for this instance. Most players should leave "
                + "these unchanged. Unsupported settings can stop Minecraft from starting or cause crashes.");
        description.setWrapText(true);
        javaFlags.setPromptText("-Xmx4G\n-XX:+UseG1GC");
        javaFlags.setPrefRowCount(12);
        javaFlags.setWrapText(false);
        Label warning = new Label("Warning: changing these values without knowing what they do may make the game "
                + "unstable or prevent it from launching. Change one setting at a time so it is easier to undo.");
        warning.setWrapText(true);
        warning.getStyleClass().add("warning-banner");
        VBox.setVgrow(javaFlags, Priority.ALWAYS);
        overrideFlags.selectedProperty().addListener((observable, oldValue, selected) -> {
            javaFlags.setEditable(selected);
            if (selected && !oldValue) {
                javaFlags.setText(String.join("\n", launcherSettings.globalJavaLaunchFlags()));
            }
            updateFlagStatus();
        });
        Button save = new Button("Save Java flags");
        IconAssets.apply(save, "save", launcherSettings.darkMode());
        save.getStyleClass().add("primary-button");
        save.setOnAction(event -> saveFlags());
        return new VBox(10, description, warning, overrideFlags, javaFlags, save);
    }

    private VBox createJavaRuntimeTab() {
        javaRuntimeChoice.setItems(FXCollections.observableArrayList(RuntimeChoice.values()));
        javaRuntimeChoice.setMaxWidth(Double.MAX_VALUE);
        javaRuntimeChoice.setCellFactory(ignored -> new javafx.scene.control.ListCell<>() {
            @Override
            protected void updateItem(RuntimeChoice choice, boolean empty) {
                super.updateItem(choice, empty);
                setText(empty || choice == null ? null : choice.label());
            }
        });
        javaRuntimeChoice.setButtonCell(new javafx.scene.control.ListCell<>() {
            @Override
            protected void updateItem(RuntimeChoice choice, boolean empty) {
                super.updateItem(choice, empty);
                setText(empty || choice == null ? null : choice.label());
            }
        });
        Label description = new Label(
                "Choose which Java version this game uses. The recommended choice is the version provided for Minecraft.");
        description.setWrapText(true);
        Label systemVersion = new Label("Launcher Java: " + System.getProperty("java.version", "unknown"));
        javaRuntimeStatus.setWrapText(true);
        Button save = new Button("Save runtime choice");
        IconAssets.apply(save, "save", launcherSettings.darkMode());
        save.getStyleClass().add("primary-button");
        save.setOnAction(event -> saveRuntimeChoice());
        installRuntime.getStyleClass().add("action-button");
        IconAssets.apply(installRuntime, "download", launcherSettings.darkMode());
        installRuntime.setOnAction(event -> installManagedRuntime());
        return new VBox(12,
                description,
                new Label("Runtime for this instance"),
                javaRuntimeChoice,
                systemVersion,
                save,
                installRuntime,
                javaRuntimeStatus);
    }

    private void loadJavaRuntime() {
        if (instanceSettings == null) {
            return;
        }
        javaRuntimeChoice.setValue(instanceSettings.javaRuntimeMode() == InstanceSettings.JavaRuntimeMode.MANAGED
                ? RuntimeChoice.MANAGED : RuntimeChoice.SYSTEM);
        if (instanceSettings.javaRuntimeMode() != InstanceSettings.JavaRuntimeMode.MANAGED) {
            javaRuntimeStatus.setText("This instance is set to use System Java.");
            return;
        }
        try {
            String component = instanceSettings.javaRuntimeComponent();
            if (component == null || component.isBlank()) {
                component = javaRuntimeManager.runtimeComponent(instance, LauncherSettings.RuntimeVendor.MOJANG);
            }
            javaRuntimeStatus.setText(javaRuntimeManager.isInstalled(instance.directory(), component)
                    ? "Managed Java runtime is installed for Minecraft " + instance.versionId() + "."
                    : "Managed Java runtime is not installed yet. Use the install button below.");
        } catch (IOException exception) {
            javaRuntimeStatus.setText("Could not check the managed Java runtime: " + exception.getMessage());
        }
    }

    private void saveRuntimeChoice() {
        if (instanceSettings == null || javaRuntimeChoice.getValue() == null) {
            return;
        }
        try {
            instanceSettings.setJavaRuntimeMode(javaRuntimeChoice.getValue().mode());
            javaRuntimeStatus.setText(javaRuntimeChoice.getValue() == RuntimeChoice.SYSTEM
                    ? "This instance will use System Java."
                    : "This instance will use the managed runtime. Install it if needed.");
            status.setText("Java runtime choice saved.");
        } catch (IOException | IllegalArgumentException exception) {
            javaRuntimeStatus.setText("Could not save Java runtime choice: " + exception.getMessage());
            status.setText(javaRuntimeStatus.getText());
        }
    }

    private void installManagedRuntime() {
        if (instance == null || instanceSettings == null) {
            return;
        }
        Instance selected = instance;
        InstanceSettings selectedSettings = instanceSettings;
        installRuntime.setDisable(true);
        javaRuntimeStatus.setText("Preparing the selected Java runtime for Minecraft " + selected.versionId() + "…");
        CompletableFuture.runAsync(() -> {
            try {
                gameLauncher.installInstanceJavaRuntime(selected,
                        progress -> updateRuntimeProgress(selected, progress));
                selectedSettings.setJavaRuntimeMode(InstanceSettings.JavaRuntimeMode.MANAGED);
                Platform.runLater(() -> {
                    if (instance != null && instance.id().equals(selected.id())) {
                        loadJavaRuntime();
                        javaRuntimeChoice.setValue(RuntimeChoice.MANAGED);
                        javaRuntimeStatus.setText("Managed Java runtime is installed and selected for this instance.");
                        status.setText("Managed Java runtime installed.");
                    }
                    installRuntime.setDisable(false);
                });
            } catch (IOException | InterruptedException | IllegalArgumentException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
                String message = "Could not install managed Java runtime: " + exception.getMessage();
                Platform.runLater(() -> {
                    if (instance != null && instance.id().equals(selected.id())) {
                        javaRuntimeStatus.setText(message);
                        status.setText(message);
                    }
                    installRuntime.setDisable(false);
                });
            }
        });
    }

    private void updateRuntimeProgress(Instance selected, DownloadManager.Progress progress) {
        Platform.runLater(() -> {
            if (instance != null && instance.id().equals(selected.id())) {
                javaRuntimeStatus.setText(progress.currentItem());
            }
        });
    }

    private VBox createOptionsTab() {
        TabPane optionsTabs = new TabPane(
                new Tab("Common options", createCommonOptionsTab()),
                new Tab("Advanced", createAdvancedOptionsTab()));
        optionsTabs.setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);
        VBox.setVgrow(optionsTabs, Priority.ALWAYS);
        return new VBox(10, optionsTabs);
    }

    private VBox createDangerousTab() {
        Label warning = new Label("These actions can replace game files or permanently remove this instance "
                + "and its worlds, settings, and installed content.");
        warning.setWrapText(true);
        Button install = new Button("Install / repair Minecraft");
        install.getStyleClass().add("danger-button");
        IconAssets.apply(install, "download", launcherSettings.darkMode());
        install.setOnAction(event -> {
            if (instance != null) {
                Instance selected = instance;
                flyouts.showConfirmation("Install or repair instance",
                        "Reinstall or repair Minecraft files for " + selected.name()
                                + "? Existing files may be replaced.", confirmed -> {
                            if (confirmed) {
                                installHandler.accept(selected);
                            }
                        });
            }
        });
        Button delete = new Button("Delete instance");
        delete.getStyleClass().add("danger-button");
        IconAssets.apply(delete, "delete", launcherSettings.darkMode());
        delete.setOnAction(event -> {
            if (instance == null) {
                return;
            }
            Instance selected = instance;
            flyouts.showTypedDeleteConfirmation("Delete instance",
                    "Permanently delete " + selected.name()
                            + " and all of its game files, worlds, settings, and content? This cannot be undone.",
                    confirmed -> {
                        if (confirmed) {
                            CompletableFuture.runAsync(() -> {
                                try {
                                    instanceManager.delete(selected);
                                    Platform.runLater(deleteHandler);
                                } catch (IOException exception) {
                                    Platform.runLater(() -> flyouts.showError("Could not delete instance",
                                            exception.getMessage()));
                                }
                            });
                        }
                    });
        });
        VBox actions = new VBox(14,
                new Label("Install or repair"), install,
                new Label("Permanently delete"), delete);
        actions.setPadding(new Insets(14));
        actions.getStyleClass().add("dangerous-panel");
        VBox content = new VBox(16, warning, actions);
        content.setPadding(new Insets(12));
        return content;
    }

    private VBox createControlsTab() {
        Label description = new Label("Choose Rebind, then press the keyboard key or mouse button you want to use. "
                + "If two actions share a key, the conflict will be shown here.");
        description.setWrapText(true);
        controlRows.setPadding(new Insets(8));
        ScrollPane scrollPane = new ScrollPane(controlRows);
        scrollPane.setFitToWidth(true);
        VBox.setVgrow(scrollPane, Priority.ALWAYS);
        controlsStatus.setWrapText(true);
        Button save = new Button("Save controls");
        IconAssets.apply(save, "save", launcherSettings.darkMode());
        save.getStyleClass().add("primary-button");
        save.setOnAction(event -> saveControls());
        return new VBox(10, description, scrollPane, save, controlsStatus);
    }

    private void refreshControlRows() {
        controlRows.getChildren().clear();
        controlButtons.clear();
        Map<String, List<String>> conflicts = MinecraftControls.conflicts(controls);
        for (Map.Entry<String, String> control : controls.entrySet().stream()
                .sorted(Map.Entry.comparingByKey()).toList()) {
            HBox row = new HBox(12);
            row.setAlignment(Pos.CENTER_LEFT);
            Label action = new Label(MinecraftControls.actionName(control.getKey()));
            action.setMinWidth(210);
            Button rebind = new Button();
            rebind.setMinWidth(180);
            rebind.setOnAction(event -> beginBindingCapture(control.getKey()));
            controlButtons.put(control.getKey(), rebind);
            Button unassign = new Button("Unassign");
            unassign.setOnAction(event -> unassignBinding(control.getKey()));
            row.getChildren().addAll(action, rebind, unassign);
            List<String> conflictingKeys = conflicts.getOrDefault(control.getValue(), List.of()).stream()
                    .filter(key -> !key.equals(control.getKey()))
                    .sorted()
                    .toList();
            if (!conflictingKeys.isEmpty()) {
                Label conflict = new Label("Conflicts with "
                        + conflictingKeys.stream().map(MinecraftControls::actionName)
                        .reduce((first, second) -> first + ", " + second).orElse(""));
                conflict.setWrapText(true);
                row.getChildren().add(conflict);
            }
            controlRows.getChildren().add(row);
        }
        updateControlButtonLabels();
    }

    private void updateControlButtonLabels() {
        for (Map.Entry<String, Button> entry : controlButtons.entrySet()) {
            String label = entry.getKey().equals(capturingBinding)
                    ? "Press a key…"
                    : MinecraftControls.displayKey(controls.get(entry.getKey()));
            entry.getValue().setText(label);
        }
    }

    private void beginBindingCapture(String optionKey) {
        capturingBinding = optionKey;
        controlsStatus.setText("Press a key or mouse button for "
                + MinecraftControls.actionName(optionKey) + ". Press Escape to cancel.");
        updateControlButtonLabels();
    }

    private void captureKeyboardBinding(KeyEvent event) {
        if (capturingBinding == null) {
            return;
        }
        if (event.getCode() == KeyCode.ESCAPE) {
            cancelBindingCapture();
            event.consume();
            return;
        }
        String value = minecraftKeyboardValue(event.getCode());
        if (value == null) {
            controlsStatus.setText("That key is not supported. Press a keyboard key or Escape to cancel.");
            event.consume();
            return;
        }
        finishBindingCapture(value);
        event.consume();
    }

    private void captureMouseBinding(MouseEvent event) {
        if (capturingBinding == null) {
            return;
        }
        String value = switch (event.getButton()) {
            case PRIMARY -> "key.mouse.left";
            case SECONDARY -> "key.mouse.right";
            case MIDDLE -> "key.mouse.middle";
            case BACK -> "key.mouse.4";
            case FORWARD -> "key.mouse.5";
            default -> null;
        };
        if (value != null) {
            finishBindingCapture(value);
            event.consume();
        }
    }

    private void finishBindingCapture(String value) {
        Map<String, String> updated = new LinkedHashMap<>(controls);
        updated.put(capturingBinding, value);
        controls = Map.copyOf(updated);
        changedControls.add(capturingBinding);
        String action = MinecraftControls.actionName(capturingBinding);
        capturingBinding = null;
        refreshControlRows();
        controlsStatus.setText(action + " is now bound to " + MinecraftControls.displayKey(value) + ".");
    }

    private void cancelBindingCapture() {
        capturingBinding = null;
        updateControlButtonLabels();
        controlsStatus.setText("Key capture cancelled.");
    }

    private void unassignBinding(String optionKey) {
        Map<String, String> updated = new LinkedHashMap<>(controls);
        updated.put(optionKey, "key.keyboard.unknown");
        controls = Map.copyOf(updated);
        changedControls.add(optionKey);
        capturingBinding = null;
        refreshControlRows();
        controlsStatus.setText(MinecraftControls.actionName(optionKey) + " is unassigned.");
    }

    private void saveControls() {
        if (instance == null) {
            return;
        }
        if (changedControls.isEmpty()) {
            controlsStatus.setText("No keybind changes to save.");
            return;
        }
        try {
            Map<String, String> changes = new LinkedHashMap<>();
            changedControls.stream().sorted().forEach(key -> changes.put(key, controls.get(key)));
            OptionsParser.setAll(instance.gameDirectory().resolve("options.txt"), changes);
            loadOptions();
            changedControls.clear();
            controlsStatus.setText("Controls saved to options.txt. Conflicts can still be saved if intentional.");
            status.setText("Instance controls saved.");
        } catch (IOException | IllegalArgumentException exception) {
            controlsStatus.setText("Could not save controls: " + exception.getMessage());
            status.setText(controlsStatus.getText());
        }
    }

    private static String minecraftKeyboardValue(KeyCode keyCode) {
        String key = keyCode.name();
        if (key.matches("[A-Z]")) {
            return "key.keyboard." + key.toLowerCase(Locale.ROOT);
        }
        if (key.matches("DIGIT[0-9]")) {
            return "key.keyboard." + key.substring("DIGIT".length());
        }
        if (key.matches("F([1-9]|1[0-9]|2[0-5])")) {
            return "key.keyboard." + key.toLowerCase(Locale.ROOT);
        }
        String mapped = switch (key) {
            case "SPACE" -> "space";
            case "ENTER" -> "enter";
            case "BACK_SPACE" -> "backspace";
            case "TAB" -> "tab";
            case "ESCAPE" -> "escape";
            case "DELETE" -> "delete";
            case "INSERT" -> "insert";
            case "HOME" -> "home";
            case "END" -> "end";
            case "PAGE_UP" -> "page.up";
            case "PAGE_DOWN" -> "page.down";
            case "UP" -> "up";
            case "DOWN" -> "down";
            case "LEFT" -> "left";
            case "RIGHT" -> "right";
            case "SHIFT" -> "left.shift";
            case "CONTROL" -> "left.control";
            case "ALT" -> "left.alt";
            case "META" -> "left.super";
            case "CAPS" -> "caps.lock";
            case "NUM_LOCK" -> "num.lock";
            case "SCROLL_LOCK" -> "scroll.lock";
            case "PRINTSCREEN" -> "print.screen";
            case "PAUSE" -> "pause";
            case "BACK_QUOTE" -> "grave.accent";
            case "QUOTE" -> "apostrophe";
            case "COMMA" -> "comma";
            case "MINUS" -> "minus";
            case "PERIOD" -> "period";
            case "SLASH" -> "slash";
            case "SEMICOLON" -> "semicolon";
            case "EQUALS" -> "equal";
            case "OPEN_BRACKET" -> "left.bracket";
            case "CLOSE_BRACKET" -> "right.bracket";
            case "BACK_SLASH" -> "backslash";
            case "NUMPAD0" -> "keypad.0";
            case "NUMPAD1" -> "keypad.1";
            case "NUMPAD2" -> "keypad.2";
            case "NUMPAD3" -> "keypad.3";
            case "NUMPAD4" -> "keypad.4";
            case "NUMPAD5" -> "keypad.5";
            case "NUMPAD6" -> "keypad.6";
            case "NUMPAD7" -> "keypad.7";
            case "NUMPAD8" -> "keypad.8";
            case "NUMPAD9" -> "keypad.9";
            case "ADD" -> "keypad.add";
            case "SUBTRACT" -> "keypad.subtract";
            case "MULTIPLY" -> "keypad.multiply";
            case "DIVIDE" -> "keypad.divide";
            case "DECIMAL" -> "keypad.decimal";
            case "NUMPAD_ENTER" -> "keypad.enter";
            default -> null;
        };
        return mapped == null ? null : "key.keyboard." + mapped;
    }

    private VBox createCommonOptionsTab() {
        fovSlider.setMajorTickUnit(10);
        fovSlider.setMinorTickCount(9);
        fovSlider.setShowTickMarks(true);
        fovSlider.setShowTickLabels(true);
        fovSlider.valueProperty().addListener((observable, oldValue, value) ->
                fovValue.setText(Math.round(value.doubleValue()) + "°"));
        gammaSlider.setMajorTickUnit(0.25);
        gammaSlider.setMinorTickCount(4);
        gammaSlider.setShowTickMarks(true);
        gammaSlider.valueProperty().addListener((observable, oldValue, value) ->
                gammaValue.setText(String.format(Locale.ROOT, "%.2f", value.doubleValue())));
        maximumFps.setEditable(true);
        unlimitedFps.selectedProperty().addListener((observable, oldValue, selected) ->
                maximumFps.setDisable(selected));
        fullscreen.setText("Start in fullscreen");
        vsync.setText("Vertical synchronization");
        ToggleSwitchStyle.apply(fullscreen);
        ToggleSwitchStyle.apply(vsync);

        GridPane settingsGrid = new GridPane();
        settingsGrid.setHgap(16);
        settingsGrid.setVgap(18);
        settingsGrid.setPadding(new Insets(18));
        settingsGrid.addRow(0, new Label("Field of view"), new HBox(10, fovSlider, fovValue));
        settingsGrid.addRow(1, new Label("Gamma / brightness"), new HBox(10, gammaSlider, gammaValue));
        settingsGrid.addRow(2, new Label("Render distance (chunks)"), renderDistance);
        settingsGrid.addRow(3, new Label("Simulation distance (chunks)"), simulationDistance);
        settingsGrid.addRow(4, new Label("Maximum frame rate"), new HBox(10, maximumFps, unlimitedFps));
        settingsGrid.addRow(5, new Label("GUI scale (0 = Auto)"), guiScale);
        settingsGrid.addRow(6, new Label("Display"), new VBox(8, fullscreen, vsync));
        settingsGrid.getChildren().forEach(node -> GridPane.setMargin(node, new Insets(8, 10, 8, 10)));
        GridPane.setHgrow(settingsGrid.getChildren().get(1), Priority.ALWAYS);
        GridPane.setHgrow(settingsGrid.getChildren().get(3), Priority.ALWAYS);
        Label description = new Label("Adjust graphics and display options for this game. Some choices may be "
                + "limited by your Minecraft version or computer.");
        description.setWrapText(true);
        optionValueFormat.setWrapText(true);
        optionsStatus.setWrapText(true);
        advancedOptionsStatus.setWrapText(true);
        Button save = new Button("Save common options");
        IconAssets.apply(save, "save", launcherSettings.darkMode());
        save.getStyleClass().add("primary-button");
        save.setOnAction(event -> saveCommonOptions());
        VBox content = new VBox(12, description, optionValueFormat, settingsGrid, save, optionsStatus);
        VBox.setVgrow(settingsGrid, Priority.ALWAYS);
        return content;
    }

    private VBox createAdvancedOptionsTab() {
        Label description = new Label("Edit an additional Minecraft setting by entering its name and value. "
                + "These advanced values are best left unchanged unless you know what they do.");
        description.setWrapText(true);
        optionKeys.setPromptText("Select an existing option");
        optionKeys.setMaxWidth(Double.MAX_VALUE);
        optionKeys.valueProperty().addListener((observable, oldValue, key) -> {
            if (key != null) {
                optionKey.setText(key);
                optionValue.setText(options.getOrDefault(key, ""));
            }
        });
        optionKey.setPromptText("Option key");
        optionValue.setPromptText("Raw value");
        Button save = new Button("Save option");
        IconAssets.apply(save, "save", launcherSettings.darkMode());
        save.getStyleClass().add("primary-button");
        save.setOnAction(event -> saveOption());
        VBox fields = new VBox(10,
                new Label("Existing options"), optionKeys,
                new Label("Key"), optionKey,
                new Label("Raw value"), optionValue, save, advancedOptionsStatus);
        fields.setPadding(new Insets(14));
        return new VBox(10, description, fields);
    }

    private VBox createServersTab() {
        Label description = new Label("Managed multiplayer servers for this instance.");
        description.setWrapText(true);
        serverList.setCellFactory(ignored -> new ListCell<>() {
            @Override
            protected void updateItem(ServerListStore.Server server, boolean empty) {
                super.updateItem(server, empty);
                setText(empty || server == null ? null : server.name() + "  ·  " + server.address());
            }
        });
        serverList.setMinHeight(80);
        serverList.setPrefHeight(160);
        serverList.setMaxHeight(Double.MAX_VALUE);
        serverList.setPlaceholder(new Label("No saved servers."));
        VBox.setVgrow(serverList, Priority.ALWAYS);
        Button add = new Button("Add server");
        IconAssets.apply(add, "add", launcherSettings.darkMode());
        add.getStyleClass().add("primary-button");
        add.setOnAction(event -> addServer());
        Button update = new Button("Edit selected");
        IconAssets.apply(update, "save", launcherSettings.darkMode());
        update.getStyleClass().add("action-button");
        update.setOnAction(event -> editServer());
        Button remove = new Button("Remove selected");
        IconAssets.apply(remove, "delete", launcherSettings.darkMode());
        remove.getStyleClass().add("action-button");
        remove.setOnAction(event -> removeServer());
        update.disableProperty().bind(serverList.getSelectionModel().selectedItemProperty().isNull());
        remove.disableProperty().bind(serverList.getSelectionModel().selectedItemProperty().isNull());
        HBox actions = new HBox(8, add, update, remove);
        actions.setAlignment(Pos.CENTER_LEFT);
        VBox content = new VBox(10, description, serverList, actions);
        VBox.setVgrow(serverList, Priority.ALWAYS);
        return content;
    }

    private void loadFlags() {
        boolean overridden = instanceSettings.hasJavaLaunchFlagsOverride();
        overrideFlags.setSelected(overridden);
        javaFlags.setEditable(overridden);
        List<String> visibleFlags = overridden ? instanceSettings.javaLaunchFlagsOverride()
                : launcherSettings.globalJavaLaunchFlags();
        javaFlags.setText(String.join("\n", visibleFlags));
        updateFlagStatus();
    }

    private void updateFlagStatus() {
        if (instanceSettings == null) {
            return;
        }
        status.setText(overrideFlags.isSelected()
                ? "Java flags are overridden for this instance."
                : "Java flags are inherited from launcher settings.");
    }

    private void saveFlags() {
        if (instanceSettings == null) {
            return;
        }
        try {
            if (overrideFlags.isSelected()) {
                instanceSettings.setJavaLaunchFlagsOverride(lines(javaFlags.getText()));
            } else {
                instanceSettings.clearJavaLaunchFlagsOverride();
            }
            loadFlags();
            status.setText(overrideFlags.isSelected()
                    ? "Instance Java flags saved."
                    : "Instance now inherits global Java flags.");
        } catch (IOException | IllegalArgumentException exception) {
            status.setText("Could not save Java flags: " + exception.getMessage());
        }
    }

    private void loadOptions() throws IOException {
        options = OptionsParser.read(instance.gameDirectory().resolve("options.txt"));
        controls = MinecraftControls.bindings(options);
        refreshControlRows();
        loadCommonOptions();
        optionValueFormat.setText(MinecraftOptionValues.usesNormalizedFov(instance.versionId())
                ? "Enter FOV in degrees (30–110). The launcher converts it to the normalized value stored by Minecraft "
                        + instance.versionId() + ". Other values use Minecraft's raw key:value format."
                : "Values use Minecraft's raw key:value format and are not converted.");
        optionKeys.setItems(FXCollections.observableArrayList(options.keySet().stream().sorted().toList()));
        optionKey.clear();
        optionValue.clear();
        optionsStatus.setText("");
        advancedOptionsStatus.setText("");
    }

    private void loadCommonOptions() {
        int fov = parseInt(MinecraftOptionValues.toDisplayValue(instance.versionId(), "fov",
                options.getOrDefault("fov", "70")), 70);
        fovSlider.setValue(fov);
        gammaSlider.setValue(clamp(parseDouble(options.getOrDefault("gamma", "0.0"), 0), 0, 1));
        renderDistance.getValueFactory().setValue(clamp(
                parseInt(options.getOrDefault("renderDistance", "12"), 12), 2, 128));
        simulationDistance.getValueFactory().setValue(clamp(
                parseInt(options.getOrDefault("simulationDistance", "12"), 12), 2, 128));
        int maxFps = parseInt(options.getOrDefault("maxFps", "120"), 120);
        boolean unlimited = maxFps >= 260;
        unlimitedFps.setSelected(unlimited);
        maximumFps.setValueFactory(new SpinnerValueFactory.IntegerSpinnerValueFactory(
                10, 500, clamp(unlimited ? 120 : maxFps, 10, 500)));
        maximumFps.setDisable(unlimited);
        guiScale.getValueFactory().setValue(clamp(parseInt(options.getOrDefault("guiScale", "0"), 0), 0, 16));
        fullscreen.setSelected(Boolean.parseBoolean(options.getOrDefault("fullscreen", "false")));
        vsync.setSelected(Boolean.parseBoolean(options.getOrDefault("enableVsync", "true")));
    }

    private void saveCommonOptions() {
        if (instance == null) {
            return;
        }
        try {
            commitSpinner(renderDistance);
            commitSpinner(simulationDistance);
            commitSpinner(guiScale);
            if (!unlimitedFps.isSelected()) {
                commitSpinner(maximumFps);
            }
            int fov = (int) Math.round(fovSlider.getValue());
            Map<String, String> changes = new LinkedHashMap<>();
            changes.put("fov", MinecraftOptionValues.toStoredValue(instance.versionId(), "fov",
                    Integer.toString(fov)));
            changes.put("gamma", String.format(Locale.ROOT, "%.2f", gammaSlider.getValue()));
            changes.put("renderDistance", renderDistance.getValue().toString());
            changes.put("simulationDistance", simulationDistance.getValue().toString());
            changes.put("maxFps", unlimitedFps.isSelected() ? "260" : maximumFps.getValue().toString());
            changes.put("guiScale", guiScale.getValue().toString());
            changes.put("fullscreen", Boolean.toString(fullscreen.isSelected()));
            changes.put("enableVsync", Boolean.toString(vsync.isSelected()));
            Path optionsFile = instance.gameDirectory().resolve("options.txt");
            OptionsParser.setAll(optionsFile, changes);
            loadOptions();
            String message = "Common settings saved to " + optionsFile + ".";
            optionsStatus.setText(message);
            status.setText(message + " Unedited options.txt lines were retained.");
        } catch (IOException | IllegalArgumentException exception) {
            optionsStatus.setText("Could not save common options: " + exception.getMessage());
            status.setText(optionsStatus.getText());
        }
    }

    private void saveOption() {
        if (instance == null) {
            return;
        }
        try {
            String key = optionKey.getText();
            OptionsParser.set(instance.gameDirectory().resolve("options.txt"), key, optionValue.getText());
            loadOptions();
            optionKeys.getSelectionModel().select(key);
            String message = "Saved option '" + key + "' to "
                    + instance.gameDirectory().resolve("options.txt") + ".";
            advancedOptionsStatus.setText(message);
            status.setText(message + " Unedited options.txt lines were retained.");
        } catch (IOException | IllegalArgumentException exception) {
            advancedOptionsStatus.setText("Could not save the game option: " + exception.getMessage());
            status.setText(advancedOptionsStatus.getText());
        }
    }

    private void loadServers() throws IOException {
        serverStore = new ServerListStore(instance.gameDirectory().resolve("servers.dat"), instance.versionId());
        serverList.setItems(FXCollections.observableArrayList(serverStore.servers()));
    }

    private void addServer() {
        showServerDialog(-1, null);
    }

    private void editServer() {
        int index = serverList.getSelectionModel().getSelectedIndex();
        ServerListStore.Server server = serverList.getSelectionModel().getSelectedItem();
        if (index < 0 || server == null) {
            status.setText("Select a saved server to edit.");
            return;
        }
        showServerDialog(index, server);
    }

    private void showServerDialog(int index, ServerListStore.Server existing) {
        TextField name = new TextField(existing == null ? "" : existing.name());
        TextField address = new TextField(existing == null ? "" : existing.address());
        name.setPromptText("For example, My Survival Server");
        address.setPromptText("For example, play.example.net");
        VBox fields = new VBox(10,
                new Label("Display name"), name,
                new Label("Server address"), address);
        fields.setPadding(new Insets(10));
        String title = existing == null ? "Add multiplayer server" : "Edit multiplayer server";
        flyouts.show(title, existing == null ? "Enter the server name and address."
                : "Update the saved server details.", fields,
                new FlyoutManager.Action("Cancel", "action-button", () -> true),
                new FlyoutManager.Action(existing == null ? "Add server" : "Save changes",
                        "primary-button", () -> {
            if (name.getText().isBlank() || address.getText().isBlank()) {
                flyouts.showToast("Enter both a display name and server address.",
                        FlyoutManager.ToastKind.WARNING);
                return false;
            }
            if (serverStore == null) {
                flyouts.showToast("Instance server settings are not loaded.", FlyoutManager.ToastKind.ERROR);
                return false;
            }
            try {
                if (existing == null) {
                    serverStore.add(name.getText(), address.getText());
                } else {
                    serverStore.update(index, name.getText(), address.getText());
                }
                serverStore.save();
                loadServers();
                serverList.getSelectionModel().select(existing == null
                        ? serverList.getItems().size() - 1 : index);
                status.setText(existing == null ? "Server added." : "Server updated.");
                return true;
            } catch (IOException | IllegalArgumentException | IndexOutOfBoundsException exception) {
                flyouts.showError("Could not save server", exception.getMessage());
                return false;
            }
        }));
    }

    private void removeServer() {
        int index = serverList.getSelectionModel().getSelectedIndex();
        if (serverStore == null || index < 0) {
            status.setText("Select a saved server to remove.");
            return;
        }
        ServerListStore.Server selected = serverList.getSelectionModel().getSelectedItem();
        flyouts.showConfirmation("Remove saved server",
                "Remove " + selected.name() + " from this instance's saved servers?", confirmed -> {
            if (!confirmed) {
                return;
            }
            try {
                serverStore.remove(index);
                serverStore.save();
                loadServers();
                status.setText("Server removed.");
            } catch (IOException | IndexOutOfBoundsException exception) {
                flyouts.showError("Could not remove server", exception.getMessage());
            }
        });
    }

    private static Spinner<Integer> integerSpinner(int minimum, int maximum, int initialValue) {
        Spinner<Integer> spinner = new Spinner<>(
                new SpinnerValueFactory.IntegerSpinnerValueFactory(minimum, maximum, initialValue));
        spinner.setEditable(true);
        spinner.setPrefWidth(120);
        return spinner;
    }

    private static void commitSpinner(Spinner<Integer> spinner) {
        int value = Integer.parseInt(spinner.getEditor().getText().trim());
        SpinnerValueFactory.IntegerSpinnerValueFactory factory =
                (SpinnerValueFactory.IntegerSpinnerValueFactory) spinner.getValueFactory();
        if (value < factory.getMin() || value > factory.getMax()) {
            throw new IllegalArgumentException("Value must be between " + factory.getMin()
                    + " and " + factory.getMax() + ".");
        }
        factory.setValue(value);
    }

    private static int parseInt(String value, int fallback) {
        try {
            return (int) Double.parseDouble(value);
        } catch (NumberFormatException exception) {
            return fallback;
        }
    }

    private static double parseDouble(String value, double fallback) {
        try {
            return Double.parseDouble(value);
        } catch (NumberFormatException exception) {
            return fallback;
        }
    }

    private static int clamp(int value, int minimum, int maximum) {
        return Math.max(minimum, Math.min(maximum, value));
    }

    private static double clamp(double value, double minimum, double maximum) {
        return Math.max(minimum, Math.min(maximum, value));
    }

    private static List<String> lines(String text) {
        List<String> values = new ArrayList<>();
        for (String line : text.split("\\R")) {
            if (!line.isBlank()) {
                values.add(line);
            }
        }
        return values;
    }

    private enum RuntimeChoice {
        SYSTEM(InstanceSettings.JavaRuntimeMode.SYSTEM, "System Java"),
        MANAGED(InstanceSettings.JavaRuntimeMode.MANAGED, "Managed Java (provider set in Launcher Settings)");

        private final InstanceSettings.JavaRuntimeMode mode;
        private final String label;

        RuntimeChoice(InstanceSettings.JavaRuntimeMode mode, String label) {
            this.mode = mode;
            this.label = label;
        }

        private InstanceSettings.JavaRuntimeMode mode() {
            return mode;
        }

        private String label() {
            return label;
        }
    }
}
