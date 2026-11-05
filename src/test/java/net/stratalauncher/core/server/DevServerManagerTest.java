package net.stratalauncher.core.server;

import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.mojang.MojangManifestClient;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNull;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

class DevServerManagerTest {
    @TempDir
    Path temporaryDirectory;

    @Test
    void createsIsolatedServerAndPersistsEulaAndJvmSettings() throws IOException {
        try (DownloadManager downloads = new DownloadManager();
             DevServerManager manager = new DevServerManager(temporaryDirectory, downloads,
                     new MojangManifestClient())) {
            DevServerManager.ServerConfig created = manager.create("Local test", "1.21.1",
                    ServerDistribution.FABRIC);
            Path serverDirectory = created.serverDirectory(temporaryDirectory.resolve("servers")
                    .resolve(created.id()));

            assertTrue(Files.isDirectory(serverDirectory));
            assertFalse(created.installed());
            assertFalse(created.eulaAccepted());
            assertTrue(Files.readString(serverDirectory.resolve("eula.txt")).contains("eula=false"));

            DevServerManager.ServerConfig updated = manager.updateSettings(created, "  Fabric dev  ", true,
                    List.of("-Xmx4G", "-XX:+UseG1GC"));
            DevServerManager.ServerConfig reloaded = manager.listServers().getFirst();

            assertEquals("Fabric dev", updated.name());
            assertEquals("Fabric dev", reloaded.name());
            assertTrue(reloaded.eulaAccepted());
            assertEquals(List.of("-Xmx4G", "-XX:+UseG1GC"), reloaded.javaFlags());
            assertEquals("eula=true\n", Files.readString(serverDirectory.resolve("eula.txt")));
        }
    }

    @Test
    void requiresEulaAndAnInstallationBeforeStart() throws IOException {
        try (DownloadManager downloads = new DownloadManager();
             DevServerManager manager = new DevServerManager(temporaryDirectory, downloads,
                     new MojangManifestClient())) {
            DevServerManager.ServerConfig server = manager.create("Vanilla", "1.21.1", ServerDistribution.VANILLA);
            DevServerManager.ServerConfig installed = new DevServerManager.ServerConfig(server.id(), server.name(),
                    server.minecraftVersion(), server.distribution(), true, false, List.of(), server.createdAt());
            Path configFile = temporaryDirectory.resolve("servers").resolve(server.id()).resolve("server.json");
            Files.writeString(configFile, new com.google.gson.Gson().toJson(installed));
            IOException eulaError = assertThrows(IOException.class, () -> manager.start(server));
            assertTrue(eulaError.getMessage().contains("EULA"));

            DevServerManager.ServerConfig accepted = manager.updateSettings(installed, server.name(), true, List.of());
            IOException jarError = assertThrows(IOException.class, () -> manager.start(accepted));
            assertTrue(jarError.getMessage().contains("JAR is missing"));
            assertThrows(IOException.class, () -> manager.sendCommand(server.id(), "say hello"));
        }
    }

    @Test
    void rejectsUnsafeNamesAndMultiLineJvmFlags() throws IOException {
        try (DownloadManager downloads = new DownloadManager();
             DevServerManager manager = new DevServerManager(temporaryDirectory, downloads,
                     new MojangManifestClient())) {
            assertThrows(IllegalArgumentException.class,
                    () -> manager.create("../escape", "1.21.1", ServerDistribution.VANILLA));
            DevServerManager.ServerConfig server = manager.create("Valid", "1.21.1", ServerDistribution.PAPER);
            assertThrows(IllegalArgumentException.class,
                    () -> manager.updateSettings(server, "Valid", false, List.of("not-a-jvm-flag")));
        }
    }

    @Test
    void persistsServerOptionsAndPreservesOtherProperties() throws IOException {
        try (DownloadManager downloads = new DownloadManager();
             DevServerManager manager = new DevServerManager(temporaryDirectory, downloads,
                     new MojangManifestClient())) {
            DevServerManager.ServerConfig created = manager.create("Options", "1.21.1",
                    ServerDistribution.VANILLA);
            Path serverProperties = manager.serverDirectoryPath(created).resolve("server.properties");

            assertEquals(10, created.viewDistance());
            assertTrue(created.onlineMode());
            Files.writeString(serverProperties, Files.readString(serverProperties) + "motd=My server\n");

            DevServerManager.ServerConfig updated = manager.updateSettings(created, created.name(), false,
                    List.of(), 16, false);
            DevServerManager.ServerConfig reloaded = manager.listServers().getFirst();
            String properties = Files.readString(serverProperties);

            assertEquals(16, reloaded.viewDistance());
            assertFalse(reloaded.onlineMode());
            assertTrue(properties.contains("view-distance=16"));
            assertTrue(properties.contains("online-mode=false"));
            assertTrue(properties.contains("motd=My server"));
            assertThrows(IllegalArgumentException.class, () -> manager.updateSettings(updated, updated.name(),
                    false, List.of(), 33, false));
        }
    }

