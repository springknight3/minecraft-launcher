package net.stratalauncher.core.settings;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class ServerListStore {
    private static final Pattern VERSION_MAJOR = Pattern.compile("^(\\d+)\\.");

    private final Path file;
    private final NbtFile.Document document;
    private final boolean compressedOnSave;

    public ServerListStore(Path file) throws IOException {
        this(file, null);
    }

    public ServerListStore(Path file, String minecraftVersion) throws IOException {
        this.file = file.toAbsolutePath().normalize();
        boolean existed = Files.isRegularFile(this.file);
        boolean existingCompression = existed && NbtFile.isCompressed(this.file);
        document = existed
                ? NbtFile.read(this.file) : new NbtFile.Document("", new LinkedHashMap<>());
        compressedOnSave = minecraftVersion == null
                ? (existed ? existingCompression : true) : usesCompressedNbt(minecraftVersion);
        servers();
        if (existed && existingCompression != compressedOnSave) {
            save();
        }
    }

    public List<Server> servers() throws IOException {
        List<Server> servers = new ArrayList<>();
        for (NbtFile.Tag tag : serverTags()) {
            Map<String, NbtFile.Tag> entry = compound(tag, "server list entry");
            servers.add(new Server(stringValue(entry.get("name")), stringValue(entry.get("ip"))));
        }
        return List.copyOf(servers);
    }

    public void add(String name, String address) throws IOException {
        List<NbtFile.Tag> entries = serverTags();
        Map<String, NbtFile.Tag> fields = new LinkedHashMap<>();
        fields.put("name", new NbtFile.Tag((byte) 8, requireText(name, "Server name")));
        fields.put("ip", new NbtFile.Tag((byte) 8, requireText(address, "Server address")));
        entries.add(new NbtFile.Tag((byte) 10, fields));
        replaceServerTags(entries);
    }

    public void update(int index, String name, String address) throws IOException {
        List<NbtFile.Tag> entries = serverTags();
        Map<String, NbtFile.Tag> fields = compound(entries.get(index), "server list entry");
        String normalizedName = requireText(name, "Server name");
        String normalizedAddress = requireText(address, "Server address");
        fields.put("name", new NbtFile.Tag((byte) 8, normalizedName));
        fields.put("ip", new NbtFile.Tag((byte) 8, normalizedAddress));
        replaceServerTags(entries);
    }

    public void remove(int index) throws IOException {
        List<NbtFile.Tag> entries = serverTags();
        entries.remove(index);
        replaceServerTags(entries);
    }

    public void save() throws IOException {
        Files.createDirectories(file.getParent());
        Path temporary = Files.createTempFile(file.getParent(), "servers-", ".dat");
        try {
            NbtFile.write(temporary, document, compressedOnSave);
            try {
                Files.move(temporary, file, StandardCopyOption.ATOMIC_MOVE,
                        StandardCopyOption.REPLACE_EXISTING);
            } catch (java.nio.file.AtomicMoveNotSupportedException exception) {
                Files.move(temporary, file, StandardCopyOption.REPLACE_EXISTING);
            }
        } finally {
            Files.deleteIfExists(temporary);
        }
    }

    private List<NbtFile.Tag> serverTags() throws IOException {
        NbtFile.Tag serversTag = document.root().get("servers");
        if (serversTag == null) {
            return new ArrayList<>();
        }
        if (serversTag.type() != 9 || !(serversTag.value() instanceof NbtFile.ListValue list)
                || list.elementType() != 10) {
            throw new IOException("servers.dat has an invalid server list; it was not modified.");
        }
        return new ArrayList<>(list.elements());
    }

    private void replaceServerTags(List<NbtFile.Tag> entries) {
        document.root().put("servers",
                new NbtFile.Tag((byte) 9, new NbtFile.ListValue((byte) 10, List.copyOf(entries))));
    }

    @SuppressWarnings("unchecked")
    private static Map<String, NbtFile.Tag> compound(NbtFile.Tag tag, String label) throws IOException {
        if (tag.type() != 10 || !(tag.value() instanceof Map<?, ?>)) {
            throw new IOException("servers.dat contains an invalid " + label + "; it was not modified.");
        }
        return (Map<String, NbtFile.Tag>) tag.value();
    }

    private static String stringValue(NbtFile.Tag tag) {
        return tag != null && tag.type() == 8 ? (String) tag.value() : "";
    }

    private static String requireText(String text, String label) {
        String normalized = Objects.requireNonNull(text, label).trim();
        if (normalized.isEmpty()) {
            throw new IllegalArgumentException(label + " cannot be empty.");
        }
        return normalized;
    }

    private static boolean usesCompressedNbt(String minecraftVersion) {
        Matcher matcher = VERSION_MAJOR.matcher(minecraftVersion);
        return !matcher.find() || Integer.parseInt(matcher.group(1)) < 26;
    }

    public record Server(String name, String address) {
    }
}
