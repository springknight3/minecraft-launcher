package net.stratalauncher.core.modrinth;

import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.instance.Instance;
import net.stratalauncher.core.instance.InstanceManager;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

class ContentManagerTest {
    @TempDir
    Path temporaryDirectory;

    @Test
    void mapsEachContentCategoryToItsMinecraftDirectory() {
        Path game = Path.of("instance", "game");

        assertEquals(game.resolve("mods"), ContentType.MOD.directory(game));
        assertEquals(game.resolve("resourcepacks"), ContentType.RESOURCE_PACK.directory(game));
        assertEquals(game.resolve("shaderpacks"), ContentType.SHADER.directory(game));
        assertEquals(game.resolve("saves").resolve("My World").resolve("datapacks"),
                ContentType.DATA_PACK.directory(game, "My World"));
        assertThrows(IllegalArgumentException.class, () -> ContentType.DATA_PACK.directory(game, ""));
    }

    @Test
    void validatesModrinthDownloadsAndRejectsUnsafeUrlsAndFilenames() throws IOException {
        String hash = "0123456789abcdef0123456789abcdef01234567";

        ContentManager.validateFile(new ModrinthClient.VersionFile(
                "example.jar", "https://cdn.modrinth.com/data/a/example.jar", hash, true, 12));
        assertThrows(IOException.class, () -> ContentManager.validateFile(new ModrinthClient.VersionFile(
                "../escape.jar", "https://cdn.modrinth.com/data/a/escape.jar", hash, true, 12)));
        assertThrows(IOException.class, () -> ContentManager.validateFile(new ModrinthClient.VersionFile(
                "example.jar", "https://evil.example/data/a/example.jar", hash, true, 12)));
        assertThrows(IOException.class, () -> ContentManager.validateFile(new ModrinthClient.VersionFile(
                "example.jar", "https://cdn.modrinth.com/data/a/example.jar", "", true, 12)));
    }

    @Test
    void removesOnlyTrackedFilesInsideTheInstance() throws IOException {
        Instance instance = new InstanceManager(temporaryDirectory.resolve("launcher"))
                .create("Test", "1.21.1");
        Path mods = instance.gameDirectory().resolve("mods");
        Files.createDirectories(mods);
        Path file = mods.resolve("example.jar");
        Files.writeString(file, "mod");
        Files.writeString(instance.gameDirectory().resolve(".stratalauncher-content.json"), """
                {"entries":[{"type":"MOD","projectId":"abc123","slug":"example-mod","title":"Example Mod",
                "versionId":"version-id","versionNumber":"1.0.0","worldName":"","filename":"example.jar",
                "installedAt":"2026-10-31T12:00:00Z"}]}
                """);
        try (DownloadManager downloads = new DownloadManager()) {
            ContentManager manager = new ContentManager(
                    new ModrinthClient("StrataLauncher", "0.1.0"), downloads);

            var installed = manager.installed(instance, ContentType.MOD).getFirst();
            manager.remove(instance, installed);

            assertFalse(Files.exists(file));
            assertTrue(manager.installed(instance, ContentType.MOD).isEmpty());
        }
    }

    @Test
    void listsOnlyWorldDirectoriesAsDataPackTargets() throws IOException {
        Instance instance = new InstanceManager(temporaryDirectory.resolve("launcher"))
                .create("Test", "1.21.1");
        Files.createDirectories(instance.gameDirectory().resolve("saves/World One"));
        Files.writeString(instance.gameDirectory().resolve("saves/not-a-world.txt"), "not a world");
        try (DownloadManager downloads = new DownloadManager()) {
            ContentManager manager = new ContentManager(
                    new ModrinthClient("StrataLauncher", "0.1.0"), downloads);

            assertEquals(java.util.List.of("World One"), manager.worlds(instance));
        }
    }
}
