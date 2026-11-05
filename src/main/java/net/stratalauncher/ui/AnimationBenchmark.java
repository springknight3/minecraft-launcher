package net.stratalauncher.ui;

import javafx.animation.AnimationTimer;
import javafx.animation.PauseTransition;
import javafx.application.Platform;
import javafx.scene.Scene;
import javafx.stage.Stage;
import javafx.util.Duration;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.function.IntConsumer;

public final class AnimationBenchmark {
    private static final Logger LOGGER = Logger.getLogger(AnimationBenchmark.class.getName());
    private static final int WARMUP_ITERATIONS = 3;
    private static final int MEASURED_ITERATIONS = 15;
    private static final Duration BETWEEN_ITERATIONS = Duration.millis(90);

    private final Stage stage;
    private final Scene scene;
    private final MainLayout layout;
    private final FlyoutManager flyouts;
    private final StartupOverlay startupOverlay;
    private final String label;
    private final Map<String, String> options;
    private final List<Scenario> scenarios;
    private final List<ScenarioResult> results = new ArrayList<>();
    private final AnimationTimer frameSampler = new AnimationTimer() {
        @Override
        public void handle(long now) {
            if (activeSample == null) {
                previousPulse = 0;
                return;
            }
            if (previousPulse != 0) {
                double frameTimeMs = (now - previousPulse) / 1_000_000.0;
                if (frameTimeMs > 0 && frameTimeMs <= 10_000) {
                    activeSample.frameTimes.add(frameTimeMs);
                }
            }
            previousPulse = now;
        }
    };
    private int scenarioIndex;
    private int iterationIndex;
    private long previousPulse;
    private Sample activeSample;

    public AnimationBenchmark(Stage stage, Scene scene, MainLayout layout, FlyoutManager flyouts,
                              StartupOverlay startupOverlay, String label, Map<String, String> options) {
        this.stage = stage;
        this.scene = scene;
        this.layout = layout;
        this.flyouts = flyouts;
        this.startupOverlay = startupOverlay;
        this.label = label == null || label.isBlank() ? defaultLabel() : safeLabel(label);
        this.options = Map.copyOf(options);
        scenarios = createScenarios();
    }

    public void start() {
        frameSampler.start();
        PauseTransition visiblePulse = new PauseTransition(Duration.millis(220));
        visiblePulse.setOnFinished(event -> runScenario());
        visiblePulse.play();
    }

    private List<Scenario> createScenarios() {
        List<Scenario> available = List.of(
                new Scenario("Screen switch", "screen-switch", Duration.millis(700),
                        () -> layout.prepareBenchmarkScenario("screen-switch"),
                        iteration -> layout.benchmarkToggleScreen()),
                new Scenario("Flyout · instance settings", "flyout-instance-settings",
                        Duration.millis(650),
                        () -> layout.prepareBenchmarkScenario("flyout-instance-settings"),
                        iteration -> layout.benchmarkOpenInstanceSettings()),
                new Scenario("Flyout · launcher settings", "flyout-launcher-settings",
                        Duration.millis(650),
                        () -> layout.prepareBenchmarkScenario("flyout-launcher-settings"),
                        iteration -> layout.benchmarkOpenLauncherSettings()),
                new Scenario("Flyout · accounts", "flyout-accounts", Duration.millis(650),
                        () -> layout.prepareBenchmarkScenario("flyout-accounts"),
                        iteration -> layout.benchmarkOpenAccountsFlyout()),
                new Scenario("Tab switch", "tab-switch", Duration.millis(700),
                        () -> layout.prepareBenchmarkScenario("tab-switch"),
                        iteration -> layout.benchmarkSelectSettingsTab((iteration & 1) == 0 ? 1 : 0), true),
                new Scenario("Card hover", "card-hover", Duration.millis(420),
                        () -> layout.prepareBenchmarkScenario("card-hover"),
                        iteration -> layout.benchmarkHoverInstanceCard(true)),
                new Scenario("Toast", "toast", Duration.millis(650),
                        () -> layout.prepareBenchmarkScenario("toast"),
                        iteration -> flyouts.showToast("Benchmark notification")),
                new Scenario("Startup animation", "startup-animation", Duration.millis(850),
                        () -> layout.prepareBenchmarkScenario("startup-animation"),
                        iteration -> {
                            startupOverlay.setOpacity(1);
                            startupOverlay.setVisible(true);
                            startupOverlay.playEntrance();
                        }));
        String only = options.get("only");
        if (only == null || only.isBlank()) {
            return available;
        }
        List<String> selected = List.of(only.split(",")).stream().map(String::trim).toList();
        List<Scenario> filtered = available.stream()
                .filter(scenario -> selected.contains(scenario.id())
                        || selected.contains(scenario.id().replace("flyout-", "flyout")))
                .toList();
        if (filtered.isEmpty()) {
            throw new IllegalArgumentException("No benchmark scenarios match --only=" + only);
        }
        return filtered;
    }

