package net.stratalauncher.core.auth;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonParseException;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.List;
import java.util.regex.Pattern;

public final class LauncherSettings {
    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();
    public static final List<String> RECOMMENDED_JAVA_FLAGS = List.of(
            "-Xms6G",
            "-Xmx8G",
            "-XX:+UnlockExperimentalVMOptions",
            "-XX:+UnlockDiagnosticVMOptions",
            "-XX:+DisableExplicitGC",
            "-XX:NmethodSweepActivity=1",
            "-XX:ReservedCodeCacheSize=400M",
            "-XX:NonNMethodCodeHeapSize=12M",
            "-XX:ProfiledCodeHeapSize=194M",
            "-XX:NonProfiledCodeHeapSize=194M",
            "-XX:-DontCompileHugeMethods",
            "-XX:MaxNodeLimit=240000",
            "-XX:NodeLimitFudgeFactor=8000",
            "-XX:+UseVectorCmov",
            "-XX:+PerfDisableSharedMem",
            "-XX:+UseFastUnorderedTimeStamps",
            "-XX:+UseCriticalJavaThreadPriority",
            "-XX:AllocatePrefetchStyle=3",
            "-XX:+UseG1GC",
            "-XX:MaxGCPauseMillis=37",
            "-XX:G1HeapRegionSize=16M",
            "-XX:G1NewSizePercent=23",
            "-XX:G1ReservePercent=20",
            "-XX:SurvivorRatio=32",
            "-XX:G1MixedGCCountTarget=3",
            "-XX:G1HeapWastePercent=20",
            "-XX:InitiatingHeapOccupancyPercent=10",
            "-XX:G1RSetUpdatingPauseTimePercent=0",
            "-XX:MaxTenuringThreshold=1",
            "-XX:G1SATBBufferEnqueueingThresholdPercent=30",
            "-XX:G1ConcMarkStepDurationMillis=5.0",
            "-XX:GCTimeRatio=99");
    private static final Pattern CLIENT_ID = Pattern.compile(
            "(?i)[0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12}");

    private final Path settingsFile;
    private String azureClientId;
    private List<String> globalJavaLaunchFlags;
    private boolean darkMode = true;
    private boolean reduceMotion;
    private RuntimeVendor runtimeVendor = RuntimeVendor.MOJANG;
    private boolean redundantJavaRuntimes;
    private boolean vsyncEnabled = defaultVsyncEnabled();
    private boolean frameTimeOverlayEnabled;

    public LauncherSettings(Path dataDirectory) throws IOException {
        settingsFile = dataDirectory.toAbsolutePath().normalize().resolve("settings.json");
        if (Files.isRegularFile(settingsFile)) {
            try {
                SettingsData data = GSON.fromJson(Files.readString(settingsFile), SettingsData.class);
                azureClientId = data == null || data.azureClientId == null ? "" : data.azureClientId;
                globalJavaLaunchFlags = data == null || data.globalJavaLaunchFlags == null
                        ? RECOMMENDED_JAVA_FLAGS
                        : net.stratalauncher.core.settings.InstanceSettings.validateFlags(data.globalJavaLaunchFlags);
                darkMode = data == null || data.darkMode == null || data.darkMode;
                reduceMotion = data != null && Boolean.TRUE.equals(data.reduceMotion);
                runtimeVendor = data == null || data.runtimeVendor == null
                        ? RuntimeVendor.MOJANG : RuntimeVendor.valueOf(data.runtimeVendor);
                redundantJavaRuntimes = data != null && Boolean.TRUE.equals(data.redundantJavaRuntimes);
                vsyncEnabled = data == null || data.vsyncEnabled == null
                        ? defaultVsyncEnabled() : data.vsyncEnabled;
                frameTimeOverlayEnabled = data != null && Boolean.TRUE.equals(data.frameTimeOverlayEnabled);
                validateClientId(azureClientId);
            } catch (JsonParseException exception) {
                throw new IOException("Launcher settings are invalid: " + settingsFile, exception);
            } catch (IllegalArgumentException exception) {
                throw new IOException("Launcher settings contain invalid values: " + settingsFile, exception);
            }
        } else {
            azureClientId = "";
            globalJavaLaunchFlags = RECOMMENDED_JAVA_FLAGS;
        }
    }

    public synchronized String azureClientId() {
        return azureClientId;
    }

    public synchronized void setAzureClientId(String clientId) throws IOException {
        String normalized = clientId == null ? "" : clientId.trim();
        setPreferences(normalized, globalJavaLaunchFlags);
    }

    public synchronized List<String> globalJavaLaunchFlags() {
        return List.copyOf(globalJavaLaunchFlags);
    }

    public synchronized boolean darkMode() {
        return darkMode;
    }

    public synchronized boolean reduceMotion() {
        return reduceMotion;
    }

    public synchronized RuntimeVendor runtimeVendor() {
        return runtimeVendor;
    }

    public synchronized boolean redundantJavaRuntimes() {
        return redundantJavaRuntimes;
    }

    public synchronized boolean vsyncEnabled() {
        return vsyncEnabled;
    }

    public synchronized boolean frameTimeOverlayEnabled() {
        return frameTimeOverlayEnabled;
    }

