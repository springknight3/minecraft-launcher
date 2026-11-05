package net.stratalauncher.ui.server;

import javafx.animation.FadeTransition;
import javafx.animation.TranslateTransition;
import javafx.application.Platform;
import javafx.geometry.Insets;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.TextArea;
import javafx.scene.control.TextField;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;
import javafx.util.Duration;
import net.stratalauncher.core.auth.LauncherSettings;
import net.stratalauncher.core.server.DevServerManager;
import net.stratalauncher.ui.IconAssets;
import net.stratalauncher.ui.MotionConstants;
import net.stratalauncher.ui.components.StatusIndicator;

import java.io.IOException;
import java.util.function.Consumer;

public final class ServerConsoleView extends VBox {
    private static final int MAX_DISPLAY_CHARACTERS = 100_000;

    private final DevServerManager serverManager;
    private final LauncherSettings settings;
    private final TextArea console = new TextArea();
    private final TextField command = new TextField();
    private final Label status = new Label("Select a server to view its console.");
    private final Label resources = new Label();
    private final StatusIndicator processIndicator = new StatusIndicator();
    private final Button start = new Button();
    private final Button stop = new Button();
    private final Button terminate = new Button();
    private DevServerManager.ServerConfig server;
    private Consumer<DevServerManager.ConsoleEvent> consoleListener;
    private Consumer<DevServerManager.ServerSnapshot> metricsListener;
    private Consumer<DevServerManager.ServerSnapshot> snapshotListener = ignored -> { };

    public ServerConsoleView(DevServerManager serverManager, LauncherSettings settings,
                             Runnable startAction, Runnable stopAction, Runnable terminateAction) {
        this.serverManager = serverManager;
        this.settings = settings;
        getStyleClass().add("server-console");
        setSpacing(10);
        setPadding(new Insets(10));
        console.setEditable(false);
        console.setWrapText(false);
        console.setPromptText("Server console output appears here.");
        VBox.setVgrow(console, Priority.ALWAYS);
        command.setPromptText("Enter a server command");
        command.setOnAction(event -> sendCommand());
        Button send = new Button("Send");
        send.getStyleClass().add("primary-button");
        send.setOnAction(event -> sendCommand());
        resources.setWrapText(true);
        status.setWrapText(true);
        configureAction(start, "start", "Start server", startAction);
        configureAction(stop, "stop", "Stop server", stopAction);
        configureAction(terminate, "terminate", "Terminate server immediately", terminateAction);
        start.setText("Start");
        stop.setText("Stop");
        terminate.setText("Terminate");
        terminate.getStyleClass().add("danger-button");
        HBox controls = new HBox(10, processIndicator, start, stop, terminate);
        controls.setAlignment(javafx.geometry.Pos.CENTER_LEFT);
        HBox input = new HBox(8, command, send);
        HBox.setHgrow(command, Priority.ALWAYS);
        getChildren().addAll(controls, resources, console, input, status);
        refreshIcons();
        updateProcessStatus(null);
    }

    private void configureAction(Button button, String icon, String accessibleText, Runnable action) {
        button.setContentDisplay(javafx.scene.control.ContentDisplay.LEFT);
        button.setGraphicTextGap(8);
        button.setAccessibleText(accessibleText);
        button.setTooltip(new javafx.scene.control.Tooltip(accessibleText));
        button.setOnAction(event -> action.run());
        button.getProperties().put("iconName", icon);
    }

    public void refreshIcons() {
        for (Button button : java.util.List.of(start, stop, terminate)) {
            IconAssets.apply(button, (String) button.getProperties().get("iconName"), settings.darkMode());
        }
    }