    private void runScenario() {
        if (scenarioIndex >= scenarios.size()) {
            finish();
            return;
        }
        Scenario scenario = scenarios.get(scenarioIndex);
        try {
            flyouts.close();
            flyouts.clearBenchmarkToasts();
            scenario.prepare().run();
            scene.getRoot().applyCss();
            scene.getRoot().layout();
            iterationIndex = 0;
            if (!scenario.keepsFlyoutOpen()) {
                continueIteration(scenario);
            } else {
                PauseTransition settle = new PauseTransition(scenario.duration());
                settle.setOnFinished(event -> continueIteration(scenario));
                settle.play();
            }
        } catch (RuntimeException exception) {
            fail(exception);
        }
    }

    private void continueIteration(Scenario scenario) {
        if (iterationIndex >= WARMUP_ITERATIONS + MEASURED_ITERATIONS) {
            results.add(new ScenarioResult(scenario, List.copyOf(measuredSamples)));
            measuredSamples.clear();
            scenarioIndex++;
            PauseTransition next = new PauseTransition(BETWEEN_ITERATIONS);
            next.setOnFinished(event -> runScenario());
            next.play();
            return;
        }
        if (!scenario.keepsFlyoutOpen()) {
            flyouts.close();
            flyouts.clearBenchmarkToasts();
            if (scenario.id().equals("card-hover")) {
                layout.benchmarkHoverInstanceCard(false);
            }
        }
        PauseTransition settle = new PauseTransition(BETWEEN_ITERATIONS);
        settle.setOnFinished(event -> measureIteration(scenario));
        settle.play();
    }

    private final List<Sample> measuredSamples = new ArrayList<>();

    private void measureIteration(Scenario scenario) {
        Sample sample = new Sample();
        activeSample = sample;
        previousPulse = 0;
        try {
            long triggerStart = System.nanoTime();
            scenario.trigger().accept(iterationIndex);
            long triggerEnd = System.nanoTime();
            long cssStart = System.nanoTime();
            scene.getRoot().applyCss();
            long cssEnd = System.nanoTime();
            scene.getRoot().layout();
            long layoutEnd = System.nanoTime();
            sample.triggerMs = elapsedMs(triggerStart, triggerEnd);
            sample.cssMs = elapsedMs(cssStart, cssEnd);
            sample.layoutMs = elapsedMs(cssEnd, layoutEnd);
            sample.setupMs = sample.triggerMs + sample.cssMs + sample.layoutMs;
            PauseTransition measurementWindow = new PauseTransition(scenario.duration());
            measurementWindow.setOnFinished(event -> {
                activeSample = null;
                if (iterationIndex >= WARMUP_ITERATIONS) {
                    measuredSamples.add(sample);
                }
                iterationIndex++;
                continueIteration(scenario);
            });
            measurementWindow.play();
        } catch (RuntimeException exception) {
            activeSample = null;
            fail(exception);
        }
    }

    private void finish() {
        frameSampler.stop();
        try {
            writeReport();
            LOGGER.info("Animation benchmark report written to build/bench/" + label + ".md");
            stage.close();
            Platform.exit();
        } catch (IOException exception) {
            fail(exception);
        }
    }

    private void fail(Throwable exception) {
        frameSampler.stop();
        LOGGER.log(Level.SEVERE, "Animation benchmark failed.", exception);
        stage.close();
        Platform.exit();
    }

