package net.stratalauncher.core.runtime;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.google.gson.JsonParseException;
import net.stratalauncher.core.auth.LauncherSettings;
import net.stratalauncher.core.download.DownloadManager;
import org.apache.commons.compress.archivers.tar.TarArchiveEntry;
import org.apache.commons.compress.archivers.tar.TarArchiveInputStream;
import org.apache.commons.compress.compressors.gzip.GzipCompressorInputStream;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.time.Duration;
import java.util.HexFormat;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.UUID;
import java.util.function.Consumer;
import java.util.regex.Pattern;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

final class ExternalJavaRuntimeInstaller {
    private static final String JAVA_PATH_FILE = ".stratalauncher-java-path";
    private static final String VERSION_FILE = ".stratalauncher-runtime-version";
    private static final String VENDOR_FILE = ".stratalauncher-runtime-vendor";
    private static final long MAX_ARCHIVE_BYTES = 1_000_000_000L;
    private static final long MAX_EXTRACTED_BYTES = 2_000_000_000L;
    private static final Pattern SAFE_COMPONENT = Pattern.compile("[a-z0-9._+-]+");
    private static final HttpClient HTTP = HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(15))
            .followRedirects(HttpClient.Redirect.NORMAL).build();

    private ExternalJavaRuntimeInstaller() {
    }

    static void install(Path runtimeDirectory, int majorVersion, LauncherSettings.RuntimeVendor vendor,
                       Consumer<DownloadManager.Progress> progress) throws IOException, InterruptedException {
        if (Files.isSymbolicLink(runtimeDirectory)) {
            throw new IOException("Java runtimes cannot be installed through symbolic links.");
        }
        RuntimePackage runtime = vendor == LauncherSettings.RuntimeVendor.TEMURIN
                ? temurinPackage(majorVersion) : zuluPackage(majorVersion);
        try {
            if (isAlreadyInstalled(runtimeDirectory, runtime)) {
                report(progress, 1, 1, vendor.label() + " Java " + runtime.version() + " is already installed");
                return;
            }
        } catch (IOException exception) {
            report(progress, 0, 1, "Repairing an invalid managed Java runtime");
        }
        Path parent = runtimeDirectory.getParent();
        if (parent == null || Files.isSymbolicLink(parent) || Files.isSymbolicLink(runtimeDirectory)) {
            throw new IOException("Java runtimes cannot be installed through symbolic links.");
        }
        Files.createDirectories(parent);
        Path staging = Files.createDirectory(parent.resolve(".staging-" + UUID.randomUUID()));
        Path archive = Files.createTempFile(parent, ".runtime-", ".archive");
        try {
            report(progress, 0, 1, "Downloading " + vendor.label() + " Java " + runtime.version());
            download(runtime, archive, progress);
            if (runtime.zip()) {
                extractZip(archive, staging);
            } else {
                extractTarGz(archive, staging);
            }
            Path executable = findExecutable(staging);
            markExecutable(executable);
            String relative = staging.toAbsolutePath().normalize()
                    .relativize(executable.toAbsolutePath().normalize()).toString();
            Files.writeString(resolveInside(staging, JAVA_PATH_FILE),
                    relative + "\n" + sha1(executable) + "\n");
            Files.writeString(resolveInside(staging, VERSION_FILE), runtime.version());
            Files.writeString(resolveInside(staging, VENDOR_FILE), vendor.name());
            JavaRuntimeManager.replaceRuntime(staging, runtimeDirectory);
            report(progress, 1, 1, "Installed " + vendor.label() + " Java " + runtime.version());
        } finally {
            Files.deleteIfExists(archive);
            if (Files.exists(staging)) {
                JavaRuntimeManager.deleteRuntimeTree(staging);
            }
        }
    }

    private static RuntimePackage temurinPackage(int major) throws IOException, InterruptedException {
        PlatformInfo platform = platformInfo();
        URI api = URI.create("https://api.adoptium.net/v3/assets/latest/" + major + "/hotspot?architecture="
                + platform.architecture() + "&heap_size=normal&image_type=jre&jvm_impl=hotspot&os="
                + platform.temurinOs() + "&vendor=eclipse");
        JsonArray assets = fetchArray(api, Set.of("api.adoptium.net"));
        if (assets.isEmpty()) {
            throw new IOException("Eclipse Temurin has no compatible Java " + major + " runtime for this device.");
        }
        JsonObject asset = object(assets.get(0), "binary");
        JsonObject pkg = object(asset, "package");
        URI download = validatedUri(string(pkg, "link"), Set.of("github.com"));
        String version = string(object(assets.get(0), "version"), "openjdk_version");
        String checksum = string(pkg, "checksum");
        if (!checksum.matches("[0-9a-fA-F]{64}")) {
            throw new IOException("Eclipse Temurin provided an invalid SHA-256 checksum.");
        }
        return new RuntimePackage(download, checksum, version, packageIsZip(pkg));
    }

    private static RuntimePackage zuluPackage(int major) throws IOException, InterruptedException {
        PlatformInfo platform = platformInfo();
        boolean windows = isWindows();
        String archive = windows ? "zip" : "tar.gz";
        URI api = URI.create("https://api.azul.com/metadata/v1/zulu/packages/?java_version=" + major
                + "&os=" + platform.zuluOs() + "&arch=" + platform.zuluArchitecture()
                + "&hw_bitness=" + platform.bits() + "&archive_type=" + archive
                + "&java_package_type=jre&release_status=ga&availability_types=CA&latest=true");
        JsonArray packages = fetchArray(api, Set.of("api.azul.com"));
        if (packages.isEmpty()) {
            throw new IOException("Azul Zulu has no compatible Java " + major + " runtime for this device.");
        }
        JsonObject pkg = packages.get(0).getAsJsonObject();
        URI download = validatedUri(string(pkg, "download_url"), Set.of("cdn.azul.com"));
        String version = pkg.has("java_version") ? versionString(pkg.getAsJsonArray("java_version"))
                : string(pkg, "name");
        return new RuntimePackage(download, "", version, windows);
    }

    private static JsonArray fetchArray(URI uri, Set<String> hosts) throws IOException, InterruptedException {
        if (!"https".equalsIgnoreCase(uri.getScheme()) || !hosts.contains(uri.getHost())) {
            throw new IOException("Java runtime metadata must come from an approved HTTPS provider.");
        }
        HttpRequest request = HttpRequest.newBuilder(uri).timeout(Duration.ofSeconds(45))
                .header("Accept", "application/json")
                .header("User-Agent", "StrataLauncher/0.1.0")
                .GET().build();
        HttpResponse<String> response = HTTP.send(request, HttpResponse.BodyHandlers.ofString());
        if (response.statusCode() < 200 || response.statusCode() >= 300) {
            throw new IOException("Java runtime provider returned HTTP " + response.statusCode() + ".");
        }
        try {
            JsonElement parsed = JsonParser.parseString(response.body());
            if (!parsed.isJsonArray()) {
                throw new IOException("Java runtime provider returned an unexpected response.");
            }
            return parsed.getAsJsonArray();
        } catch (JsonParseException | IllegalStateException exception) {
            throw new IOException("Could not parse Java runtime provider metadata.", exception);
        }
    }

    private static void download(RuntimePackage runtime, Path destination,
                                 Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        HttpRequest request = HttpRequest.newBuilder(runtime.uri()).timeout(Duration.ofMinutes(10))
                .header("User-Agent", "StrataLauncher/0.1.0")
                .GET().build();
        HttpResponse<InputStream> response = HTTP.send(request, HttpResponse.BodyHandlers.ofInputStream());
        URI resolved = response.uri();
        if (!"https".equalsIgnoreCase(resolved.getScheme())
                || !(resolved.getHost().equals("release-assets.githubusercontent.com")
                || resolved.getHost().equals("github.com") || resolved.getHost().equals("cdn.azul.com"))) {
            response.body().close();
            throw new IOException("Java runtime download redirected to an unapproved host.");
        }
        if (response.statusCode() < 200 || response.statusCode() >= 300) {
            response.body().close();
            throw new IOException("Java runtime download failed with HTTP " + response.statusCode() + ".");
        }
        long length = 0;
        try (InputStream input = response.body(); OutputStream output = Files.newOutputStream(destination)) {
            byte[] buffer = new byte[64 * 1024];
            int read;
            while ((read = input.read(buffer)) != -1) {
                length += read;
                if (length > MAX_ARCHIVE_BYTES) {
                    throw new IOException("Java runtime archive exceeds the 1 GB safety limit.");
                }
                output.write(buffer, 0, read);
            }
        }
        if (!runtime.sha256().isBlank() && !runtime.sha256().equalsIgnoreCase(sha256(destination))) {
            throw new IOException("Java runtime archive failed its SHA-256 integrity check.");
        }
        report(progress, 0, 1, "Downloaded " + length + " bytes");
    }

    private static void extractZip(Path archive, Path root) throws IOException {
        long extracted = 0;
        int entries = 0;
        try (ZipInputStream input = new ZipInputStream(Files.newInputStream(archive))) {
            ZipEntry entry;
            while ((entry = input.getNextEntry()) != null) {
                if (++entries > 200_000) {
                    throw new IOException("Java runtime archive contains too many files.");
                }
                Path target = resolveArchivePath(root, entry.getName());
                if (entry.isDirectory()) {
                    Files.createDirectories(target);
                    continue;
                }
                Files.createDirectories(target.getParent());
                try (OutputStream output = Files.newOutputStream(target)) {
                    byte[] buffer = new byte[64 * 1024];
                    int read;
                    while ((read = input.read(buffer)) != -1) {
                        extracted += read;
                        if (extracted > MAX_EXTRACTED_BYTES) {
                            throw new IOException("Extracted Java runtime exceeds the 2 GB safety limit.");
                        }
                        output.write(buffer, 0, read);
                    }
                }
            }
        }
    }

    private static void extractTarGz(Path archive, Path root) throws IOException {
        long extracted = 0;
        int entries = 0;
        try (TarArchiveInputStream input = new TarArchiveInputStream(
                new GzipCompressorInputStream(Files.newInputStream(archive)))) {
            TarArchiveEntry entry;
            while ((entry = input.getNextEntry()) != null) {
                if (++entries > 200_000) {
                    throw new IOException("Java runtime archive contains too many files.");
                }
                Path target = resolveArchivePath(root, entry.getName());
                if (entry.isDirectory()) {
                    Files.createDirectories(target);
                } else if (entry.isSymbolicLink()) {
                    safeLinkTarget(entry.getName(), entry.getLinkName());
                    Files.createDirectories(target.getParent());
                    Files.createSymbolicLink(target, Path.of(entry.getLinkName()));
                } else if (entry.isFile()) {
                    Files.createDirectories(target.getParent());
                    try (OutputStream output = Files.newOutputStream(target)) {
                        byte[] buffer = new byte[64 * 1024];
                        int read;
                        while ((read = input.read(buffer)) != -1) {
                            extracted += read;
                            if (extracted > MAX_EXTRACTED_BYTES) {
                                throw new IOException("Extracted Java runtime exceeds the 2 GB safety limit.");
                            }
                            output.write(buffer, 0, read);
                        }
                    }
                } else {
                    throw new IOException("Java runtime archive contains an unsupported file type.");
                }
            }
        }
    }

    private static Path findExecutable(Path root) throws IOException {
        String target = isWindows() ? "java.exe" : "java";
        try (var files = Files.walk(root)) {
            return files.filter(file -> file.getFileName().toString().equals(target))
                    .filter(file -> file.getParent() != null
                            && file.getParent().getFileName().toString().equals("bin"))
                    .findFirst().orElseThrow(() -> new IOException("Java runtime archive contains no executable."));
        }
    }

    private static void safeLinkTarget(String path, String target) throws IOException {
        if (target.isBlank() || target.indexOf('\\') >= 0 || Path.of(target).isAbsolute()
                || target.matches("^[A-Za-z]:.*")) {
            throw new IOException("Java runtime archive contains an unsafe symbolic link.");
        }
        Path link = Path.of(path);
        Path resolved = link.getParent() == null ? Path.of(target).normalize()
                : link.getParent().resolve(target).normalize();
        if (resolved.isAbsolute() || resolved.startsWith("..")) {
            throw new IOException("Java runtime archive symbolic link escapes its installation folder.");
        }
    }

    private static Path resolveInside(Path root, String relative) throws IOException {
        if (relative == null || relative.isBlank() || relative.indexOf('\\') >= 0
                || relative.startsWith("/") || relative.matches("^[A-Za-z]:.*")) {
            throw new IOException("Java runtime archive contains an unsafe file path.");
        }
        Path target = root.toAbsolutePath().normalize().resolve(relative).normalize();
        if (!target.startsWith(root.toAbsolutePath().normalize())) {
            throw new IOException("Java runtime archive path escapes its installation folder.");
        }
        return target;
    }

    private static Path resolveArchivePath(Path root, String relative) throws IOException {
        Path target = resolveInside(root, relative);
        Path current = root.toAbsolutePath().normalize();
        Path partial = current;
        for (Path component : current.relativize(target)) {
            partial = partial.resolve(component);
            if (Files.exists(partial) && Files.isSymbolicLink(partial)) {
                throw new IOException("Java runtime archive attempts to write through a symbolic link.");
            }
        }
        return target;
    }

    private static boolean isAlreadyInstalled(Path directory, RuntimePackage expected) throws IOException {
        Path marker = directory.resolve(JAVA_PATH_FILE);
        Path version = directory.resolve(VERSION_FILE);
        if (!Files.isRegularFile(marker) || !Files.isRegularFile(version)
                || !Files.readString(version).trim().equals(expected.version())) {
            return false;
        }
        List<String> lines = Files.readAllLines(marker);
        if (lines.size() != 2 || !lines.get(1).matches("[0-9a-f]{40}")) {
            return false;
        }
        Path executable = resolveInside(directory, lines.getFirst());
        return Files.isRegularFile(executable) && executable.toRealPath().startsWith(directory.toRealPath())
                && (isWindows() || Files.isExecutable(executable))
                && lines.get(1).equals(sha1(executable));
    }

    private static void markExecutable(Path executable) throws IOException {
        if (isWindows()) {
            return;
        }
        if (!executable.toFile().setExecutable(true, false) && !Files.isExecutable(executable)) {
            throw new IOException("Could not mark the managed Java runtime executable.");
        }
    }

    private static URI validatedUri(String value, Set<String> hosts) throws IOException {
        URI uri;
        try {
            uri = URI.create(value);
        } catch (IllegalArgumentException exception) {
            throw new IOException("Java runtime provider returned an invalid download URL.", exception);
        }
        if (!"https".equalsIgnoreCase(uri.getScheme()) || !hosts.contains(uri.getHost())) {
            throw new IOException("Java runtime provider returned a URL outside its official download hosts.");
        }
        return uri;
    }

    private static String string(JsonObject object, String key) throws IOException {
        JsonElement element = object == null ? null : object.get(key);
        if (element == null || !element.isJsonPrimitive()) {
            throw new IOException("Java runtime metadata is missing " + key + ".");
        }
        return element.getAsString();
    }

    private static JsonObject object(JsonElement element, String key) throws IOException {
        JsonElement nested = element == null || !element.isJsonObject() ? null : element.getAsJsonObject().get(key);
        if (nested == null || !nested.isJsonObject()) {
            throw new IOException("Java runtime metadata is missing " + key + ".");
        }
        return nested.getAsJsonObject();
    }

    private static boolean packageIsZip(JsonObject pkg) throws IOException {
        String name = string(pkg, "name").toLowerCase(Locale.ROOT);
        return name.endsWith(".zip");
    }

    private static String versionString(JsonArray version) {
        List<String> parts = new java.util.ArrayList<>();
        for (JsonElement element : version) {
            parts.add(element.getAsString());
        }
        while (parts.size() > 3 && parts.getLast().equals("0")) {
            parts.removeLast();
        }
        return String.join(".", parts);
    }

    private static String sha1(Path file) throws IOException {
        return digest(file, "SHA-1");
    }

    private static String sha256(Path file) throws IOException {
        return digest(file, "SHA-256");
    }

    private static String digest(Path file, String algorithm) throws IOException {
        try {
            var digest = java.security.MessageDigest.getInstance(algorithm);
            try (InputStream input = Files.newInputStream(file)) {
                byte[] buffer = new byte[64 * 1024];
                int read;
                while ((read = input.read(buffer)) != -1) {
                    digest.update(buffer, 0, read);
                }
            }
            return HexFormat.of().formatHex(digest.digest());
        } catch (java.security.NoSuchAlgorithmException exception) {
            throw new IOException(algorithm + " is not available.", exception);
        }
    }

    private static PlatformInfo platformInfo() throws IOException {
        String os = System.getProperty("os.name", "").toLowerCase(Locale.ROOT);
        String arch = System.getProperty("os.arch", "").toLowerCase(Locale.ROOT);
        boolean arm = arch.equals("aarch64") || arch.equals("arm64");
        boolean x64 = arch.equals("amd64") || arch.equals("x86_64") || arch.equals("x64");
        if (!arm && !x64) {
            throw new IOException("Temurin and Zulu runtime installs support 64-bit ARM or x64 systems.");
        }
        String architecture = arm ? "aarch64" : "x64";
        String zuluArchitecture = arm ? "aarch64" : "x86";
        int bits = 64;
        if (os.contains("linux")) {
            return new PlatformInfo("linux", "linux", architecture, zuluArchitecture, bits);
        }
        if (os.contains("win")) {
            return new PlatformInfo("windows", "windows", architecture, zuluArchitecture, bits);
        }
        if (os.contains("mac") || os.contains("darwin")) {
            return new PlatformInfo("mac", "macos", architecture, zuluArchitecture, bits);
        }
        throw new IOException("Temurin and Zulu runtimes are not available for this operating system.");
    }

    private static boolean isWindows() {
        return System.getProperty("os.name", "").toLowerCase(Locale.ROOT).contains("win");
    }

    private static void report(Consumer<DownloadManager.Progress> progress, long completed, long total, String item) {
        if (progress != null) {
            int percent = total <= 0 ? 0 : (int) Math.min(100, completed * 100.0 / total);
            progress.accept(new DownloadManager.Progress(percent, 100, item));
        }
    }

    private record RuntimePackage(URI uri, String sha256, String version, boolean zip) {
    }

    private record PlatformInfo(String temurinOs, String zuluOs, String architecture,
                                String zuluArchitecture, int bits) {
    }
}
