package net.stratalauncher.core.modrinth;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonParseException;
import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.download.DownloadTask;
import net.stratalauncher.core.instance.Instance;
import net.stratalauncher.core.loader.LoaderType;

import java.io.IOException;
import java.net.URI;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Objects;
import java.util.function.Consumer;

public final class ContentManager {
    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();
    private static final String STATE_FILE = ".stratalauncher-content.json";

    private final ModrinthClient modrinthClient;
    private final DownloadManager downloadManager;

    public ContentManager(ModrinthClient modrinthClient, DownloadManager downloadManager) {
        this.modrinthClient = Objects.requireNonNull(modrinthClient);
        this.downloadManager = Objects.requireNonNull(downloadManager);
    }

    public List<InstalledContent> installed(Instance instance, ContentType type) throws IOException {
        ContentState state = readState(instance);
        return state.entries.stream().filter(entry -> entry.type == type).map(this::toInstalledContent).toList();
    }

    public List<String> worlds(Instance instance) throws IOException {
        Path saves = instance.gameDirectory().resolve("saves");
        if (!Files.isDirectory(saves)) {
            return List.of();
        }
        try (var entries = Files.list(saves)) {
            return entries.filter(Files::isDirectory)
                    .map(path -> path.getFileName().toString())
                    .filter(ContentManager::isSafeWorldName)
                    .sorted(String.CASE_INSENSITIVE_ORDER)
                    .toList();
        }
    }

    public InstalledContent install(Instance instance, ContentType type, LoaderType loader,
                                    String worldName, ModrinthClient.Project project,
                                    ModrinthClient.ProjectVersion version,
                                    Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        Objects.requireNonNull(instance);
        Objects.requireNonNull(type);
        Objects.requireNonNull(project);
        Objects.requireNonNull(version);
        if (!project.id().equals(version.projectId()) || !type.projectType().equals(version.projectType())) {
            throw new IOException("Selected Modrinth version does not match the project and content category.");
        }
        if (!version.gameVersions().contains(instance.versionId())) {
            throw new IOException("Selected Modrinth version is not compatible with Minecraft "
                    + instance.versionId() + ".");
        }
        if (type == ContentType.MOD && (loader == null || loader == LoaderType.VANILLA
                || !version.loaders().contains(modrinthLoader(loader)))) {
            throw new IOException("Selected mod version is not compatible with this instance's installed loader.");
        }
        if (type == ContentType.DATA_PACK && !worlds(instance).contains(worldName)) {
            throw new IOException("Select an existing world before installing a data pack.");
        }

        ModrinthClient.VersionFile file = version.primaryFile();
        validateFile(file);
        Path gameRoot = instance.gameDirectory().toAbsolutePath().normalize();
        Path targetDirectory = type.directory(gameRoot, type == ContentType.DATA_PACK ? worldName : null);
        Files.createDirectories(targetDirectory);
        ensureInsideGameDirectory(gameRoot, targetDirectory);
        Path target = targetDirectory.resolve(file.filename()).normalize();
        if (!target.startsWith(targetDirectory) || Files.exists(target)) {
            throw new IOException("A content file with this name already exists in the selected instance.");
        }

        downloadManager.downloadAll(List.of(new DownloadTask(URI.create(file.url()), target,
                file.sha1(), project.title() + " " + version.versionNumber())), progress);
        InstalledEntry entry = new InstalledEntry(type, project.id(), project.slug(), project.title(),
                version.id(), version.versionNumber(), type == ContentType.DATA_PACK ? worldName : "",
                file.filename(), Instant.now().toString());
        ContentState state = readState(instance);
        state.entries.add(entry);
        try {
            writeState(instance, state);
        } catch (IOException exception) {
            try {
                Files.deleteIfExists(target);
            } catch (IOException rollbackException) {
                exception.addSuppressed(rollbackException);
            }
            throw exception;
        }
        return toInstalledContent(entry);
    }

    public void remove(Instance instance, InstalledContent content) throws IOException {
        Objects.requireNonNull(content);
        ContentState state = readState(instance);
        InstalledEntry entry = state.entries.stream()
                .filter(candidate -> candidate.type == content.type()
                        && candidate.projectId.equals(content.projectId())
                        && candidate.versionId.equals(content.versionId())
                        && candidate.filename.equals(content.filename())
                        && candidate.worldName.equals(content.worldName()))
                .findFirst().orElseThrow(() -> new IOException("Installed content entry was not found."));
        Path gameRoot = instance.gameDirectory().toAbsolutePath().normalize();
        Path targetDirectory = entry.type.directory(gameRoot,
                entry.type == ContentType.DATA_PACK ? entry.worldName : null);
        Path target = targetDirectory.resolve(entry.filename).normalize();
        if (!target.startsWith(targetDirectory) || !target.startsWith(gameRoot)) {
            throw new IOException("Installed content entry contains an unsafe path.");
        }
        if (Files.exists(targetDirectory)) {
            ensureInsideGameDirectory(gameRoot, targetDirectory);
        }
        if (Files.exists(target) && Files.isSymbolicLink(target)) {
            throw new IOException("Refusing to remove a symbolic-link content file.");
        }
        Files.deleteIfExists(target);
        state.entries.remove(entry);
        writeState(instance, state);
    }

