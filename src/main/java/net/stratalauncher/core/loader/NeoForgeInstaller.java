package net.stratalauncher.core.loader;

import com.google.gson.JsonObject;
import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.download.DownloadTask;
import net.stratalauncher.core.instance.Instance;

import java.io.IOException;
import java.net.URI;
import java.nio.file.Path;
import java.util.List;
import java.util.regex.Pattern;
import java.util.function.Consumer;

public final class NeoForgeInstaller {
    private static final URI MAVEN_ROOT = URI.create("https://maven.neoforged.net/releases/");
    private static final URI METADATA_URI = MAVEN_ROOT.resolve("net/neoforged/neoforge/maven-metadata.xml");
    private static final Pattern SAFE_VERSION = Pattern.compile("[A-Za-z0-9._+~-]+");

    private final LoaderMetadataClient metadataClient;
    private final DownloadManager downloadManager;
    private final Path installerCache;

    NeoForgeInstaller(LoaderMetadataClient metadataClient, DownloadManager downloadManager,
                      Path installerCache) {
        this.metadataClient = metadataClient;
        this.downloadManager = downloadManager;
        this.installerCache = installerCache;
    }

    public List<String> versions(String minecraftVersion) throws IOException, InterruptedException {
        String prefix = compatibilityPrefix(minecraftVersion);
        if (prefix.isEmpty()) {
            return List.of();
        }
        return MavenVersions.parse(metadataClient.getText(METADATA_URI), prefix);
    }

    LoaderProfile install(Instance instance, String minecraftVersion, String neoForgeVersion,
                          Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        safeVersion(neoForgeVersion);
        String artifact = "net/neoforged/neoforge/" + neoForgeVersion
                + "/neoforge-" + neoForgeVersion + "-installer.jar";
        URI installerUri = MAVEN_ROOT.resolve(artifact);
        Path installerFile = installerCache.resolve("neoforge").resolve(neoForgeVersion + "-installer.jar");
        String checksum = metadataClient.getText(URI.create(installerUri + ".sha1")).trim();
        if (!checksum.matches("(?i)[0-9a-f]{40}")) {
            throw new IOException("NeoForge installer did not provide a valid SHA-1 checksum.");
        }
        downloadManager.downloadAll(List.of(new DownloadTask(installerUri, installerFile,
                checksum, "NeoForge installer " + neoForgeVersion)), progress);
        Path logFile = instance.gameDirectory().resolve("neoforge-installer.log");
        java.nio.file.Files.createDirectories(logFile.getParent());
        Path javaExecutable = Path.of(System.getProperty("java.home"), "bin",
                System.getProperty("os.name", "").toLowerCase(java.util.Locale.ROOT).contains("win")
                        ? "java.exe" : "java");
        Process process = new ProcessBuilder(javaExecutable.toString(), "-jar", installerFile.toString(),
                "--install-client", instance.gameDirectory().toAbsolutePath().normalize().toString())
                .directory(instance.gameDirectory().toFile())
                .redirectErrorStream(true)
                .redirectOutput(logFile.toFile())
                .start();
        try {
            if (!process.waitFor(15, java.util.concurrent.TimeUnit.MINUTES)) {
                process.destroyForcibly();
                throw new IOException("NeoForge installation timed out. See " + logFile);
            }
        } catch (InterruptedException exception) {
            process.destroyForcibly();
            Thread.currentThread().interrupt();
            throw exception;
        }
        if (process.exitValue() != 0) {
            throw new IOException("NeoForge installer failed with exit code " + process.exitValue()
                    + ". See " + logFile);
        }
        JsonObject profile = ForgeInstaller.findInstalledProfile(instance, minecraftVersion,
                neoForgeVersion, "neoforge");
        return new LoaderProfile(LoaderType.NEOFORGE, minecraftVersion, neoForgeVersion,
                profile.get("id").getAsString(), profile);
    }

    static String compatibilityPrefix(String minecraftVersion) {
        if (minecraftVersion == null || !minecraftVersion.matches("[0-9]+\\.[0-9]+(?:\\.[0-9]+)?")) {
            return "";
        }
        String[] parts = minecraftVersion.split("\\.");
        int major = Integer.parseInt(parts[0]);
        int minor = Integer.parseInt(parts[1]);
        if (major == 1) {
            if (minor < 20) {
                return "";
            }
            String patch = parts.length > 2 ? parts[2] : "0";
            return minor + "." + patch + ".";
        }
        return major + "." + minor + ".";
    }

    private static void safeVersion(String value) throws IOException {
        if (value == null || !SAFE_VERSION.matcher(value).matches()) {
            throw new IOException("Invalid NeoForge version identifier.");
        }
    }
}
