package net.stratalauncher.core.auth;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Path;
import java.util.List;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

class LauncherSettingsTest {
    @TempDir
    Path temporaryDirectory;

    @Test
    void persistsTheAzureClientId() throws IOException {
        LauncherSettings settings = new LauncherSettings(temporaryDirectory);
        settings.setAzureClientId("01234567-89ab-cdef-0123-456789abcdef");

        assertEquals("01234567-89ab-cdef-0123-456789abcdef",
                new LauncherSettings(temporaryDirectory).azureClientId());
    }

    @Test
    void rejectsNonGuidClientIds() throws IOException {
        LauncherSettings settings = new LauncherSettings(temporaryDirectory);

        assertThrows(IllegalArgumentException.class, () -> settings.setAzureClientId("client-id"));
    }

    @Test
    void persistsGlobalJavaFlagsWithoutLosingAuthenticationSettings() throws IOException {
        LauncherSettings settings = new LauncherSettings(temporaryDirectory);
        settings.setPreferences("01234567-89ab-cdef-0123-456789abcdef", List.of("-Xmx4G", "-XX:+UseG1GC"));

        LauncherSettings reloaded = new LauncherSettings(temporaryDirectory);
        assertEquals("01234567-89ab-cdef-0123-456789abcdef", reloaded.azureClientId());
        assertEquals(List.of("-Xmx4G", "-XX:+UseG1GC"), reloaded.globalJavaLaunchFlags());
    }

    @Test
    void persistsThemeAndMotionPreferencesAlongsideExistingSettings() throws IOException {
        LauncherSettings settings = new LauncherSettings(temporaryDirectory);
        settings.setPreferences("01234567-89ab-cdef-0123-456789abcdef",
                List.of("-Xmx4G"), false, true);

        LauncherSettings reloaded = new LauncherSettings(temporaryDirectory);

        assertEquals(false, reloaded.darkMode());
        assertEquals(true, reloaded.reduceMotion());
        assertEquals("01234567-89ab-cdef-0123-456789abcdef", reloaded.azureClientId());
        assertEquals(List.of("-Xmx4G"), reloaded.globalJavaLaunchFlags());
    }

    @Test
    void persistsRuntimeProviderAndSharingPreferencesWithExistingSettings() throws IOException {
        LauncherSettings settings = new LauncherSettings(temporaryDirectory);
        settings.setPreferences("01234567-89ab-cdef-0123-456789abcdef", List.of("-Xmx4G"),
                false, true, LauncherSettings.RuntimeVendor.TEMURIN, true);

        LauncherSettings reloaded = new LauncherSettings(temporaryDirectory);

        assertEquals(LauncherSettings.RuntimeVendor.TEMURIN, reloaded.runtimeVendor());
        assertEquals(true, reloaded.redundantJavaRuntimes());
        assertEquals(false, reloaded.darkMode());
        assertEquals(true, reloaded.reduceMotion());
        assertEquals("01234567-89ab-cdef-0123-456789abcdef", reloaded.azureClientId());
    }

    @Test
    void defaultsVsyncByPlatformAndKeepsTheDebugOverlayOff() throws IOException {
        LauncherSettings settings = new LauncherSettings(temporaryDirectory);

        assertEquals(LauncherSettings.defaultVsyncEnabled(), settings.vsyncEnabled());
        assertEquals(false, settings.frameTimeOverlayEnabled());
    }

    @Test
    void persistsVsyncAndDebugOverlayPreferences() throws IOException {
        LauncherSettings settings = new LauncherSettings(temporaryDirectory);
        settings.setPreferences("", List.of("-Xmx4G"), true, false,
                LauncherSettings.RuntimeVendor.MOJANG, false, false, true);

        LauncherSettings reloaded = new LauncherSettings(temporaryDirectory);

        assertEquals(false, reloaded.vsyncEnabled());
        assertEquals(true, reloaded.frameTimeOverlayEnabled());
    }

    @Test
    void suppliesRecommendedFlagsForExistingSettingsWithoutAFlagProfile() throws IOException {
        Path settingsFile = temporaryDirectory.resolve("settings.json");
        java.nio.file.Files.writeString(settingsFile, "{\"azureClientId\":\"\"}");

        assertEquals(List.of(
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
                        "-XX:GCTimeRatio=99"),
                new LauncherSettings(temporaryDirectory).globalJavaLaunchFlags());
    }
}