    private ContentState readState(Instance instance) throws IOException {
        Path path = instance.gameDirectory().resolve(STATE_FILE);
        if (!Files.isRegularFile(path)) {
            return new ContentState(new ArrayList<>());
        }
        try {
            ContentState state = GSON.fromJson(Files.readString(path), ContentState.class);
            if (state == null || state.entries == null) {
                throw new IOException("Instance content state is incomplete.");
            }
            for (InstalledEntry entry : state.entries) {
                validateEntry(entry);
            }
            return state;
        } catch (JsonParseException | IllegalArgumentException exception) {
            throw new IOException("Instance content state is invalid.", exception);
        }
    }

    private void writeState(Instance instance, ContentState state) throws IOException {
        Path path = instance.gameDirectory().resolve(STATE_FILE);
        Files.createDirectories(path.getParent());
        Path temporary = Files.createTempFile(path.getParent(), "content-", ".tmp");
        try {
            Files.writeString(temporary, GSON.toJson(state));
            try {
                Files.move(temporary, path, StandardCopyOption.ATOMIC_MOVE,
                        StandardCopyOption.REPLACE_EXISTING);
            } catch (java.nio.file.AtomicMoveNotSupportedException exception) {
                Files.move(temporary, path, StandardCopyOption.REPLACE_EXISTING);
            }
        } finally {
            Files.deleteIfExists(temporary);
        }
    }

    private InstalledContent toInstalledContent(InstalledEntry entry) {
        return new InstalledContent(entry.type, entry.projectId, entry.slug, entry.title,
                entry.versionId, entry.versionNumber, entry.worldName, entry.filename, entry.installedAt);
    }

    public static void validateFile(ModrinthClient.VersionFile file) throws IOException {
        String filename = file.filename();
        if (filename == null || filename.isBlank() || filename.equals(".") || filename.equals("..")
                || filename.contains("/") || filename.contains("\\")
                || filename.chars().anyMatch(Character::isISOControl)) {
            throw new IOException("Modrinth returned an unsafe content filename.");
        }
        URI uri;
        try {
            uri = URI.create(file.url());
        } catch (IllegalArgumentException exception) {
            throw new IOException("Modrinth returned an invalid content download URL.", exception);
        }
        String host = uri.getHost() == null ? "" : uri.getHost().toLowerCase(Locale.ROOT);
        if (!"https".equalsIgnoreCase(uri.getScheme())
                || uri.getRawUserInfo() != null || uri.getPort() != -1 && uri.getPort() != 443
                || !(host.equals("modrinth.com") || host.endsWith(".modrinth.com")
                || host.equals("modrinth.gg") || host.endsWith(".modrinth.gg"))) {
            throw new IOException("Modrinth file downloads must use the official HTTPS CDN.");
        }
        if (file.sha1() == null || !file.sha1().matches("(?i)[0-9a-f]{40}")) {
            throw new IOException("Modrinth file is missing a valid SHA-1 integrity hash.");
        }
    }

    private static String modrinthLoader(LoaderType loader) throws IOException {
        return switch (loader) {
            case FABRIC -> "fabric";
            case QUILT -> "quilt";
            case FORGE -> "forge";
            case NEOFORGE -> "neoforge";
            case VANILLA -> throw new IOException("Vanilla instances cannot install mods.");
        };
    }

    private static void ensureInsideGameDirectory(Path gameRoot, Path targetDirectory) throws IOException {
        Path realRoot = gameRoot.toRealPath();
        Path realTarget = targetDirectory.toRealPath();
        if (!realTarget.startsWith(realRoot)) {
            throw new IOException("Content directory resolves outside this instance.");
        }
    }

    private static boolean isSafeWorldName(String name) {
        return name != null && !name.isBlank() && !name.equals(".") && !name.equals("..")
                && !name.contains("/") && !name.contains("\\")
                && name.chars().noneMatch(Character::isISOControl);
    }

    private static void validateEntry(InstalledEntry entry) {
        if (entry == null || entry.type == null || entry.projectId == null || entry.versionId == null
                || entry.filename == null || entry.worldName == null
                || entry.type == ContentType.DATA_PACK && !isSafeWorldName(entry.worldName)
                || entry.type != ContentType.DATA_PACK && !entry.worldName.isEmpty()
                || entry.filename.isBlank() || entry.filename.contains("/") || entry.filename.contains("\\")
                || entry.filename.equals(".") || entry.filename.equals("..")) {
            throw new IllegalArgumentException("Invalid installed content entry.");
        }
    }

    public record InstalledContent(ContentType type, String projectId, String slug, String title,
                                   String versionId, String versionNumber, String worldName,
                                   String filename, String installedAt) {
    }

    private static final class ContentState {
        private List<InstalledEntry> entries;

        private ContentState(List<InstalledEntry> entries) {
            this.entries = entries;
        }
    }

    private static final class InstalledEntry {
        private ContentType type;
        private String projectId;
        private String slug;
        private String title;
        private String versionId;
        private String versionNumber;
        private String worldName;
        private String filename;
        private String installedAt;

        private InstalledEntry(ContentType type, String projectId, String slug, String title,
                               String versionId, String versionNumber, String worldName,
                               String filename, String installedAt) {
            this.type = type;
            this.projectId = projectId;
            this.slug = slug;
            this.title = title;
            this.versionId = versionId;
            this.versionNumber = versionNumber;
            this.worldName = worldName;
            this.filename = filename;
            this.installedAt = installedAt;
        }
    }
}
