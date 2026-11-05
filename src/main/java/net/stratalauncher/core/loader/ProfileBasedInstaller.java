package net.stratalauncher.core.loader;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.download.DownloadTask;

import java.io.IOException;
import java.net.URI;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Objects;
import java.util.function.Consumer;
import java.util.regex.Pattern;

abstract class ProfileBasedInstaller {
    private static final Pattern SAFE_VERSION = Pattern.compile("[A-Za-z0-9._+~-]+");

    private final LoaderType type;
    private final String metadataEndpoint;
    private final URI defaultMavenRepository;
    private final LoaderMetadataClient metadataClient;
    private final DownloadManager downloadManager;
    private final Path librariesDirectory;

    ProfileBasedInstaller(LoaderType type, String metadataEndpoint, URI defaultMavenRepository,
                          LoaderMetadataClient metadataClient, DownloadManager downloadManager,
                          Path librariesDirectory) {
        this.type = Objects.requireNonNull(type);
        this.metadataEndpoint = metadataEndpoint;
        this.defaultMavenRepository = defaultMavenRepository;
        this.metadataClient = metadataClient;
        this.downloadManager = downloadManager;
        this.librariesDirectory = librariesDirectory;
    }

    List<String> versions(String minecraftVersion) throws IOException, InterruptedException {
        JsonElement response = metadataClient.getJson(URI.create(metadataEndpoint + "/versions/loader/"
                + safeSegment(minecraftVersion)));
        if (!response.isJsonArray()) {
            throw new IOException(type + " returned an invalid loader version list.");
        }
        List<String> versions = new ArrayList<>();
        for (JsonElement element : response.getAsJsonArray()) {
            if (!element.isJsonObject()) {
                continue;
            }
            JsonObject loader = element.getAsJsonObject().getAsJsonObject("loader");
            if (loader != null && loader.has("version")) {
                String version = loader.get("version").getAsString();
                if (SAFE_VERSION.matcher(version).matches() && !versions.contains(version)) {
                    versions.add(version);
                }
            }
        }
        return List.copyOf(versions);
    }

    LoaderProfile install(String minecraftVersion, String loaderVersion,
                          Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        safeSegment(minecraftVersion);
        safeSegment(loaderVersion);
        URI profileUri = URI.create(metadataEndpoint + "/versions/loader/" + minecraftVersion + "/"
                + loaderVersion + "/profile/json");
        JsonElement json = metadataClient.getJson(profileUri);
        LoaderProfile parsed = parse(type, minecraftVersion, loaderVersion, json);
        JsonObject profile = parsed.launchMetadata();
        downloadLibraries(profile, progress);
        return parsed;
    }

    static LoaderProfile parse(LoaderType type, String minecraftVersion, String loaderVersion,
                               JsonElement json) throws IOException {
        if (json == null || !json.isJsonObject()) {
            throw new IOException(type + " returned an invalid launcher profile.");
        }
        JsonObject profile = json.getAsJsonObject();
        if (!minecraftVersion.equals(requiredString(profile, "inheritsFrom"))) {
            throw new IOException(type + " profile targets a different Minecraft version.");
        }
        String profileId = requiredString(profile, "id");
        requiredString(profile, "mainClass");
        if (profile.has("libraries") && !profile.get("libraries").isJsonArray()) {
            throw new IOException(type + " profile contains an invalid libraries list.");
        }
        return new LoaderProfile(type, minecraftVersion, loaderVersion, profileId, profile);
    }

    private void downloadLibraries(JsonObject profile, Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        JsonArray libraries = profile.has("libraries") && profile.get("libraries").isJsonArray()
                ? profile.getAsJsonArray("libraries") : new JsonArray();
        List<DownloadTask> tasks = new ArrayList<>();
        for (JsonElement element : libraries) {
            if (!element.isJsonObject()) {
                continue;
            }
            JsonObject library = element.getAsJsonObject();
            if (!isAllowedOnCurrentOs(library)) {
                continue;
            }
            String name = requiredString(library, "name");
            MavenArtifact artifact = MavenArtifact.parse(name);
            JsonObject downloads = library.has("downloads") && library.get("downloads").isJsonObject()
                    ? library.getAsJsonObject("downloads") : null;
            JsonObject descriptor = downloads != null && downloads.has("artifact")
                    && downloads.get("artifact").isJsonObject()
                    ? downloads.getAsJsonObject("artifact") : null;
            String relativePath = descriptor != null && descriptor.has("path")
                    ? descriptor.get("path").getAsString() : artifact.relativePath();
            Path destination = safeResolve(librariesDirectory, relativePath);
            URI source;
            if (descriptor != null && descriptor.has("url")) {
                source = URI.create(descriptor.get("url").getAsString());
            } else {
                URI repository = library.has("url")
                        ? URI.create(library.get("url").getAsString()) : defaultMavenRepository;
                if (!"https".equalsIgnoreCase(repository.getScheme())) {
                    throw new IOException("Loader library repository must use HTTPS.");
                }
                source = repository.resolve(artifact.relativePath());
            }
            if (!"https".equalsIgnoreCase(source.getScheme())) {
                throw new IOException("Loader library must use HTTPS.");
            }
            String sha1 = descriptor != null && descriptor.has("sha1")
                    ? descriptor.get("sha1").getAsString() : "";
            tasks.add(new DownloadTask(source, destination, sha1, name));
        }
        downloadManager.downloadAll(tasks, progress);
    }

