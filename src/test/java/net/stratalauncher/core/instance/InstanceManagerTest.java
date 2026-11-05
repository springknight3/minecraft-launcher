package net.stratalauncher.core.instance;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

class InstanceManagerTest {
    @TempDir
    Path temporaryDirectory;

    @Test
    void createsAndListsInstancesWithIsolatedGameDirectories() throws IOException {
        InstanceManager manager = new InstanceManager(temporaryDirectory.resolve("launcher-data"));

        Instance created = manager.create("My World", "1.21.1");

        assertTrue(Files.isDirectory(created.gameDirectory()));
        assertEquals(temporaryDirectory.resolve("launcher-data"), manager.dataDirectory());
        assertEquals(created, manager.listInstances().getFirst());
    }

    @Test
    void duplicateCopiesGameFilesIntoANewIsolatedDirectoryAndDeleteRemovesOnlyCopy() throws IOException {
        InstanceManager manager = new InstanceManager(temporaryDirectory.resolve("launcher-data"));
        Instance original = manager.create("Original", "1.21.1");
        Path save = original.gameDirectory().resolve("saves/world/level.dat");
        Files.createDirectories(save.getParent());
        Files.writeString(save, "world data");

        Instance duplicate = manager.duplicate(original, "Copy");

        assertNotEquals(original.id(), duplicate.id());
        assertNotEquals(original.gameDirectory(), duplicate.gameDirectory());
        assertEquals("world data", Files.readString(duplicate.gameDirectory().resolve("saves/world/level.dat")));
        manager.delete(duplicate);
        assertTrue(Files.exists(original.gameDirectory()));
        assertFalse(Files.exists(duplicate.directory()));
        assertEquals(1, manager.listInstances().size());
    }

    @Test
    void rejectsBlankNamesAndVersions() {
        InstanceManager manager = new InstanceManager(temporaryDirectory.resolve("launcher-data"));

        assertThrows(IllegalArgumentException.class, () -> manager.create("  ", "1.21.1"));
        assertThrows(IllegalArgumentException.class, () -> manager.create("Instance", " "));
    }

    @Test
    void renamesAnInstanceAndPersistsTheDisplayName() throws IOException {
        InstanceManager manager = new InstanceManager(temporaryDirectory.resolve("launcher-data"));
        Instance original = manager.create("Original name", "1.21.1");

        Instance renamed = manager.rename(original, "  New name  ");

        assertEquals("New name", renamed.name());
        assertEquals("New name", manager.listInstances().getFirst().name());
        assertEquals(original.id(), renamed.id());
        assertEquals(original.directory(), renamed.directory());
        assertThrows(IllegalArgumentException.class, () -> manager.rename(renamed, " "));
    }

    @Test
    void persistsFavoritesAndListsThemFirst() throws IOException {
        InstanceManager manager = new InstanceManager(temporaryDirectory.resolve("launcher-data"));
        manager.create("Alpha", "1.21.1");
        Instance second = manager.create("Beta", "1.21.1");

        Instance favorite = manager.setFavorite(second, true);

        assertTrue(favorite.favorite());
        assertEquals("Beta", manager.listInstances().getFirst().name());
        assertEquals(favorite, manager.listInstances().getFirst());
        assertFalse(manager.setFavorite(favorite, false).favorite());
    }

    @Test
    void treatsFavoritesMissingFromOlderMetadataAsFalse() throws IOException {
        InstanceManager manager = new InstanceManager(temporaryDirectory.resolve("launcher-data"));
        Instance instance = manager.create("Legacy", "1.21.1");
        Files.writeString(instance.directory().resolve("instance.json"), """
                {
                  "id": "%s",
                  "name": "Legacy",
                  "versionId": "1.21.1",
                  "createdAt": "%s"
                }
                """.formatted(instance.id(), instance.createdAt()));

        assertFalse(manager.listInstances().getFirst().favorite());
    }
}
