package net.stratalauncher.core.loader;

import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.instance.Instance;
import net.stratalauncher.core.instance.InstanceManager;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Instant;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

class LoaderInstallerTest {
    @TempDir
    Path temporaryDirectory;

    @Test
    void loadsInstalledLoaderSelectionAndProfileFromInstanceDirectory() throws IOException {
        InstanceManager instanceManager = new InstanceManager(temporaryDirectory.resolve("launcher"));
        Instance instance = instanceManager.create("Modded", "1.21.1");
        Path stateFile = instance.gameDirectory().resolve(".stratalauncher-loader.json");
        Files.writeString(stateFile, """
                {"type":"FABRIC","minecraftVersion":"1.21.1","loaderVersion":"0.16.14",
                 "profileVersionId":"fabric-loader-0.16.14-1.21.1","installed":true}
                """);
        Files.writeString(instance.gameDirectory().resolve("loader-profile.json"), """
                {"id":"fabric-loader-0.16.14-1.21.1","inheritsFrom":"1.21.1",
                 "mainClass":"net.fabricmc.loader.impl.launch.knot.KnotClient","libraries":[]}
                """);
        try (DownloadManager downloads = new DownloadManager()) {
            LoaderInstaller installer = new LoaderInstaller(instanceManager.dataDirectory(), downloads);

            assertEquals(new LoaderSelection(LoaderType.FABRIC, "0.16.14"),
                    installer.installedSelection(instance));
            assertEquals("fabric-loader-0.16.14-1.21.1",
                    installer.installedProfile(instance).profileVersionId());
        }
    }

    @Test
    void refusesToLaunchLoaderWithPendingInstallationState() throws IOException {
        Instance instance = new Instance("00000000-0000-0000-0000-000000000001", "Modded",
                "1.21.1", Instant.now(), temporaryDirectory.resolve("instance"));
        Files.createDirectories(instance.gameDirectory());
        Files.writeString(instance.gameDirectory().resolve(".stratalauncher-loader.json"), """
                {"type":"FABRIC","minecraftVersion":"1.21.1","loaderVersion":"0.16.14",
                 "profileVersionId":"","installed":false}
                """);
        try (DownloadManager downloads = new DownloadManager()) {
            LoaderInstaller installer = new LoaderInstaller(temporaryDirectory, downloads);

            assertThrows(IOException.class, () -> installer.installedProfile(instance));
        }
    }
}