    private void writeReport() throws IOException {
        Path destination = Path.of("build", "bench", label + ".md");
        Files.createDirectories(destination.getParent());
        StringBuilder report = new StringBuilder("# Animation benchmark: ")
                .append(label).append("\n\n")
                .append("- Timestamp: ").append(java.time.Instant.now()).append('\n')
                .append("- XDG_SESSION_TYPE: ").append(System.getenv().getOrDefault("XDG_SESSION_TYPE", "unset"))
                .append('\n')
                .append("- Prism pipeline/order: ")
                .append(System.getProperty("prism.order", "JavaFX default")).append('\n')
                .append("- Effective VSync: ").append(System.getProperty("prism.vsync", "JavaFX default"))
                .append(" (source: ")
                .append(System.getProperty("stratalauncher.vsync.source", "unknown")).append(")\n")
                .append("- JavaFX pulse logger: ")
                .append(System.getProperty("javafx.pulseLogger", "false")).append("\n\n")
                .append("Each scenario used 3 discarded warmups, then 15 measured iterations. ")
                .append("The first row of statistics is measured iteration 1 after warmup; ")
                .append("steady state aggregates measured iterations 2–15. Frame times are pulse intervals ")
                .append("sampled with an AnimationTimer. Setup time includes synchronous trigger work, ")
                .append("applyCss(), and layout() on the FX thread.\n\n")
                .append("| Animation | Run | Frames | Avg ms | p95 ms | Max ms | >16.6 ms | ")
                .append("Trigger ms | CSS ms | Layout ms | Setup ms |\n")
                .append("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
        for (ScenarioResult result : results) {
            appendStats(report, result.scenario().name(), "First run", List.of(result.samples().getFirst()));
            appendStats(report, result.scenario().name(), "Steady state",
                    result.samples().subList(1, result.samples().size()));
        }
        Files.writeString(destination, report);
    }

    private static void appendStats(StringBuilder report, String name, String run, List<Sample> samples) {
        List<Double> frames = samples.stream().flatMap(sample -> sample.frameTimes.stream()).sorted().toList();
        double mean = frames.stream().mapToDouble(Double::doubleValue).average().orElse(0);
        double p95 = percentile(frames, 0.95);
        double max = frames.isEmpty() ? 0 : frames.getLast();
        long overBudget = frames.stream().filter(frame -> frame > 16.6).count();
        double percentage = frames.isEmpty() ? 0 : overBudget * 100.0 / frames.size();
        double triggerMs = average(samples, sample -> sample.triggerMs);
        double cssMs = average(samples, sample -> sample.cssMs);
        double layoutMs = average(samples, sample -> sample.layoutMs);
        double setupMs = average(samples, sample -> sample.setupMs);
        report.append("| ").append(name).append(" | ").append(run).append(" | ")
                .append(frames.size()).append(" | ").append(format(mean)).append(" | ")
                .append(format(p95)).append(" | ").append(format(max)).append(" | ")
                .append(format(percentage)).append("% | ").append(format(triggerMs)).append(" | ")
                .append(format(cssMs)).append(" | ").append(format(layoutMs)).append(" | ")
                .append(format(setupMs)).append(" |\n");
    }

    private static double average(List<Sample> samples, java.util.function.ToDoubleFunction<Sample> getter) {
        return samples.stream().mapToDouble(getter).average().orElse(0);
    }

    private static double percentile(List<Double> sorted, double percentile) {
        if (sorted.isEmpty()) {
            return 0;
        }
        int index = Math.min(sorted.size() - 1, (int) Math.ceil(sorted.size() * percentile) - 1);
        return sorted.get(Math.max(index, 0));
    }

    private static double elapsedMs(long start, long end) {
        return (end - start) / 1_000_000.0;
    }

    private static String format(double value) {
        return String.format(Locale.ROOT, "%.2f", value);
    }

    private static String defaultLabel() {
        return Boolean.parseBoolean(System.getProperty("prism.vsync", "false"))
                ? "vsync-on" : "vsync-off";
    }

    private static String safeLabel(String value) {
        String normalized = value.toLowerCase(Locale.ROOT).replaceAll("[^a-z0-9._-]+", "-");
        if (normalized.isBlank()) {
            throw new IllegalArgumentException("Benchmark label must contain letters or numbers.");
        }
        return normalized;
    }

    private record Scenario(String name, String id, Duration duration, Runnable prepare,
                            IntConsumer trigger, boolean keepsFlyoutOpen) {
        private Scenario(String name, String id, Duration duration, Runnable prepare, IntConsumer trigger) {
            this(name, id, duration, prepare, trigger, false);
        }

    }

    private record ScenarioResult(Scenario scenario, List<Sample> samples) {
    }

    private static final class Sample {
        private final List<Double> frameTimes = new ArrayList<>();
        private double triggerMs;
        private double cssMs;
        private double layoutMs;
        private double setupMs;

        private Sample() {
        }
    }
}
