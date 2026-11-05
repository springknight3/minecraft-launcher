package net.stratalauncher.core.loader;

import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.google.gson.JsonParseException;
import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.download.DownloadTask;
import net.stratalauncher.core.instance.Instance;

import java.io.IOException;
import java.net.URI;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;
import java.util.regex.Pattern;

public final class ForgeInstaller {
    private static final URI MAVEN_ROOT = URI.create("https://maven.minecraftforge.net/");
    private static final URI METADATA_URI = MAVEN_ROOT.resolve("net/minecraftforge/forge/maven-metadata.xml");
    private static final Pattern SAFE_VERSION = Pattern.compile("[A-Za-z0-9._+~-]+");

    private final LoaderMetadataClient metadataClient;
    private final DownloadManager downloadManager;
    private final Path installerCache;

    ForgeInstaller(LoaderMetadataClient metadataClient, DownloadManager downloadManager,
                   Path installerCache) {
        this.metadataClient = metadataClient;
        this.downloadManager = downloadManager;
        this.installerCache = installerCache;
    }

    public List<String> versions(String minecraftVersion) throws IOException, InterruptedException {
        String prefix = safeMinecraftVersion(minecraftVersion) + "-";
        return MavenVersions.parse(metadataClient.getText(METADATA_URI), prefix).stream()
                .map(version -> version.substring(prefix.length()))
                .toList();
    }

    LoaderProfile install(Instance instance, String minecraftVersion, String forgeVersion,
                          Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        safeMinecraftVersion(minecraftVersion);
        safeLoaderVersion(forgeVersion);
        String artifact = "net/minecraftforge/forge/" + minecraftVersion + "-" + forgeVersion
                + "/forge-" + minecraftVersion + "-" + forgeVersion + "-installer.jar";
        URI installerUri = MAVEN_ROOT.resolve(artifact);
        Path installerFile = installerCache.resolve("forge").resolve(minecraftVersion + "-" + forgeVersion
                + "-installer.jar");
        downloadInstaller(installerUri, installerFile, "Forge installer " + forgeVersion, progress);

        JsonObject installedProfile = runInstaller(instance, minecraftVersion, forgeVersion,
                installerFile, "--installClient");
        return new LoaderProfile(LoaderType.FORGE, minecraftVersion, forgeVersion,
                requiredProfileId(installedProfile), installedProfile);
    }

    private void downloadInstaller(URI source, Path destination, String description,
                                   Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        String checksum = metadataClient.getText(URI.create(source + ".sha1")).trim();
        if (!checksum.matches("(?i)[0-9a-f]{40}")) {
            throw new IOException("Forge installer did not provide a valid SHA-1 checksum.");
        }
        downloadManager.downloadAll(List.of(new DownloadTask(source, destination, checksum, description)), progress);
    }

    JsonObject runInstaller(Instance instance, String minecraftVersion, String loaderVersion,
                            Path installerFile, String installArgument)
            throws IOException, InterruptedException {
        Path logFile = instance.gameDirectory().resolve("forge-installer.log");
        Files.createDirectories(logFile.getParent());
        Path javaExecutable = Path.of(System.getProperty("java.home"), "bin",
                System.getProperty("os.name", "").toLowerCase(Locale.ROOT).contains("win")
                        ? "java.exe" : "java");
        Process process = new ProcessBuilder(javaExecutable.toString(), "-jar", installerFile.toString(),
                installArgument, instance.gameDirectory().toAbsolutePath().normalize().toString())
                .directory(instance.gameDirectory().toFile())
                .redirectErrorStream(true)
                .redirectOutput(logFile.toFile())
                .start();
        try {
            if (!process.waitFor(15, TimeUnit.MINUTES)) {
                process.destroyForcibly();
                throw new IOException("Forge installation timed out. See " + logFile);
            }
        } catch (InterruptedException exception) {
            process.destroyForcibly();
            Thread.currentThread().interrupt();
            throw exception;
        }
        if (process.exitValue() != 0) {
            throw new IOException("Forge installer failed with exit code " + process.exitValue()
                    + ". See " + logFile);
        }
        return findInstalledProfile(instance, minecraftVersion, loaderVersion, "forge");
    }

    static JsonObject findInstalledProfile(Instance instance, String minecraftVersion,
                                           String loaderVersion, String loaderMarker) throws IOException {
        Path versionsDirectory = instance.gameDirectory().resolve("versions");
        if (!Files.isDirectory(versionsDirectory)) {
            throw new IOException("Loader installer did not create a Minecraft version profile.");
        }
        try (var versionDirectories = Files.list(versionsDirectory)) {
            for (Path versionDirectory : versionDirectories.filter(Files::isDirectory).toList()) {
                try (var files = Files.list(versionDirectory)) {
                    for (Path file : files.filter(path -> path.getFileName().toString().endsWith(".json")).toList()) {
                        JsonObject profile;
                        try {
                            profile = JsonParser.parseString(Files.readString(file)).getAsJsonObject();
                        } catch (JsonParseException | IllegalStateException exception) {
                            continue;
                        }
                        String id = profile.has("id") ? profile.get("id").getAsString() : "";
                        if (id.equals(minecraftVersion) || !id.toLowerCase(Locale.ROOT)
                                .contains(loaderVersion.toLowerCase(Locale.ROOT))) {
                            continue;
                        }
                        if (!profile.has("inheritsFrom")
                                || !minecraftVersion.equals(profile.get("inheritsFrom").getAsString())
                                || !profile.has("mainClass") || !profile.has("libraries")
                                || !profile.get("libraries").isJsonArray()) {
                            continue;
                        }
                        if (!id.toLowerCase(Locale.ROOT).contains(loaderMarker)) {
                            continue;
                        }
                        return profile;
                    }
                }
            }
        }
        String installerName = loaderMarker.equals("neoforge") ? "neoforge" : "forge";
        throw new IOException(installerName + " installer completed but did not create a usable launch profile. See "
                + instance.gameDirectory().resolve(installerName + "-installer.log"));
    }

    private static String requiredProfileId(JsonObject profile) throws IOException {
        if (!profile.has("id") || profile.get("id").getAsString().isBlank()) {
            throw new IOException("Forge installer returned a profile without an identifier.");
        }
        return profile.get("id").getAsString();
    }

    private static String safeMinecraftVersion(String value) throws IOException {
        return safeVersion(value, "Minecraft");
    }

    private static String safeLoaderVersion(String value) throws IOException {
        return safeVersion(value, "Forge");
    }

    private static String safeVersion(String value, String label) throws IOException {
        if (value == null || !SAFE_VERSION.matcher(value).matches()) {
            throw new IOException("Invalid " + label + " version identifier.");
        }
        return value;
    }
}
