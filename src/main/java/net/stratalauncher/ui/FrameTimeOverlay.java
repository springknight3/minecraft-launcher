package net.stratalauncher.ui;

import javafx.animation.AnimationTimer;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.Scene;
import javafx.scene.control.Label;
import javafx.scene.input.KeyCode;
import javafx.scene.input.KeyEvent;
import javafx.scene.layout.VBox;
import javafx.util.Duration;
import net.stratalauncher.core.auth.LauncherSettings;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Instant;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

public final class FrameTimeOverlay extends VBox {
    private static FrameTimeOverlay activeOverlay;

    private final LauncherSettings settings;
    private final boolean activeVsync;
    private final Label mode = new Label();
    private final VBox measurements = new VBox(3);
    private final Map<String, List<List<Double>>> completedMeasurements = new LinkedHashMap<>();
    private final List<ActivityWindow> activeMeasurements = new ArrayList<>();
    private final AnimationTimer timer = new AnimationTimer() {
        @Override
        public void handle(long now) {
            if (previousPulse != 0) {
                double frameTime = (now - previousPulse) / 1_000_000.0;
                if (frameTime > 0 && frameTime <= 10_000) {
                    for (ActivityWindow sample : activeMeasurements) {
                        sample.frameTimes().add(frameTime);
                    }
                }
            }
            previousPulse = now;
            activeMeasurements.removeIf(sample -> {
                if (now < sample.endNanos()) {
                    return false;
                }
                completedMeasurements.computeIfAbsent(sample.activity(), ignored -> new ArrayList<>())
                        .add(List.copyOf(sample.frameTimes()));
                return true;
            });
            if (now - lastRefresh > 250_000_000L) {
                refreshDisplay();
                lastRefresh = now;
            }
        }
    };
    private long previousPulse;
    private long lastRefresh;

    public FrameTimeOverlay(LauncherSettings settings) {
        this(settings, settings.frameTimeOverlayEnabled());
    }

    public FrameTimeOverlay(LauncherSettings settings, boolean enabled) {
        this.settings = settings;
        activeVsync = Boolean.parseBoolean(System.getProperty("prism.vsync",
                Boolean.toString(settings.vsyncEnabled())));
        if (activeOverlay != null) {
            activeOverlay.stop();
        }
        activeOverlay = this;
        getStyleClass().add("frame-time-overlay");
        setAlignment(Pos.TOP_LEFT);
        setPadding(new Insets(10));
        setSpacing(4);
        setMaxWidth(420);
        setMouseTransparent(true);
        Label title = new Label("Frame time · debug");
        title.getStyleClass().add("frame-time-title");
        getChildren().addAll(title, mode, measurements,
                new Label("Ctrl+F9 clears samples"));
        setManaged(false);
        setEnabled(enabled);
    }

    public void attach(Scene scene) {
        scene.addEventFilter(KeyEvent.KEY_PRESSED, event -> {
            if (isVisible() && event.isControlDown() && event.getCode() == KeyCode.F9) {
                clearMeasurements();
                event.consume();
            }
        });
    }

    public void setEnabled(boolean enabled) {
        setVisible(enabled);
        setManaged(enabled);
        if (enabled) {
            previousPulse = 0;
            lastRefresh = 0;
            mode.setText("VSync: " + (activeVsync ? "on" : "off"));
            timer.start();
        } else {
            timer.stop();
            activeMeasurements.clear();
        }
    }

    public void stop() {
        timer.stop();
        if (activeOverlay == this) {
            activeOverlay = null;
        }
    }

    public void writeReport(Path dataDirectory) throws IOException {
        Files.createDirectories(dataDirectory);
        Path destination = dataDirectory.resolve("frame-times-" + (activeVsync ? "vsync-on" : "vsync-off")
                + "-" + Instant.now().toEpochMilli() + ".csv");
        StringBuilder report = new StringBuilder(
                "vsync,activity,runs,frames,mean_ms,p50_ms,p95_ms,max_ms\n");
        String vsync = activeVsync ? "on" : "off";
        for (Map.Entry<String, List<List<Double>>> entry : completedMeasurements.entrySet()) {
            List<Double> frames = entry.getValue().stream().flatMap(List::stream).sorted().toList();
            if (frames.isEmpty()) {
                continue;
            }
            report.append(vsync).append(',')
                    .append(csv(entry.getKey())).append(',')
                    .append(entry.getValue().size()).append(',')
                    .append(frames.size()).append(',')
                    .append(format(mean(frames))).append(',')
                    .append(format(percentile(frames, 0.50))).append(',')
                    .append(format(percentile(frames, 0.95))).append(',')
                    .append(format(frames.getLast())).append('\n');
        }
        Path temporary = Files.createTempFile(dataDirectory, "frame-times-", ".tmp");
        try {
            Files.writeString(temporary, report);
            Files.move(temporary, destination, java.nio.file.StandardCopyOption.REPLACE_EXISTING);
        } finally {
            Files.deleteIfExists(temporary);
        }
    }

    public static void measure(String activity, Duration duration) {
        FrameTimeOverlay overlay = activeOverlay;
        if (overlay == null || !overlay.isVisible() || duration.lessThanOrEqualTo(Duration.ZERO)) {
            return;
        }
        long start = System.nanoTime();
        overlay.activeMeasurements.add(new ActivityWindow(activity,
                start + (long) (duration.toMillis() * 1_000_000), new ArrayList<>()));
    }

    private void clearMeasurements() {
        activeMeasurements.clear();
        completedMeasurements.clear();
        refreshDisplay();
    }

    private void refreshDisplay() {
        mode.setText("VSync: " + (activeVsync ? "on" : "off"));
        measurements.getChildren().clear();
        if (completedMeasurements.isEmpty()) {
            measurements.getChildren().add(new Label("Interact to collect samples."));
            return;
        }
        for (Map.Entry<String, List<List<Double>>> entry : completedMeasurements.entrySet()) {
            List<Double> frames = entry.getValue().stream().flatMap(List::stream).sorted().toList();
            if (frames.isEmpty()) {
                continue;
            }
            measurements.getChildren().add(new Label(String.format(Locale.ROOT,
                    "%s · n=%d · mean %.2f / p95 %.2f / max %.2f ms",
                    entry.getKey(), frames.size(), mean(frames), percentile(frames, 0.95),
                    frames.getLast())));
        }
    }

    private static double mean(List<Double> values) {
        return values.stream().mapToDouble(Double::doubleValue).average().orElse(0);
    }

    private static double percentile(List<Double> sorted, double percentile) {
        int index = Math.min(sorted.size() - 1, (int) Math.ceil(sorted.size() * percentile) - 1);
        return sorted.get(Math.max(0, index));
    }

    private static String format(double value) {
        return String.format(Locale.ROOT, "%.2f", value);
    }

    private static String csv(String value) {
        return "\"" + value.replace("\"", "\"\"") + "\"";
    }

    private record ActivityWindow(String activity, long endNanos, List<Double> frameTimes) {
    }
}