    private static boolean isAllowedOnCurrentOs(JsonObject library) throws IOException {
        if (!library.has("rules") || !library.get("rules").isJsonArray()) {
            return true;
        }
        String os = osName();
        boolean allowed = false;
        for (JsonElement element : library.getAsJsonArray("rules")) {
            if (!element.isJsonObject()) {
                throw new IOException("Loader metadata contains an invalid library rule.");
            }
            JsonObject rule = element.getAsJsonObject();
            JsonObject osRule = rule.has("os") && rule.get("os").isJsonObject()
                    ? rule.getAsJsonObject("os") : null;
            boolean matches = osRule == null || !osRule.has("name")
                    || os.equals(osRule.get("name").getAsString());
            if (matches) {
                allowed = "allow".equals(rule.has("action") ? rule.get("action").getAsString() : "");
            }
        }
        return allowed;
    }

    private static String osName() {
        String name = System.getProperty("os.name", "").toLowerCase(Locale.ROOT);
        if (name.contains("win")) {
            return "windows";
        }
        if (name.contains("mac") || name.contains("darwin")) {
            return "osx";
        }
        return "linux";
    }

    private static String safeSegment(String value) throws IOException {
        if (value == null || !SAFE_VERSION.matcher(value).matches()) {
            throw new IOException("Invalid " + value + " loader version identifier.");
        }
        return value;
    }

    private static String requiredString(JsonObject object, String key) throws IOException {
        if (!object.has(key) || !object.get(key).isJsonPrimitive()
                || !object.getAsJsonPrimitive(key).isString()
                || object.get(key).getAsString().isBlank()) {
            throw new IOException("Loader profile is missing " + key + ".");
        }
        return object.get(key).getAsString();
    }

    private static Path safeResolve(Path root, String relativePath) throws IOException {
        Path normalizedRoot = root.toAbsolutePath().normalize();
        Path result = normalizedRoot.resolve(relativePath).normalize();
        if (!result.startsWith(normalizedRoot)) {
            throw new IOException("Loader metadata contains an unsafe library path.");
        }
        return result;
    }

    private record MavenArtifact(String relativePath) {
        static MavenArtifact parse(String coordinates) throws IOException {
            String[] parts = coordinates.split(":", 4);
            if (parts.length < 3 || !parts[0].matches("[A-Za-z0-9_.-]+")
                    || !parts[1].matches("[A-Za-z0-9_.+-]+")
                    || !parts[2].matches("[A-Za-z0-9_.+-]+")) {
                throw new IOException("Loader metadata contains invalid Maven coordinates.");
            }
            String classifier = parts.length == 4 ? parts[3] : "";
            String extension = "jar";
            int extensionSeparator = classifier.indexOf('@');
            if (extensionSeparator >= 0) {
                extension = classifier.substring(extensionSeparator + 1);
                classifier = classifier.substring(0, extensionSeparator);
            }
            if (!classifier.isEmpty() && !classifier.matches("[A-Za-z0-9_.+-]+")
                    || !extension.matches("[A-Za-z0-9_.+-]+")) {
                throw new IOException("Loader metadata contains invalid Maven artifact details.");
            }
            String groupPath = String.join("/", List.of(parts[0].split("\\.")));
            String filename = parts[1] + "-" + parts[2]
                    + (classifier.isEmpty() ? "" : "-" + classifier) + "." + extension;
            return new MavenArtifact(groupPath + "/" + parts[1] + "/" + parts[2] + "/" + filename);
        }
    }
}
