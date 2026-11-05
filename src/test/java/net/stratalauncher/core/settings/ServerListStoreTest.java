package net.stratalauncher.core.settings;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;

class ServerListStoreTest {
    @TempDir
    Path temporaryDirectory;

    @Test
    void editsServerFieldsWhileRetainingUnknownRootAndEntryTags() throws IOException {
        Path file = temporaryDirectory.resolve("servers.dat");
        Map<String, NbtFile.Tag> server = new LinkedHashMap<>();
        server.put("name", new NbtFile.Tag((byte) 8, "Old name"));
        server.put("ip", new NbtFile.Tag((byte) 8, "old.example"));
        server.put("acceptTextures", new NbtFile.Tag((byte) 1, (byte) 1));
        server.put("futureServerData", new NbtFile.Tag((byte) 3, 42));
        Map<String, NbtFile.Tag> root = new LinkedHashMap<>();
        root.put("servers", new NbtFile.Tag((byte) 9,
                new NbtFile.ListValue((byte) 10,
                        new ArrayList<>(List.of(new NbtFile.Tag((byte) 10, server))))));
        root.put("futureRootData", new NbtFile.Tag((byte) 8, "keep-me"));
        NbtFile.write(file, new NbtFile.Document("root-name", root), true);

        ServerListStore store = new ServerListStore(file);
        store.update(0, "New name", "new.example");
        store.add("Second", "second.example");
        store.save();

        ServerListStore reloaded = new ServerListStore(file);
        assertEquals(List.of(
                new ServerListStore.Server("New name", "new.example"),
                new ServerListStore.Server("Second", "second.example")), reloaded.servers());
        NbtFile.Document saved = NbtFile.read(file);
        assertEquals("keep-me", saved.root().get("futureRootData").value());
        NbtFile.ListValue savedServers = (NbtFile.ListValue) saved.root().get("servers").value();
        @SuppressWarnings("unchecked")
        Map<String, NbtFile.Tag> savedFirst =
                (Map<String, NbtFile.Tag>) savedServers.elements().getFirst().value();
        assertEquals((byte) 1, savedFirst.get("acceptTextures").value());
        assertEquals(42, savedFirst.get("futureServerData").value());
        assertEquals("root-name", saved.rootName());
    }

    @Test
    void refusesToModifyMalformedServerLists() throws IOException {
        Path file = temporaryDirectory.resolve("bad-servers.dat");
        Map<String, NbtFile.Tag> root = new LinkedHashMap<>();
        root.put("servers", new NbtFile.Tag((byte) 8, "not a list"));
        NbtFile.write(file, new NbtFile.Document("", root), true);

        assertThrows(IOException.class, () -> new ServerListStore(file));
    }

    @Test
    void migratesLegacyCompressedServersToCurrentMinecraftNbtFormat() throws IOException {
        Path file = temporaryDirectory.resolve("migration-servers.dat");
        Map<String, NbtFile.Tag> server = new LinkedHashMap<>();
        server.put("name", new NbtFile.Tag((byte) 8, "Legacy"));
        server.put("ip", new NbtFile.Tag((byte) 8, "legacy.example"));
        Map<String, NbtFile.Tag> root = new LinkedHashMap<>();
        root.put("servers", new NbtFile.Tag((byte) 9,
                new NbtFile.ListValue((byte) 10,
                        new ArrayList<>(List.of(new NbtFile.Tag((byte) 10, server))))));
        NbtFile.write(file, new NbtFile.Document("", root), true);

        ServerListStore migrated = new ServerListStore(file, "26.3");

        assertFalse(NbtFile.isCompressed(file));
        assertEquals(List.of(new ServerListStore.Server("Legacy", "legacy.example")), migrated.servers());
        migrated.add("New", "new.example");
        migrated.save();
        assertFalse(NbtFile.isCompressed(file));
    }

    @Test
    void invalidUpdateDoesNotPartiallyChangeServerData() throws IOException {
        Path file = temporaryDirectory.resolve("stable-servers.dat");
        ServerListStore store = new ServerListStore(file);
        store.add("Original", "original.example");
        store.save();

        assertThrows(IllegalArgumentException.class, () -> store.update(0, "Changed", " "));
        store.save();

        assertEquals(List.of(new ServerListStore.Server("Original", "original.example")),
                new ServerListStore(file).servers());
    }
}