    @Test
    void savesServerPropertiesTextAndSynchronizesManagedSettings() throws IOException {
        try (DownloadManager downloads = new DownloadManager();
             DevServerManager manager = new DevServerManager(temporaryDirectory, downloads,
                     new MojangManifestClient())) {
            DevServerManager.ServerConfig created = manager.create("Properties", "1.21.1",
                    ServerDistribution.VANILLA);
            String edited = "# server settings\nview-distance=14\nonline-mode=false\nmotd=New server\n";

            DevServerManager.ServerConfig saved = manager.saveServerProperties(created, edited);

            assertEquals(edited, manager.readServerProperties(saved));
            assertEquals(14, saved.viewDistance());
            assertFalse(saved.onlineMode());
            assertEquals(saved, manager.listServers().getFirst());
            assertThrows(IllegalArgumentException.class, () -> manager.saveServerProperties(saved,
                    "view-distance=99\nonline-mode=true\n"));
            assertThrows(IllegalArgumentException.class, () -> manager.saveServerProperties(saved,
                    "view-distance=10\nview-distance=12\n"));
        }
    }

    @Test
    void loadsOlderServerConfigWithDefaultsForNewSettings() throws IOException {
        try (DownloadManager downloads = new DownloadManager();
             DevServerManager manager = new DevServerManager(temporaryDirectory, downloads,
                     new MojangManifestClient())) {
            DevServerManager.ServerConfig created = manager.create("Legacy", "1.21.1", ServerDistribution.PAPER);
            Path configFile = temporaryDirectory.resolve("servers").resolve(created.id()).resolve("server.json");
            Files.writeString(configFile, """
                    {
                      "id": "%s",
                      "name": "Legacy",
                      "minecraftVersion": "1.21.1",
                      "distribution": "PAPER",
                      "installed": true,
                      "eulaAccepted": true,
                      "javaFlags": [],
                      "createdAt": "2026-10-31T22:18:12.246808146Z"
                    }
                    """.formatted(created.id()));

            DevServerManager.ServerConfig loaded = manager.listServers().getFirst();

            assertEquals(10, loaded.viewDistance());
            assertTrue(loaded.onlineMode());
            assertFalse(loaded.useManagedJavaRuntime());
            assertNull(loaded.javaRuntimeComponent());
        }
    }

    @Test
    void deletesServerDirectoryAndRejectsLaterOperations() throws IOException {
        try (DownloadManager downloads = new DownloadManager();
             DevServerManager manager = new DevServerManager(temporaryDirectory, downloads,
                     new MojangManifestClient())) {
            DevServerManager.ServerConfig server = manager.create("Delete me", "1.21.1",
                    ServerDistribution.VANILLA);
            Path serverRoot = temporaryDirectory.resolve("servers").resolve(server.id());

            assertDoesNotThrow(() -> manager.delete(server));

            assertFalse(Files.exists(serverRoot));
            assertTrue(manager.listServers().isEmpty());
            assertThrows(IOException.class, () -> manager.delete(server));
        }
    }

    @Test
    void selectsNewestStablePaperBuildAndRejectsUnsafeDownloads() throws IOException {
        var builds = com.google.gson.JsonParser.parseString("""
                [
                  {"id":12,"channel":"EXPERIMENTAL","downloads":{"server:default":{
                    "name":"paper-test.jar","url":"https://fill-data.papermc.io/test",
                    "checksums":{"sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}}},
                  {"id":8,"channel":"STABLE","downloads":{"server:default":{
                    "name":"paper-test-8.jar","url":"https://fill-data.papermc.io/build-8",
                    "checksums":{"sha256":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"}}}},
                  {"id":10,"channel":"STABLE","downloads":{"server:default":{
                    "name":"paper-test-10.jar","url":"https://fill-data.papermc.io/build-10",
                    "checksums":{"sha256":"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"}}}}
                ]
                """).getAsJsonArray();

        DevServerManager.PaperBuild selected = DevServerManager.latestStablePaperBuild(builds);

        assertEquals(10, selected.id());
        assertEquals("paper-test-10.jar", selected.filename());
        assertEquals("https://fill-data.papermc.io/build-10", selected.url().toString());
        assertEquals("c".repeat(64), selected.sha256());
        assertThrows(IOException.class, () -> DevServerManager.latestStablePaperBuild(
                com.google.gson.JsonParser.parseString("""
                        [{"id":1,"channel":"STABLE","downloads":{"server:default":{
                          "name":"paper.jar","url":"https://attacker.example/paper.jar",
                          "checksums":{"sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}}}]
                        """).getAsJsonArray()));
    }
}
