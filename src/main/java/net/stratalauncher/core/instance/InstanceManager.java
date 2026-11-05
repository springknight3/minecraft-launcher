package net.stratalauncher.core.instance;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonParseException;

import java.io.IOException;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.StandardCopyOption;
import java.nio.file.attribute.BasicFileAttributes;
import java.time.Instant;
import java.time.format.DateTimeParseException;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.UUID;
import java.util.regex.Pattern;
import java.util.stream.Stream;

public class InstanceManager {
    private static final Pattern INSTANCE_ID = Pattern.compile("[0-9a-fA-F-]{36}");
    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();

    private final Path dataDirectory;
    private final Path instancesDirectory;

    public InstanceManager() {
        this(defaultDataDirectory());
    }

    public InstanceManager(Path dataDirectory) {
        this.dataDirectory = dataDirectory.toAbsolutePath().normalize();
        instancesDirectory = this.dataDirectory.resolve("instances");
    }

    public static Path defaultDataDirectory() {
        String os = System.getProperty("os.name", "").toLowerCase(Locale.ROOT);
        String home = System.getProperty("user.home");
        if (os.contains("win")) {
            String appData = System.getenv("APPDATA");
            return (appData == null || appData.isBlank()
                    ? Path.of(home, "AppData", "Roaming")
                    : Path.of(appData)).resolve("StrataLauncher");
        }
        if (os.contains("mac")) {
            return Path.of(home, "Library", "Application Support", "StrataLauncher");
        }
        String xdgDataHome = System.getenv("XDG_DATA_HOME");
        Path base = xdgDataHome == null || xdgDataHome.isBlank()
                ? Path.of(home, ".local", "share")
                : Path.of(xdgDataHome);
        return base.resolve("StrataLauncher");
    }

    public Path dataDirectory() {
        return dataDirectory;
    }

    public List<Instance> listInstances() throws IOException {
        Files.createDirectories(instancesDirectory);
        List<Instance> instances = new java.util.ArrayList<>();
        try (Stream<Path> entries = Files.list(instancesDirectory)) {
            for (Path entry : entries.filter(Files::isDirectory).toList()) {
                instances.add(readInstance(entry));
            }
        }
        instances.sort(Comparator.comparing(Instance::favorite).reversed()
                .thenComparing(Instance::name, String.CASE_INSENSITIVE_ORDER));
        return List.copyOf(instances);
    }

    public Instance create(String name, String versionId) throws IOException {
        String normalizedName = requireText(name, "Instance name");
        String normalizedVersion = requireText(versionId, "Minecraft version");
        Files.createDirectories(instancesDirectory);
        String id = UUID.randomUUID().toString();
        Path directory = instancesDirectory.resolve(id);
        Files.createDirectory(directory);
        Files.createDirectory(directory.resolve("game"));
        Instance instance = new Instance(id, normalizedName, normalizedVersion, Instant.now(), directory);
        try {
            writeMetadata(instance);
        } catch (IOException exception) {
            deleteTree(directory);
            throw exception;
        }
        return instance;
    }

    public Instance duplicate(Instance source, String newName) throws IOException {
        Instance storedSource = requireStoredInstance(source.id());
        String normalizedName = requireText(newName, "Instance name");
        Files.createDirectories(instancesDirectory);
        String id = UUID.randomUUID().toString();
        Path directory = instancesDirectory.resolve(id);
        Files.createDirectory(directory);
        try {
            copyTree(storedSource.gameDirectory(), directory.resolve("game"));
            Instance duplicate = new Instance(id, normalizedName, storedSource.versionId(), Instant.now(), directory,
                    storedSource.favorite());
            writeMetadata(duplicate);
            return duplicate;
        } catch (IOException exception) {
            deleteTree(directory);
            throw exception;
        }
    }

    public Instance rename(Instance instance, String newName) throws IOException {
        Instance storedInstance = requireStoredInstance(instance.id());
        String normalizedName = requireText(newName, "Instance name");
        Instance renamed = new Instance(storedInstance.id(), normalizedName, storedInstance.versionId(),
                storedInstance.createdAt(), storedInstance.directory(), storedInstance.favorite());
        writeMetadata(renamed);
        return renamed;
    }

    public Instance setFavorite(Instance instance, boolean favorite) throws IOException {
        Instance storedInstance = requireStoredInstance(instance.id());
        Instance updated = new Instance(storedInstance.id(), storedInstance.name(), storedInstance.versionId(),
                storedInstance.createdAt(), storedInstance.directory(), favorite);
        writeMetadata(updated);
        return updated;
    }

