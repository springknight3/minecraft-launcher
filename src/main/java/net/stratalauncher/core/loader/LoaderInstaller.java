package net.stratalauncher.core.loader;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import com.google.gson.JsonParser;
import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.download.DownloadManager.Progress;
import net.stratalauncher.core.instance.Instance;

import java.io.IOException;
import java.net.http.HttpClient;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.time.Duration;
import java.util.List;
import java.util.Objects;
import java.util.function.Consumer;

public final class LoaderInstaller {
    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();
    private static final String STATE_FILE = ".stratalauncher-loader.json";
    private static final String PROFILE_FILE = "loader-profile.json";

    private final Path installerCache;
    private final FabricInstaller fabricInstaller;
    private final QuiltInstaller quiltInstaller;
    private final ForgeInstaller forgeInstaller;
    private final NeoForgeInstaller neoForgeInstaller;

    public LoaderInstaller(Path dataDirectory, DownloadManager downloadManager) {
        Objects.requireNonNull(dataDirectory);
        Objects.requireNonNull(downloadManager);
        HttpClient httpClient = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(15))
                .followRedirects(HttpClient.Redirect.NORMAL)
                .build();
        LoaderMetadataClient metadataClient = new LoaderMetadataClient(httpClient);
        Path normalizedDataDirectory = dataDirectory.toAbsolutePath().normalize();
        installerCache = normalizedDataDirectory.resolve("loader-installers");
        Path librariesDirectory = normalizedDataDirectory.resolve("libraries");
        fabricInstaller = new FabricInstaller(metadataClient, downloadManager, librariesDirectory);
        quiltInstaller = new QuiltInstaller(metadataClient, downloadManager, librariesDirectory);
        forgeInstaller = new ForgeInstaller(metadataClient, downloadManager, installerCache);
        neoForgeInstaller = new NeoForgeInstaller(metadataClient, downloadManager, installerCache);
    }

    public List<String> availableVersions(LoaderType type, String minecraftVersion)
            throws IOException, InterruptedException {
        Objects.requireNonNull(type);
        return switch (type) {
            case VANILLA -> List.of();
            case FABRIC -> fabricInstaller.versions(minecraftVersion);
            case QUILT -> quiltInstaller.versions(minecraftVersion);
            case FORGE -> forgeInstaller.versions(minecraftVersion);
            case NEOFORGE -> neoForgeInstaller.versions(minecraftVersion);
        };
    }

    public LoaderSelection installedSelection(Instance instance) throws IOException {
        Path stateFile = instance.gameDirectory().resolve(STATE_FILE);
        if (!Files.isRegularFile(stateFile)) {
            return LoaderSelection.vanilla();
        }
        try {
            InstalledState state = GSON.fromJson(Files.readString(stateFile), InstalledState.class);
            if (state == null || state.type == null) {
                throw new IOException("Loader state is incomplete: " + stateFile);
            }
            LoaderType type = LoaderType.valueOf(state.type);
            if (type == LoaderType.VANILLA) {
                return LoaderSelection.vanilla();
            }
            return new LoaderSelection(type, state.loaderVersion);
        } catch (JsonParseException | IllegalArgumentException exception) {
            throw new IOException("Loader state is invalid: " + stateFile, exception);
        }
    }

    public LoaderProfile installedProfile(Instance instance) throws IOException {
        Path stateFile = instance.gameDirectory().resolve(STATE_FILE);
        if (!Files.isRegularFile(stateFile)) {
            return null;
        }
        try {
            InstalledState state = GSON.fromJson(Files.readString(stateFile), InstalledState.class);
            if (state == null || state.type == null || LoaderType.valueOf(state.type) == LoaderType.VANILLA) {
                return null;
            }
            if (!state.installed) {
                throw new IOException("Loader installation is incomplete. Use Install / Repair before launching.");
            }
            if (!instance.versionId().equals(state.minecraftVersion)) {
                throw new IOException("Installed loader targets a different Minecraft version. Use Install / Repair.");
            }
            Path profileFile = instance.gameDirectory().resolve(PROFILE_FILE);
            JsonObject profile = JsonParser.parseString(Files.readString(profileFile)).getAsJsonObject();
            String profileId = requiredString(profile, "id");
            if (!profileId.equals(state.profileVersionId)) {
                throw new IOException("Installed loader profile is inconsistent. Use Install / Repair.");
            }
            return new LoaderProfile(LoaderType.valueOf(state.type), state.minecraftVersion,
                    state.loaderVersion, state.profileVersionId, profile);
        } catch (JsonParseException | IllegalArgumentException exception) {
            throw new IOException("Installed loader profile is invalid. Use Install / Repair.", exception);
        }
    }

    public void install(Instance instance, LoaderSelection selection, Consumer<Progress> progress)
            throws IOException, InterruptedException {
        Objects.requireNonNull(instance);
        Objects.requireNonNull(selection);
        Consumer<Progress> listener = progress == null ? ignored -> { } : progress;
        Path stateFile = instance.gameDirectory().resolve(STATE_FILE);
        Path profileFile = instance.gameDirectory().resolve(PROFILE_FILE);
        Files.createDirectories(instance.gameDirectory());
        if (selection.type() == LoaderType.VANILLA) {
            prepare(instance, selection);
            listener.accept(new Progress(0, 0, "Vanilla installation complete"));
            return;
        }

        prepare(instance, selection);
        List<String> compatibleVersions = availableVersions(selection.type(), instance.versionId());
        if (!compatibleVersions.contains(selection.version())) {
            throw new IOException(selection.type() + " " + selection.version()
                    + " is not available for Minecraft " + instance.versionId() + ".");
        }
        listener.accept(new Progress(0, 0, "Preparing " + selection.type() + " " + selection.version()));

        LoaderProfile profile = switch (selection.type()) {
            case FABRIC -> fabricInstaller.install(instance.versionId(), selection.version(), listener);
            case QUILT -> quiltInstaller.install(instance.versionId(), selection.version(), listener);
            case FORGE -> {
                listener.accept(new Progress(0, 0, "Running the official Forge client installer"));
                yield forgeInstaller.install(instance, instance.versionId(), selection.version(), listener);
            }
            case NEOFORGE -> {
                listener.accept(new Progress(0, 0, "Running the official NeoForge client installer"));
                yield neoForgeInstaller.install(instance, instance.versionId(), selection.version(), listener);
            }
            case VANILLA -> throw new IllegalStateException("Vanilla installation was handled above.");
        };
        if (!instance.versionId().equals(profile.minecraftVersion())
                || profile.type() != selection.type()
                || !selection.version().equals(profile.loaderVersion())) {
            throw new IOException("Loader installer returned a profile that does not match the requested instance.");
        }
        writeJson(profileFile, profile.launchMetadata());
        writeState(stateFile, new InstalledState(selection.type().name(), instance.versionId(),
                selection.version(), profile.profileVersionId(), true));
    }

    public void prepare(Instance instance, LoaderSelection selection) throws IOException {
        Path stateFile = instance.gameDirectory().resolve(STATE_FILE);
        Path profileFile = instance.gameDirectory().resolve(PROFILE_FILE);
        Files.createDirectories(instance.gameDirectory());
        if (selection.type() == LoaderType.VANILLA) {
            Files.deleteIfExists(stateFile);
            Files.deleteIfExists(profileFile);
            return;
        }
        writeState(stateFile, new InstalledState(selection.type().name(), instance.versionId(),
                selection.version(), "", false));
        Files.deleteIfExists(profileFile);
    }

    private static String requiredString(JsonObject object, String key) throws IOException {
        if (!object.has(key) || !object.get(key).isJsonPrimitive()
                || object.get(key).getAsString().isBlank()) {
            throw new IOException("Loader profile is missing " + key + ".");
        }
        return object.get(key).getAsString();
    }

    private static void writeState(Path path, InstalledState state) throws IOException {
        writeJson(path, GSON.toJsonTree(state).getAsJsonObject());
    }

    private static void writeJson(Path path, JsonObject json) throws IOException {
        Files.createDirectories(path.getParent());
        Path temporary = Files.createTempFile(path.getParent(), "loader-", ".tmp");
        try {
            Files.writeString(temporary, GSON.toJson(json));
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

    private record InstalledState(String type, String minecraftVersion, String loaderVersion,
                                  String profileVersionId, boolean installed) {
    }
}
