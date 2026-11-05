package net.stratalauncher.core.launch;

import net.stratalauncher.core.instance.Instance;
import net.stratalauncher.core.instance.InstanceManager;
import net.stratalauncher.core.auth.LauncherSettings;
import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.mojang.MojangManifestClient;
import net.stratalauncher.core.settings.InstanceSettings;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Path;
import java.util.List;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

class GameLauncherTest {
    @TempDir
    Path temporaryDirectory;

    @Test
    void rejectsInvalidOfflineNamesBeforeNetworkRequests() throws IOException {
        InstanceManager instanceManager = new InstanceManager(temporaryDirectory);
        Instance instance = instanceManager.create("Test", "1.21.1");
        try (DownloadManager downloads = new DownloadManager()) {
            GameLauncher launcher = new GameLauncher(instanceManager, new MojangManifestClient(), downloads);
            assertThrows(IllegalArgumentException.class,
                    () -> launcher.launchOffline(instance, "not a name", null));
        }
    }

    @Test
    void resolvesInstanceJavaFlagsOverGlobalDefaults() throws IOException {
        InstanceManager instanceManager = new InstanceManager(temporaryDirectory);
        Instance instance = instanceManager.create("Test", "1.21.1");
        LauncherSettings settings = new LauncherSettings(temporaryDirectory);
        settings.setGlobalJavaLaunchFlags(List.of("-Xmx3G"));
        InstanceSettings instanceSettings = new InstanceSettings(instance);
        instanceSettings.setJavaLaunchFlagsOverride(List.of("-Xmx5G"));

        try (DownloadManager downloads = new DownloadManager()) {
            GameLauncher launcher = new GameLauncher(instanceManager, new MojangManifestClient(),
                    downloads, null, settings);

            assertEquals(List.of("-Xmx5G"), launcher.effectiveJavaLaunchFlags(instance));
        }
    }

    @Test
    void replacesMetadataGarbageCollectorDefaultsWhenCustomCollectorIsSelected() {
        List<String> metadata = List.of("-XX:+UseZGC", "-XX:+UseStringDeduplication", "-Dloader.setting=true");

        assertEquals(List.of("-XX:+UseStringDeduplication", "-Dloader.setting=true", "-XX:+UseG1GC"),
                GameLauncher.composeJvmArguments(metadata, List.of("-XX:+UseG1GC")));
    }

    @Test
    void rejectsConflictingGarbageCollectorsInCustomFlags() {
        assertThrows(IllegalArgumentException.class,
                () -> InstanceSettings.validateFlags(List.of("-XX:+UseZGC", "-XX:+UseG1GC")));
    }
}
