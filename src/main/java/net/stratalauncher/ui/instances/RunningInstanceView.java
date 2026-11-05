package net.stratalauncher.ui.instances;

import javafx.application.Platform;
import javafx.geometry.Insets;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.Tab;
import javafx.scene.control.TabPane;
import javafx.scene.control.TextArea;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.Priority;
import javafx.scene.layout.VBox;
import net.stratalauncher.core.instance.Instance;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

public final class RunningInstanceView extends BorderPane implements AutoCloseable {
    private static final long MAX_LOG_BYTES = 2L * 1024 * 1024;
    private final Path logFile;
    private final TextArea console = new TextArea();
    private final TextArea logs = new TextArea();
    private final ScheduledExecutorService reader = Executors.newSingleThreadScheduledExecutor(task -> {
        Thread thread = new Thread(task, "stratalauncher-game-console");
        thread.setDaemon(true);
        return thread;
    });
    private long consoleOffset;
    private volatile boolean closed;

    public RunningInstanceView(Instance instance, long consoleOffset, Runnable onBack) {
        this.logFile = instance.gameDirectory().resolve("launcher.log");
        this.consoleOffset = consoleOffset;
        setPadding(new Insets(14));
        getStyleClass().addAll("instances-view", "running-instance-view");

        Button back = new Button("‹ Instances");
        back.getStyleClass().add("action-button");
        back.setOnAction(event -> onBack.run());
        Label title = new Label(instance.name());
        title.getStyleClass().add("hero-title");
        VBox header = new VBox(10, back, title);
        header.setPadding(new Insets(4, 4, 12, 4));
        setTop(header);

        configureOutput(console);
        configureOutput(logs);
        Tab consoleTab = new Tab("Console", console);
        consoleTab.setClosable(false);
        Tab logsTab = new Tab("Logs", logs);
        logsTab.setClosable(false);
        TabPane tabs = new TabPane(consoleTab, logsTab);
        tabs.setTabClosingPolicy(TabPane.TabClosingPolicy.UNAVAILABLE);
        setCenter(tabs);
        BorderPane.setMargin(tabs, new Insets(0, 4, 4, 4));
        reader.scheduleWithFixedDelay(this::readOutput, 0, 500, TimeUnit.MILLISECONDS);
    }

    private static void configureOutput(TextArea output) {
        output.setEditable(false);
        output.setWrapText(false);
        output.getStyleClass().addAll("server-console", "running-instance-output");
        output.setMaxHeight(Double.MAX_VALUE);
        VBox.setVgrow(output, Priority.ALWAYS);
    }

    private void readOutput() {
        if (closed) {
            return;
        }
        try {
            if (!Files.isRegularFile(logFile)) {
                return;
            }
            long length = Files.size(logFile);
            if (length < consoleOffset) {
                consoleOffset = 0;
            }
            byte[] recent;
            try (RandomAccessFile file = new RandomAccessFile(logFile.toFile(), "r")) {
                file.seek(consoleOffset);
                recent = new byte[(int) Math.min(length - consoleOffset, MAX_LOG_BYTES)];
                file.readFully(recent);
                consoleOffset = file.getFilePointer();
            }
            String logText = readLogTail();
            String newOutput = new String(recent, StandardCharsets.UTF_8);
            Platform.runLater(() -> {
                if (closed) {
                    return;
                }
                if (!newOutput.isEmpty()) {
                    console.appendText(newOutput);
                }
                if (!logs.getText().equals(logText)) {
                    logs.setText(logText);
                    logs.positionCaret(logs.getLength());
                }
                console.positionCaret(console.getLength());
            });
        } catch (IOException exception) {
            String message = "Could not read Minecraft output: " + exception.getMessage();
            Platform.runLater(() -> {
                if (!closed) {
                    console.appendText(message + System.lineSeparator());
                }
            });
        }
    }

    private String readLogTail() throws IOException {
        long length = Files.size(logFile);
        long start = Math.max(0, length - MAX_LOG_BYTES);
        byte[] content = new byte[(int) (length - start)];
        try (RandomAccessFile file = new RandomAccessFile(logFile.toFile(), "r")) {
            file.seek(start);
            file.readFully(content);
        }
        return new String(content, StandardCharsets.UTF_8);
    }

    @Override
    public void close() {
        closed = true;
        reader.shutdownNow();
    }
}