    public void showServer(DevServerManager.ServerConfig selected) {
        detach();
        server = selected;
        console.clear();
        if (selected == null) {
            processIndicator.setStatus(StatusIndicator.Status.GREY);
            start.setDisable(true);
            stop.setDisable(true);
            terminate.setDisable(true);
            status.setText("Select a server to view its console.");
            resources.setText("");
            return;
        }
        status.setText(selected.name() + " console");
        consoleListener = event -> Platform.runLater(() -> {
            if (server != null && server.id().equals(event.serverId())) {
                appendLine(event.line());
            }
        });
        metricsListener = snapshot -> Platform.runLater(() -> {
            if (server != null && server.id().equals(snapshot.serverId())) {
                resources.setText(formatMetrics(snapshot));
                updateProcessStatus(snapshot);
                snapshotListener.accept(snapshot);
            }
        });
        serverManager.addConsoleListener(selected.id(), consoleListener);
        serverManager.addMetricsListener(selected.id(), metricsListener);
        DevServerManager.ServerSnapshot currentSnapshot = serverManager.snapshot(selected.id()).orElse(null);
        if (currentSnapshot == null || !currentSnapshot.running()) {
            resources.setText("Server is stopped. Start it to view live resource metrics.");
            updateProcessStatus(currentSnapshot);
        } else {
            updateProcessStatus(currentSnapshot);
        }
    }

    private void updateProcessStatus(DevServerManager.ServerSnapshot snapshot) {
        boolean running = snapshot != null && snapshot.running();
        start.setDisable(server == null || running);
        stop.setDisable(!running);
        terminate.setDisable(!running);
        if (snapshot == null || "STOPPED".equals(snapshot.state())) {
            processIndicator.setStatus(StatusIndicator.Status.GREY);
        } else if ("CRASHED".equals(snapshot.state())) {
            processIndicator.setStatus(StatusIndicator.Status.RED);
        } else if ("RUNNING".equals(snapshot.state())) {
            processIndicator.setStatus(StatusIndicator.Status.GREEN);
        } else {
            processIndicator.setStatus(StatusIndicator.Status.BLUE);
        }
    }

    public void playEntrance() {
        if (MotionConstants.reduceMotion) {
            setOpacity(1);
            setTranslateY(0);
            return;
        }
        setOpacity(0);
        setTranslateY(12);
        FadeTransition fade = new FadeTransition(MotionConstants.NORMAL, this);
        fade.setFromValue(0);
        fade.setToValue(1);
        fade.setInterpolator(MotionConstants.STANDARD);
        TranslateTransition translate = new TranslateTransition(MotionConstants.NORMAL, this);
        translate.setFromY(12);
        translate.setToY(0);
        translate.setInterpolator(MotionConstants.EMPHASIS);
        fade.play();
        translate.play();
    }

    public void setSnapshotListener(Consumer<DevServerManager.ServerSnapshot> listener) {
        snapshotListener = listener == null ? ignored -> { } : listener;
    }

    private void sendCommand() {
        if (server == null) {
            status.setText("Select a server before sending a command.");
            return;
        }
        String text = command.getText();
        try {
            serverManager.sendCommand(server.id(), text);
            command.clear();
        } catch (IOException | IllegalArgumentException exception) {
            status.setText("Could not send command: " + exception.getMessage());
        }
    }

    private void appendLine(String line) {
        console.appendText(line + System.lineSeparator());
        if (console.getLength() > MAX_DISPLAY_CHARACTERS) {
            console.deleteText(0, console.getLength() - MAX_DISPLAY_CHARACTERS);
        }
        console.positionCaret(console.getLength());
    }

    private static String formatMetrics(DevServerManager.ServerSnapshot snapshot) {
        String memory = snapshot.residentMemoryBytes() < 0
                ? "unavailable"
                : String.format(java.util.Locale.ROOT, "%.0f MiB",
                snapshot.residentMemoryBytes() / (1024.0 * 1024.0));
        return String.format(java.util.Locale.ROOT,
                "%s · uptime %s · process memory %s · CPU %.1f%% · CPU time %s%nJVM flags: %s",
                snapshot.state(), formatDuration(snapshot.uptime()), memory, snapshot.cpuPercent(),
                formatDuration(snapshot.totalCpuTime()), String.join(" ", snapshot.javaFlags()));
    }

    private static String formatDuration(java.time.Duration duration) {
        long seconds = duration.toSeconds();
        return "%02d:%02d:%02d".formatted(seconds / 3600, seconds / 60 % 60, seconds % 60);
    }

    private void detach() {
        if (server != null && consoleListener != null) {
            serverManager.removeConsoleListener(server.id(), consoleListener);
        }
        if (server != null && metricsListener != null) {
            serverManager.removeMetricsListener(server.id(), metricsListener);
        }
        consoleListener = null;
        metricsListener = null;
    }
}
