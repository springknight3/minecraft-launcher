package net.stratalauncher.core.runtime;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import com.google.gson.JsonParser;
import net.stratalauncher.core.auth.LauncherSettings;
import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.download.DownloadTask;
import net.stratalauncher.core.instance.Instance;

import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.nio.file.attribute.PosixFilePermission;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.time.Duration;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.HexFormat;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.UUID;
import java.util.function.Consumer;
import java.util.regex.Pattern;
import org.apache.commons.compress.archivers.tar.TarArchiveEntry;
import org.apache.commons.compress.archivers.tar.TarArchiveInputStream;
import org.apache.commons.compress.compressors.gzip.GzipCompressorInputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public final class JavaRuntimeManager {
    public static final URI RUNTIME_INDEX_URI = URI.create(
            "https://piston-meta.mojang.com/v1/products/java-runtime/"
                    + "2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json");

    private static final Pattern SAFE_COMPONENT = Pattern.compile("[a-z0-9-]+");
    private static final Pattern SAFE_RUNTIME_VERSION = Pattern.compile("[A-Za-z0-9._+-]+");
    private static final String JAVA_PATH_FILE = ".stratalauncher-java-path";
    private static final String RUNTIME_VERSION_FILE = ".stratalauncher-runtime-version";
    private static final String RUNTIME_VENDOR_FILE = ".stratalauncher-runtime-vendor";
    private static final int MAX_MANIFEST_BYTES = 16 * 1024 * 1024;
    private static final long MAX_ARCHIVE_BYTES = 1_000_000_000L;
    private static final long MAX_EXTRACTED_BYTES = 2_000_000_000L;
    private final DownloadManager downloadManager;
    private final HttpClient httpClient;
    private final Path dataDirectory;
    private final LauncherSettings launcherSettings;

    public JavaRuntimeManager(DownloadManager downloadManager) {
        this(downloadManager, null, null);
    }

    public JavaRuntimeManager(DownloadManager downloadManager, Path dataDirectory,
                              LauncherSettings launcherSettings) {
        this.downloadManager = Objects.requireNonNull(downloadManager);
        this.dataDirectory = dataDirectory == null ? null : dataDirectory.toAbsolutePath().normalize();
        this.launcherSettings = launcherSettings;
        httpClient = HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(15))
                .followRedirects(HttpClient.Redirect.NORMAL).build();
    }

    public String install(Instance instance, JsonObject versionMetadata, Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        return install(instance.directory(), instance.versionId(), versionMetadata, progress);
    }

    public String install(Path ownerDirectory, String versionId, JsonObject versionMetadata,
                          Consumer<DownloadManager.Progress> progress) throws IOException, InterruptedException {
        LauncherSettings.RuntimeVendor vendor = launcherSettings == null
                ? LauncherSettings.RuntimeVendor.MOJANG : launcherSettings.runtimeVendor();
        boolean redundant = launcherSettings != null && launcherSettings.redundantJavaRuntimes();
        return install(ownerDirectory, versionId, versionMetadata, vendor, redundant, progress);
    }

    public String install(Path ownerDirectory, String versionId, JsonObject versionMetadata,
                          LauncherSettings.RuntimeVendor vendor, boolean redundant,
                          Consumer<DownloadManager.Progress> progress) throws IOException, InterruptedException {
        Objects.requireNonNull(vendor);
        if (vendor != LauncherSettings.RuntimeVendor.MOJANG) {
            return installExternal(ownerDirectory, versionId, versionMetadata, vendor, redundant, progress);
        }
        return installMojang(ownerDirectory, versionId, versionMetadata, redundant, progress);
    }

    private String installMojang(Path ownerDirectory, String versionId, JsonObject versionMetadata,
                                 boolean redundant, Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        String component = requiredComponent(versionMetadata, versionId);
        URI indexUri = RUNTIME_INDEX_URI;
        JsonObject runtimeIndex = fetchJson(indexUri);
        String platform = currentPlatform();
        RuntimeManifestInfo runtime = selectRuntime(runtimeIndex, platform, component);
        Path runtimeDirectory = runtimeDirectory(ownerDirectory, component, redundant);
        InstalledExecutable installedExecutable;
        try {
            installedExecutable = readInstalledExecutable(runtimeDirectory);
        } catch (IOException exception) {
            installedExecutable = null;
            if (progress != null) {
                progress.accept(new DownloadManager.Progress(0, 1,
                        "Repairing invalid managed Java runtime metadata: " + exception.getMessage()));
            }
        }
        if (installedExecutable != null && Files.isRegularFile(installedExecutable.path())
                && (isWindows() || Files.isExecutable(installedExecutable.path()))
                && Files.isRegularFile(runtimeDirectory.resolve(RUNTIME_VERSION_FILE))
                && Files.readString(runtimeDirectory.resolve(RUNTIME_VERSION_FILE)).trim()
                .equals(runtime.version())
                && installedExecutable.sha1().equals(sha1(installedExecutable.path()))) {
            if (progress != null) {
                progress.accept(new DownloadManager.Progress(1, 1,
                        "Java " + runtime.version() + " is already installed"));
            }
            return component;
        }

        JsonObject runtimeManifest = fetchJson(runtime.manifestUri(), runtime.manifestSha1());
        List<RuntimeFile> files = parseRuntimeFiles(runtimeManifest);
        String executable = findJavaExecutable(files, platform);
        Path ownerRoot = ownerDirectory.toAbsolutePath().normalize();
        Path runtimesRoot = runtimeDirectory.getParent();
        if (Files.isSymbolicLink(ownerRoot) || Files.isSymbolicLink(runtimesRoot)) {
            throw new IOException("Java runtimes cannot be installed through a symbolic link.");
        }
        Files.createDirectories(runtimesRoot);
        Path staging = Files.createDirectory(runtimesRoot.resolve(".staging-" + UUID.randomUUID()));
        try {
            List<DownloadTask> downloads = new ArrayList<>();
            for (RuntimeFile file : files) {
                if (file.type() == RuntimeFileType.DIRECTORY) {
                    Files.createDirectories(resolveInside(staging, file.path()));
                } else if (file.type() == RuntimeFileType.FILE) {
                    Path destination = resolveInside(staging, file.path());
                    downloads.add(new DownloadTask(file.source(), destination, file.sha1(),
                            "Java " + runtime.version() + " " + file.path()));
                }
            }
            downloadManager.downloadAll(downloads, progress);
            for (RuntimeFile file : files) {
                if (file.type() == RuntimeFileType.LINK) {
                    createLink(staging, file.path(), file.target());
                } else if (file.type() == RuntimeFileType.FILE && file.executable()) {
                    markExecutable(resolveInside(staging, file.path()));
                }
            }
            Path javaPathFile = resolveInside(staging, JAVA_PATH_FILE);
            Files.writeString(javaPathFile, executable + "\n"
                    + sha1(resolveInside(staging, executable)) + "\n");
            Files.writeString(resolveInside(staging, RUNTIME_VERSION_FILE), runtime.version());
            Files.writeString(resolveInside(staging, RUNTIME_VENDOR_FILE), LauncherSettings.RuntimeVendor.MOJANG.name());

            replaceRuntime(staging, runtimeDirectory);
            if (progress != null) {
                progress.accept(new DownloadManager.Progress(1, 1,
                        "Installed Java " + runtime.version() + " for Minecraft " + versionId));
            }
        } finally {
            if (Files.exists(staging)) {
                deleteRuntimeTree(staging);
            }
        }
        return component;
    }

    private String installExternal(Path ownerDirectory, String versionId, JsonObject versionMetadata,
                                  LauncherSettings.RuntimeVendor vendor, boolean redundant,
                                  Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        int major = requiredJavaMajor(versionMetadata, versionId);
        String component = runtimeComponent(vendor, major);
        Path runtimeDirectory = runtimeDirectory(ownerDirectory, component, redundant);
        ExternalJavaRuntimeInstaller.install(runtimeDirectory, major, vendor, progress);
        return component;
    }

    public String runtimeComponent(LauncherSettings.RuntimeVendor vendor, JsonObject metadata, String versionId)
            throws IOException {
        return vendor == LauncherSettings.RuntimeVendor.MOJANG
                ? requiredComponent(metadata, versionId)
                : runtimeComponent(vendor, requiredJavaMajor(metadata, versionId));
    }

    private static String runtimeComponent(LauncherSettings.RuntimeVendor vendor, int major) {
        return vendor.name().toLowerCase(Locale.ROOT) + "-java-" + major;
    }

    private static int requiredJavaMajor(JsonObject metadata, String versionId) throws IOException {
        JsonObject javaVersion = metadata != null && metadata.has("javaVersion")
                && metadata.get("javaVersion").isJsonObject() ? metadata.getAsJsonObject("javaVersion") : null;
        if (javaVersion != null && javaVersion.has("majorVersion")) {
            try {
                int major = javaVersion.get("majorVersion").getAsInt();
                if (major >= 8 && major <= 99) {
                    return major;
                }
            } catch (RuntimeException exception) {
                throw new IOException("Minecraft metadata contains an invalid Java major version.", exception);
            }
        }
        return switch (requiredComponent(metadata, versionId)) {
            case "jre-legacy" -> 8;
            case "java-runtime-alpha" -> 16;
            case "java-runtime-gamma" -> 17;
            case "java-runtime-delta" -> 21;
            case "java-runtime-epsilon" -> 25;
            default -> throw new IOException("Could not determine Java required by Minecraft " + versionId + ".");
        };
    }

    public Path javaExecutable(Instance instance, JsonObject versionMetadata) throws IOException {
        String component = requiredComponent(versionMetadata, instance.versionId());
        return javaExecutable(instance.directory(), component, instance.versionId());
    }

    public Path javaExecutable(Instance instance, String component) throws IOException {
        return javaExecutable(instance.directory(), component, instance.versionId());
    }

    public Path javaExecutable(Path ownerDirectory, String component, String versionId) throws IOException {
        Path runtimeDirectory = findInstalledRuntime(ownerDirectory, component);
        InstalledExecutable installedExecutable = runtimeDirectory == null
                ? null : readInstalledExecutable(runtimeDirectory);
        if (runtimeDirectory == null || installedExecutable == null || !Files.isRegularFile(installedExecutable.path())) {
            throw new IOException("The managed Java runtime for Minecraft " + versionId
                    + " is not installed. Install it in this server's Java runtime settings or select System Java.");
        }
        if (!isWindows() && !Files.isExecutable(installedExecutable.path())) {
            throw new IOException("The managed Java executable is not marked executable: "
                    + installedExecutable.path());
        }
        if (!installedExecutable.sha1().equals(sha1(installedExecutable.path()))) {
            throw new IOException("The managed Java executable failed its integrity check. Reinstall this runtime.");
        }
        return installedExecutable.path();
    }

    public boolean isInstalled(Path ownerDirectory, String component) throws IOException {
        return findInstalledRuntime(ownerDirectory, component) != null;
    }

    public String runtimeComponent(Instance instance) throws IOException {
        LauncherSettings.RuntimeVendor vendor = launcherSettings == null
                ? LauncherSettings.RuntimeVendor.MOJANG : launcherSettings.runtimeVendor();
        return runtimeComponent(instance, vendor);
    }

    public String runtimeComponent(Instance instance, LauncherSettings.RuntimeVendor vendor) throws IOException {
        Objects.requireNonNull(vendor);
        Path versionFile = instance.gameDirectory().resolve("versions").resolve(instance.versionId())
                .resolve(instance.versionId() + ".json");
        if (!Files.isRegularFile(versionFile) || Files.isSymbolicLink(versionFile)) {
            throw new IOException("Installed Minecraft version metadata is missing for " + instance.versionId());
        }
        try {
            JsonObject metadata = JsonParser.parseString(Files.readString(versionFile)).getAsJsonObject();
            return runtimeComponent(vendor, metadata, instance.versionId());
        } catch (JsonParseException | IllegalStateException exception) {
            throw new IOException("Could not read Minecraft Java runtime requirements.", exception);
        }
    }

    public List<InstalledRuntime> listInstalledRuntimes(List<RuntimeOwner> owners) throws IOException {
        if (dataDirectory == null) {
            return List.of();
        }
        Map<Path, InstalledRuntime> found = new java.util.LinkedHashMap<>();
        collectInstalledRuntimes(dataDirectory.resolve("java-runtimes"), owners, found);
        for (RuntimeOwner owner : owners) {
            collectInstalledRuntimes(owner.directory().resolve("java-runtimes"), owners, found);
        }
        return found.values().stream()
                .sorted(java.util.Comparator.comparing(InstalledRuntime::vendor)
                        .thenComparing(InstalledRuntime::version))
                .toList();
    }

    private void collectInstalledRuntimes(Path root, List<RuntimeOwner> owners,
                                          Map<Path, InstalledRuntime> found) throws IOException {
        if (!Files.isDirectory(root) || Files.isSymbolicLink(root)) {
            return;
        }
        Path launcherRoot = dataDirectory.toRealPath();
        if (!root.toRealPath().startsWith(launcherRoot)) {
            return;
        }
        try (var paths = Files.list(root)) {
            for (Path directory : paths.filter(Files::isDirectory).filter(path -> !Files.isSymbolicLink(path)).toList()) {
                if (!Files.isRegularFile(directory.resolve(JAVA_PATH_FILE))
                        || !Files.isRegularFile(directory.resolve(RUNTIME_VERSION_FILE))) {
                    continue;
                }
                String component = directory.getFileName().toString();
                boolean shared = dataDirectory.resolve("java-runtimes").equals(root);
                List<String> users = owners.stream()
                        .filter(RuntimeOwner::managed)
                        .filter(owner -> component.equals(owner.component()))
                        .filter(owner -> shared || owner.directory().resolve("java-runtimes")
                                .resolve(component).equals(directory))
                        .map(RuntimeOwner::name).distinct().sorted().toList();
                Path normalized = directory.toAbsolutePath().normalize();
                String vendor = Files.isRegularFile(directory.resolve(RUNTIME_VENDOR_FILE))
                        ? Files.readString(directory.resolve(RUNTIME_VENDOR_FILE)).trim()
                        : "MOJANG";
                String version = Files.readString(directory.resolve(RUNTIME_VERSION_FILE)).trim();
                found.put(normalized, new InstalledRuntime(normalized, component, vendor, version, users));
            }
        }
    }

    public void deleteRuntime(InstalledRuntime runtime) throws IOException {
        Objects.requireNonNull(runtime);
        if (!runtime.users().isEmpty()) {
            throw new IOException("This Java runtime is still used by " + String.join(", ", runtime.users()) + ".");
        }
        if (dataDirectory == null) {
            throw new IOException("Launcher runtime storage is not configured.");
        }
        Path directory = runtime.directory().toAbsolutePath().normalize();
        Path parent = directory.getParent();
        if (parent == null || !parent.getFileName().toString().equals("java-runtimes")
                || !directory.startsWith(dataDirectory) || Files.isSymbolicLink(parent)
                || Files.isSymbolicLink(directory)) {
            throw new IOException("Java runtime path is outside the launcher runtime folders.");
        }
        Path launcherRoot = dataDirectory.toRealPath();
        if (!parent.toRealPath().startsWith(launcherRoot)
                || !directory.toRealPath().startsWith(launcherRoot)) {
            throw new IOException("Java runtime resolves outside the launcher runtime folders.");
        }
        deleteRuntimeTree(directory);
    }

    private Path findInstalledRuntime(Path ownerDirectory, String component) throws IOException {
        Path preferred = runtimeDirectory(ownerDirectory, component,
                launcherSettings == null || launcherSettings.redundantJavaRuntimes());
        Path alternate = runtimeDirectory(ownerDirectory, component,
                !(launcherSettings == null || launcherSettings.redundantJavaRuntimes()));
        for (Path candidate : List.of(preferred, alternate)) {
            if (!Files.isDirectory(candidate) || Files.isSymbolicLink(candidate)) {
                continue;
            }
            InstalledExecutable executable;
            try {
                executable = readInstalledExecutable(candidate);
            } catch (IOException exception) {
                continue;
            }
            if (executable != null && Files.isRegularFile(executable.path())
                    && (isWindows() || Files.isExecutable(executable.path()))
                    && executable.sha1().equals(sha1(executable.path()))) {
                return candidate;
            }
        }
        return null;
    }

    public static String requiredComponent(JsonObject versionMetadata, String versionId) throws IOException {
        JsonObject javaVersion = versionMetadata != null && versionMetadata.has("javaVersion")
                && versionMetadata.get("javaVersion").isJsonObject()
                ? versionMetadata.getAsJsonObject("javaVersion") : null;
        if (javaVersion != null && javaVersion.has("component")
                && javaVersion.get("component").isJsonPrimitive()) {
            String component = javaVersion.get("component").getAsString();
            if (SAFE_COMPONENT.matcher(component).matches()) {
                return component;
            }
            throw new IOException("Minecraft metadata contains an invalid Java runtime component.");
        }
        return fallbackComponent(versionId);
    }

    static String fallbackComponent(String versionId) throws IOException {
        if (versionId == null) {
            throw new IOException("Minecraft version is required to select a Java runtime.");
        }
        java.util.regex.Matcher numberedVersion = Pattern.compile("^1\\.(\\d+)(?:\\.(\\d+))?(?:[-+].*)?$")
                .matcher(versionId);
        if (numberedVersion.matches()) {
            int minor = Integer.parseInt(numberedVersion.group(1));
            int patch = numberedVersion.group(2) == null ? 0 : Integer.parseInt(numberedVersion.group(2));
            if (minor <= 16) {
                return "jre-legacy";
            }
            if (minor == 17) {
                return "java-runtime-alpha";
            }
            if (minor <= 19 || (minor == 20 && patch <= 4)) {
                return "java-runtime-gamma";
            }
            if (minor == 20 || minor <= 25) {
                return "java-runtime-delta";
            }
        }
        if (versionId.matches("26(?:\\..*)?")) {
            return "java-runtime-epsilon";
        }
        if (versionId.matches("(?:a|b|rc)1\\.[0-9]+\\.[0-9]+")) {
            return "jre-legacy";
        }
        if (versionId.matches("(?:1[0-9]|2[0-9])w[0-9]+[a-z]")) {
            int year = Integer.parseInt(versionId.substring(0, 2));
            if (year <= 20) {
                return "jre-legacy";
            }
            if (year == 21) {
                return "java-runtime-alpha";
            }
            if (year <= 23) {
                return "java-runtime-gamma";
            }
            return year >= 26 ? "java-runtime-epsilon" : "java-runtime-delta";
        }
        throw new IOException("Minecraft " + versionId
                + " does not declare its Java runtime requirement. Select System Java or install the version first.");
    }

    static String currentPlatform() throws IOException {
        String os = System.getProperty("os.name", "").toLowerCase(Locale.ROOT);
        String arch = System.getProperty("os.arch", "").toLowerCase(Locale.ROOT);
        boolean arm64 = arch.equals("aarch64") || arch.equals("arm64");
        boolean x64 = arch.equals("amd64") || arch.equals("x86_64") || arch.equals("x64");
        boolean x86 = arch.equals("x86") || arch.matches("i[3-6]86");
        if (os.contains("linux")) {
            if (x64) {
                return "linux";
            }
            if (x86) {
                return "linux-i386";
            }
        } else if (os.contains("mac") || os.contains("darwin")) {
            if (arm64) {
                return "mac-os-arm64";
            }
            if (x64) {
                return "mac-os";
            }
        } else if (os.contains("win")) {
            if (arm64) {
                return "windows-arm64";
            }
            if (x64) {
                return "windows-x64";
            }
            if (x86) {
                return "windows-x86";
            }
        }
        throw new IOException("Mojang does not provide a managed Java runtime for this operating system/architecture.");
    }

    static RuntimeManifestInfo selectRuntime(JsonObject index, String platform, String component) throws IOException {
        JsonElement platformElement = index.get(platform);
        if (platformElement == null || !platformElement.isJsonObject()) {
            throw new IOException("Mojang has no Java runtime index for " + platform + ".");
        }
        JsonElement runtimesElement = platformElement.getAsJsonObject().get(component);
        if (runtimesElement == null || !runtimesElement.isJsonArray()
                || runtimesElement.getAsJsonArray().isEmpty()) {
            throw new IOException("Mojang has no " + component + " Java runtime for " + platform + ".");
        }
        JsonObject newest = null;
        String newestRelease = "";
        for (JsonElement element : runtimesElement.getAsJsonArray()) {
            if (!element.isJsonObject()) {
                continue;
            }
            JsonObject candidate = element.getAsJsonObject();
            JsonObject manifest = object(candidate, "manifest");
            String release = requiredString(object(candidate, "version"), "released");
            if (newest == null || release.compareTo(newestRelease) > 0) {
                newest = manifest;
                newestRelease = release;
            }
        }
        if (newest == null) {
            throw new IOException("Mojang's " + component + " runtime index is empty for " + platform + ".");
        }
        String url = requiredString(newest, "url");
        String sha1 = requiredString(newest, "sha1");
        String version = requiredString(objectFromRuntime(runtimesElement.getAsJsonArray(), url), "name");
        requireMojangUri(URI.create(url));
        if (!sha1.matches("[0-9a-fA-F]{40}") || !SAFE_RUNTIME_VERSION.matcher(version).matches()) {
            throw new IOException("Mojang runtime manifest has invalid version or checksum metadata.");
        }
        return new RuntimeManifestInfo(URI.create(url), sha1, version);
    }

    static List<RuntimeFile> parseRuntimeFiles(JsonObject manifest) throws IOException {
        JsonObject fileMap = object(manifest, "files");
        List<RuntimeFile> files = new ArrayList<>();
        for (Map.Entry<String, JsonElement> entry : fileMap.entrySet()) {
            String relativePath = safeRelativePath(entry.getKey());
            if (relativePath.equals(JAVA_PATH_FILE) || relativePath.equals(RUNTIME_VERSION_FILE)
                    || relativePath.equals(RUNTIME_VENDOR_FILE)) {
                throw new IOException("Mojang runtime manifest collides with launcher runtime metadata.");
            }
            if (!entry.getValue().isJsonObject()) {
                throw new IOException("Mojang runtime manifest contains an invalid file entry.");
            }
            JsonObject item = entry.getValue().getAsJsonObject();
            String type = requiredString(item, "type");
            switch (type) {
                case "directory" -> files.add(new RuntimeFile(relativePath, RuntimeFileType.DIRECTORY,
                        null, "", false, ""));
                case "link" -> {
                    String target = requiredString(item, "target");
                    safeLinkTarget(relativePath, target);
                    files.add(new RuntimeFile(relativePath, RuntimeFileType.LINK, null, "", false, target));
                }
                case "file" -> {
                    JsonObject downloads = object(item, "downloads");
                    JsonElement rawElement = downloads.get("raw");
                    if (rawElement == null || !rawElement.isJsonObject()) {
                        throw new IOException("Mojang runtime file has no raw download: " + relativePath);
                    }
                    JsonObject raw = rawElement.getAsJsonObject();
                    URI source = URI.create(requiredString(raw, "url"));
                    requireMojangUri(source);
                    String sha1 = requiredString(raw, "sha1");
                    if (!sha1.matches("[0-9a-fA-F]{40}")) {
                        throw new IOException("Mojang runtime file has an invalid checksum: " + relativePath);
                    }
                    boolean executable = item.has("executable") && item.get("executable").getAsBoolean();
                    files.add(new RuntimeFile(relativePath, RuntimeFileType.FILE, source, sha1, executable, ""));
                }
                default -> throw new IOException("Unsupported Mojang runtime file type: " + type);
            }
        }
        if (files.isEmpty()) {
            throw new IOException("Mojang runtime manifest contains no files.");
        }
        return List.copyOf(files);
    }

    private static JsonObject objectFromRuntime(JsonArray runtimes, String manifestUrl) throws IOException {
        for (JsonElement element : runtimes) {
            if (!element.isJsonObject()) {
                continue;
            }
            JsonObject runtime = element.getAsJsonObject();
            JsonObject manifest = object(runtime, "manifest");
            if (manifestUrl.equals(manifest.get("url").getAsString())) {
                return object(runtime, "version");
            }
        }
        throw new IOException("Mojang runtime version metadata is incomplete.");
    }

    private JsonObject fetchJson(URI uri) throws IOException, InterruptedException {
        return fetchJson(uri, "");
    }

    private JsonObject fetchJson(URI uri, String expectedSha1) throws IOException, InterruptedException {
        requireMojangUri(uri);
        HttpRequest request = HttpRequest.newBuilder(uri)
                .timeout(Duration.ofMinutes(2))
                .header("Accept", "application/json")
                .header("User-Agent", "StrataLauncher/0.1.0")
                .GET()
                .build();
        HttpResponse<InputStream> response = httpClient.send(request, HttpResponse.BodyHandlers.ofInputStream());
        byte[] bytes;
        try (InputStream input = response.body()) {
            if (response.statusCode() < 200 || response.statusCode() >= 300) {
                throw new IOException("Mojang runtime request failed with HTTP " + response.statusCode());
            }
            bytes = input.readNBytes(MAX_MANIFEST_BYTES + 1);
        }
        if (bytes.length > MAX_MANIFEST_BYTES) {
            throw new IOException("Mojang runtime metadata exceeds the supported size limit.");
        }
        if (!expectedSha1.isBlank() && !expectedSha1.equalsIgnoreCase(sha1(bytes))) {
            throw new IOException("Mojang Java runtime manifest checksum does not match.");
        }
        try {
            return JsonParser.parseString(new String(bytes, java.nio.charset.StandardCharsets.UTF_8)).getAsJsonObject();
        } catch (JsonParseException | IllegalStateException exception) {
            throw new IOException("Could not parse Mojang Java runtime metadata.", exception);
        }
    }

    private static String findJavaExecutable(List<RuntimeFile> files, String platform) throws IOException {
        List<String> candidates = platform.startsWith("windows")
                ? List.of("bin/javaw.exe", "bin/java.exe", "bin/javaw", "bin/java")
                : List.of("bin/java", "Contents/Home/bin/java");
        return files.stream()
                .filter(file -> file.type() == RuntimeFileType.FILE)
                .map(RuntimeFile::path)
                .filter(path -> candidates.stream().anyMatch(path::endsWith))
                .sorted((left, right) -> Integer.compare(candidateRank(right, candidates), candidateRank(left, candidates)))
                .findFirst()
                .orElseThrow(() -> new IOException("Mojang runtime manifest does not contain a Java executable."));
    }

    private static int candidateRank(String path, List<String> candidates) {
        for (int i = 0; i < candidates.size(); i++) {
            if (path.endsWith(candidates.get(i))) {
                return candidates.size() - i;
            }
        }
        return 0;
    }

    private Path runtimeDirectory(Path ownerDirectory, String component, boolean redundant) throws IOException {
        if (!SAFE_COMPONENT.matcher(component).matches()) {
            throw new IOException("Invalid Java runtime component.");
        }
        Path instanceRoot = ownerDirectory.toAbsolutePath().normalize();
        Path runtimeRoot = !redundant && dataDirectory != null
                ? dataDirectory.resolve("java-runtimes")
                : instanceRoot.resolve("java-runtimes");
        Path runtimePath = runtimeRoot.resolve(component).normalize();
        if (!runtimePath.startsWith(runtimeRoot) || dataDirectory != null
                && !runtimeRoot.startsWith(dataDirectory)) {
            throw new IOException("Java runtime path is outside this instance.");
        }
        if (Files.isSymbolicLink(instanceRoot) || Files.isSymbolicLink(runtimeRoot)
                || Files.isSymbolicLink(runtimePath)) {
            throw new IOException("Java runtimes cannot be accessed through a symbolic link.");
        }
        return runtimePath;
    }

    private static InstalledExecutable readInstalledExecutable(Path runtimeDirectory) throws IOException {
        Path marker = runtimeDirectory.resolve(JAVA_PATH_FILE);
        if (!Files.isRegularFile(marker)) {
            return null;
        }
        List<String> markerLines = Files.readAllLines(marker);
        if (markerLines.size() != 2 || !markerLines.get(1).matches("[0-9a-f]{40}")) {
            throw new IOException("Managed Java runtime metadata is invalid. Reinstall this runtime.");
        }
        String relative = safeRelativePath(markerLines.get(0));
        Path executable = resolveInside(runtimeDirectory, relative);
        if (!executable.startsWith(runtimeDirectory.toAbsolutePath().normalize())) {
            throw new IOException("Installed Java executable path is outside its runtime.");
        }
        return new InstalledExecutable(executable, markerLines.get(1));
    }

    private static Path resolveInside(Path root, String relative) throws IOException {
        Path normalizedRoot = root.toAbsolutePath().normalize();
        Path resolved = normalizedRoot.resolve(safeRelativePath(relative)).normalize();
        if (!resolved.startsWith(normalizedRoot)) {
            throw new IOException("Java runtime manifest path escapes its installation directory.");
        }
        return resolved;
    }

    private static String safeRelativePath(String path) throws IOException {
        if (path == null || path.isBlank() || path.indexOf('\\') >= 0 || path.startsWith("/")
                || path.matches("^[A-Za-z]:.*")) {
            throw new IOException("Mojang runtime manifest contains an unsafe path.");
        }
        Path relative = Path.of(path);
        if (relative.isAbsolute()) {
            throw new IOException("Mojang runtime manifest contains an absolute path.");
        }
        for (Path component : relative) {
            if (component.toString().equals("..") || component.toString().equals(".")) {
                throw new IOException("Mojang runtime manifest contains an unsafe path.");
            }
        }
        String normalized = relative.normalize().toString();
        if (normalized.isBlank() || normalized.equals(".")) {
            throw new IOException("Mojang runtime manifest contains an empty path.");
        }
        return normalized;
    }

    private static void safeLinkTarget(String path, String target) throws IOException {
        if (target.isBlank() || target.indexOf('\\') >= 0 || Path.of(target).isAbsolute()
                || target.matches("^[A-Za-z]:.*")) {
            throw new IOException("Mojang runtime manifest contains an unsafe symbolic link.");
        }
        Path link = Path.of(path);
        Path resolved = link.getParent() == null
                ? Path.of(target).normalize()
                : link.getParent().resolve(target).normalize();
        if (resolved.isAbsolute() || resolved.startsWith("..")) {
            throw new IOException("Mojang runtime symbolic link escapes its installation directory.");
        }
    }

    private static void createLink(Path root, String path, String target) throws IOException {
        safeLinkTarget(path, target);
        Path destination = resolveInside(root, path);
        Path linkTarget = Path.of(target);
        Files.createDirectories(destination.getParent());
        try {
            Files.createSymbolicLink(destination, linkTarget);
        } catch (UnsupportedOperationException exception) {
            throw new IOException("This system cannot create a symbolic link required by the Mojang Java runtime.",
                    exception);
        }
    }

    private static void markExecutable(Path file) throws IOException {
        if (isWindows()) {
            return;
        }
        try {
            Set<PosixFilePermission> permissions = EnumSet.copyOf(Files.getPosixFilePermissions(file));
            permissions.add(PosixFilePermission.OWNER_EXECUTE);
            permissions.add(PosixFilePermission.GROUP_EXECUTE);
            permissions.add(PosixFilePermission.OTHERS_EXECUTE);
            Files.setPosixFilePermissions(file, permissions);
        } catch (UnsupportedOperationException exception) {
            if (!file.toFile().setExecutable(true, false)) {
                throw new IOException("Could not mark the managed Java runtime executable: " + file);
            }
        }
    }

    private static void requireMojangUri(URI uri) throws IOException {
        String host = uri.getHost();
        if (!"https".equalsIgnoreCase(uri.getScheme()) || host == null
                || !(host.equals("mojang.com") || host.endsWith(".mojang.com"))) {
            throw new IOException("Java runtime metadata and files must come from an official Mojang HTTPS host.");
        }
    }

    private static JsonObject object(JsonObject object, String key) throws IOException {
        JsonElement value = object == null ? null : object.get(key);
        if (value == null || !value.isJsonObject()) {
            throw new IOException("Mojang runtime metadata is missing " + key + ".");
        }
        return value.getAsJsonObject();
    }

    private static String requiredString(JsonObject object, String key) throws IOException {
        JsonElement value = object == null ? null : object.get(key);
        if (value == null || value.isJsonNull() || !value.isJsonPrimitive()) {
            throw new IOException("Mojang runtime metadata is missing " + key + ".");
        }
        return value.getAsString();
    }

    private static String sha1(byte[] content) throws IOException {
        try {
            return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-1").digest(content));
        } catch (NoSuchAlgorithmException exception) {
            throw new IOException("SHA-1 is not available for Java runtime verification.", exception);
        }
    }

    private static String sha1(Path file) throws IOException {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-1");
            try (InputStream input = Files.newInputStream(file)) {
                byte[] buffer = new byte[8192];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    digest.update(buffer, 0, count);
                }
            }
            return HexFormat.of().formatHex(digest.digest());
        } catch (NoSuchAlgorithmException exception) {
            throw new IOException("SHA-1 is not available for Java runtime verification.", exception);
        }
    }

    private static boolean isWindows() {
        return System.getProperty("os.name", "").toLowerCase(Locale.ROOT).contains("win");
    }

    static void deleteRuntimeTree(Path target) throws IOException {
        if (!Files.exists(target)) {
            return;
        }
        Files.walkFileTree(target, new java.nio.file.SimpleFileVisitor<>() {
            @Override
            public java.nio.file.FileVisitResult visitFile(Path file,
                                                           java.nio.file.attribute.BasicFileAttributes attributes)
                    throws IOException {
                Files.delete(file);
                return java.nio.file.FileVisitResult.CONTINUE;
            }

            @Override
            public java.nio.file.FileVisitResult postVisitDirectory(Path directory, IOException exception)
                    throws IOException {
                if (exception != null) {
                    throw exception;
                }
                Files.delete(directory);
                return java.nio.file.FileVisitResult.CONTINUE;
            }
        });
    }

    static void replaceRuntime(Path staging, Path runtimeDirectory) throws IOException {
        Path parent = runtimeDirectory.getParent();
        if (parent == null || !staging.toAbsolutePath().normalize().getParent()
                .equals(parent.toAbsolutePath().normalize())) {
            throw new IOException("Temporary Java runtime is outside the runtime storage folder.");
        }
        if (Files.isSymbolicLink(runtimeDirectory) || Files.isSymbolicLink(staging)) {
            throw new IOException("Java runtime replacement cannot use symbolic links.");
        }
        Files.createDirectories(parent);
        Path backup = parent.resolve(".previous-" + UUID.randomUUID());
        boolean hadPrevious = Files.exists(runtimeDirectory);
        if (hadPrevious) {
            Files.move(runtimeDirectory, backup);
        }
        try {
            try {
                Files.move(staging, runtimeDirectory, StandardCopyOption.ATOMIC_MOVE);
            } catch (java.nio.file.AtomicMoveNotSupportedException exception) {
                Files.move(staging, runtimeDirectory);
            }
        } catch (IOException exception) {
            if (hadPrevious && Files.exists(backup)) {
                Files.move(backup, runtimeDirectory);
            }
            throw exception;
        }
        if (hadPrevious) {
            deleteRuntimeTree(backup);
        }
    }

    record RuntimeManifestInfo(URI manifestUri, String manifestSha1, String version) {
    }

    public record RuntimeOwner(String name, Path directory, boolean managed, String component) {
        public RuntimeOwner {
            Objects.requireNonNull(name);
            Objects.requireNonNull(directory);
        }
    }

    public record InstalledRuntime(Path directory, String component, String vendor,
                                   String version, List<String> users) {
        public InstalledRuntime {
            users = List.copyOf(users);
        }

        public String displayName() {
            String label = switch (vendor) {
                case "MOJANG" -> "Mojang";
                case "TEMURIN" -> "Eclipse Temurin";
                case "ZULU" -> "Azul Zulu";
                default -> vendor;
            };
            return label + " Java " + version + (users.isEmpty()
                    ? " · Not currently used" : " · Used by " + String.join(", ", users));
        }
    }

    private record InstalledExecutable(Path path, String sha1) {
    }

    record RuntimeFile(String path, RuntimeFileType type, URI source, String sha1, boolean executable, String target) {
    }

    enum RuntimeFileType {
        FILE,
        DIRECTORY,
        LINK
    }
}