    public synchronized void setRuntimePreferences(RuntimeVendor vendor, boolean redundant) throws IOException {
        RuntimeVendor normalizedVendor = java.util.Objects.requireNonNull(vendor);
        persistSettings(azureClientId, globalJavaLaunchFlags, darkMode, reduceMotion,
                normalizedVendor, redundant, vsyncEnabled, frameTimeOverlayEnabled);
        runtimeVendor = normalizedVendor;
        redundantJavaRuntimes = redundant;
    }

    public synchronized void setDiagnosticsPreferences(boolean vsync, boolean frameTimeOverlay) throws IOException {
        persistSettings(azureClientId, globalJavaLaunchFlags, darkMode, reduceMotion, runtimeVendor,
                redundantJavaRuntimes, vsync, frameTimeOverlay);
        vsyncEnabled = vsync;
        frameTimeOverlayEnabled = frameTimeOverlay;
    }

    public synchronized void setGlobalJavaLaunchFlags(List<String> flags) throws IOException {
        setPreferences(azureClientId, flags);
    }

    public synchronized void setPreferences(String clientId, List<String> flags) throws IOException {
        setPreferences(clientId, flags, darkMode, reduceMotion);
    }

    public synchronized void setPreferences(String clientId, List<String> flags,
                                            boolean darkMode, boolean reduceMotion) throws IOException {
        setPreferences(clientId, flags, darkMode, reduceMotion, runtimeVendor, redundantJavaRuntimes);
    }

    public synchronized void setPreferences(String clientId, List<String> flags,
                                            boolean darkMode, boolean reduceMotion, RuntimeVendor vendor,
                                            boolean redundantJavaRuntimes) throws IOException {
        setPreferences(clientId, flags, darkMode, reduceMotion, vendor, redundantJavaRuntimes,
                vsyncEnabled, frameTimeOverlayEnabled);
    }

    public synchronized void setPreferences(String clientId, List<String> flags,
                                            boolean darkMode, boolean reduceMotion, RuntimeVendor vendor,
                                            boolean redundantJavaRuntimes, boolean vsyncEnabled,
                                            boolean frameTimeOverlayEnabled) throws IOException {
        String normalizedClientId = clientId == null ? "" : clientId.trim();
        validateClientId(normalizedClientId);
        List<String> normalizedFlags = net.stratalauncher.core.settings.InstanceSettings.validateFlags(flags);
        RuntimeVendor normalizedVendor = java.util.Objects.requireNonNull(vendor);
        persistSettings(normalizedClientId, normalizedFlags, darkMode, reduceMotion,
                normalizedVendor, redundantJavaRuntimes, vsyncEnabled, frameTimeOverlayEnabled);
        azureClientId = normalizedClientId;
        globalJavaLaunchFlags = normalizedFlags;
        this.darkMode = darkMode;
        this.reduceMotion = reduceMotion;
        runtimeVendor = normalizedVendor;
        this.redundantJavaRuntimes = redundantJavaRuntimes;
        this.vsyncEnabled = vsyncEnabled;
        this.frameTimeOverlayEnabled = frameTimeOverlayEnabled;
    }

    private void persistSettings(String clientId, List<String> flags,
                                 boolean darkMode, boolean reduceMotion, RuntimeVendor vendor,
                                 boolean redundantJavaRuntimes, boolean vsyncEnabled,
                                 boolean frameTimeOverlayEnabled) throws IOException {
        Files.createDirectories(settingsFile.getParent());
        Path temporary = Files.createTempFile(settingsFile.getParent(), "settings-", ".tmp");
        try {
            Files.writeString(temporary, GSON.toJson(new SettingsData(clientId, flags, darkMode, reduceMotion,
                    vendor.name(), redundantJavaRuntimes, vsyncEnabled, frameTimeOverlayEnabled)));
            try {
                Files.move(temporary, settingsFile, StandardCopyOption.ATOMIC_MOVE,
                        StandardCopyOption.REPLACE_EXISTING);
            } catch (java.nio.file.AtomicMoveNotSupportedException exception) {
                Files.move(temporary, settingsFile, StandardCopyOption.REPLACE_EXISTING);
            }
        } finally {
            Files.deleteIfExists(temporary);
        }
    }

    public static boolean defaultVsyncEnabled() {
        return !System.getProperty("os.name", "").toLowerCase(java.util.Locale.ROOT).contains("linux");
    }

    public enum RuntimeVendor {
        MOJANG("Mojang"),
        TEMURIN("Eclipse Temurin"),
        ZULU("Azul Zulu");

        private final String label;

        RuntimeVendor(String label) {
            this.label = label;
        }

        public String label() {
            return label;
        }
    }

    private static void validateClientId(String clientId) {
        if (!clientId.isBlank() && !CLIENT_ID.matcher(clientId).matches()) {
            throw new IllegalArgumentException("Azure client ID must be a GUID or empty.");
        }
    }

    private record SettingsData(String azureClientId, List<String> globalJavaLaunchFlags,
                                Boolean darkMode, Boolean reduceMotion, String runtimeVendor,
                                Boolean redundantJavaRuntimes, Boolean vsyncEnabled,
                                Boolean frameTimeOverlayEnabled) {
    }
}