    public void delete(Instance instance) throws IOException {
        Instance stored = requireStoredInstance(instance.id());
        deleteTree(stored.directory());
    }

    private Instance requireStoredInstance(String id) throws IOException {
        if (id == null || !INSTANCE_ID.matcher(id).matches()) {
            throw new IOException("Invalid instance identifier");
        }
        Path directory = instancesDirectory.resolve(id).normalize();
        if (!directory.getParent().equals(instancesDirectory) || !Files.isDirectory(directory)) {
            throw new IOException("Instance does not exist: " + id);
        }
        return readInstance(directory);
    }

    private Instance readInstance(Path directory) throws IOException {
        Path metadataPath = directory.resolve("instance.json");
        try {
            Metadata metadata = GSON.fromJson(Files.readString(metadataPath), Metadata.class);
            if (metadata == null || metadata.id == null || !metadata.id.equals(directory.getFileName().toString())
                    || metadata.name == null || metadata.versionId == null || metadata.createdAt == null) {
                throw new IOException("Instance metadata is incomplete: " + metadataPath);
            }
            return new Instance(metadata.id, metadata.name, metadata.versionId,
                    Instant.parse(metadata.createdAt), directory, metadata.favorite);
        } catch (JsonParseException exception) {
            throw new IOException("Could not parse instance metadata: " + metadataPath, exception);
        } catch (DateTimeParseException exception) {
            throw new IOException("Instance creation time is invalid: " + metadataPath, exception);
        }
    }

    private void writeMetadata(Instance instance) throws IOException {
        Metadata metadata = new Metadata();
        metadata.id = instance.id();
        metadata.name = instance.name();
        metadata.versionId = instance.versionId();
        metadata.createdAt = instance.createdAt().toString();
        metadata.favorite = instance.favorite();
        Path metadataPath = instance.directory().resolve("instance.json");
        Path temporary = Files.createTempFile(instance.directory(), "instance-", ".tmp");
        try {
            Files.writeString(temporary, GSON.toJson(metadata));
            try {
                Files.move(temporary, metadataPath, StandardCopyOption.ATOMIC_MOVE,
                        StandardCopyOption.REPLACE_EXISTING);
            } catch (java.nio.file.AtomicMoveNotSupportedException exception) {
                Files.move(temporary, metadataPath, StandardCopyOption.REPLACE_EXISTING);
            }
        } finally {
            Files.deleteIfExists(temporary);
        }
    }

    private static String requireText(String value, String label) {
        if (value == null || value.isBlank()) {
            throw new IllegalArgumentException(label + " must not be blank");
        }
        return value.trim();
    }

    private static void copyTree(Path source, Path target) throws IOException {
        if (!Files.isDirectory(source)) {
            throw new IOException("Instance game directory is missing: " + source);
        }
        Files.walkFileTree(source, new SimpleFileVisitor<>() {
            @Override
            public FileVisitResult preVisitDirectory(Path directory, BasicFileAttributes attributes) throws IOException {
                if (attributes.isSymbolicLink()) {
                    return FileVisitResult.SKIP_SUBTREE;
                }
                Files.createDirectories(target.resolve(source.relativize(directory)));
                return FileVisitResult.CONTINUE;
            }

            @Override
            public FileVisitResult visitFile(Path file, BasicFileAttributes attributes) throws IOException {
                if (attributes.isRegularFile() && !attributes.isSymbolicLink()) {
                    Files.copy(file, target.resolve(source.relativize(file)), StandardCopyOption.COPY_ATTRIBUTES);
                }
                return FileVisitResult.CONTINUE;
            }
        });
    }

    private static void deleteTree(Path target) throws IOException {
        if (!Files.exists(target)) {
            return;
        }
        Files.walkFileTree(target, new SimpleFileVisitor<>() {
            @Override
            public FileVisitResult visitFile(Path file, BasicFileAttributes attributes) throws IOException {
                Files.delete(file);
                return FileVisitResult.CONTINUE;
            }

            @Override
            public FileVisitResult postVisitDirectory(Path directory, IOException exception) throws IOException {
                if (exception != null) {
                    throw exception;
                }
                Files.delete(directory);
                return FileVisitResult.CONTINUE;
            }
        });
    }

    private static final class Metadata {
        private String id;
        private String name;
        private String versionId;
        private String createdAt;
        private boolean favorite;
    }
}
