package net.stratalauncher.core.launch;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.download.DownloadTask;
import net.stratalauncher.core.auth.Account;
import net.stratalauncher.core.auth.AccountManager;
import net.stratalauncher.core.auth.LauncherSettings;
import net.stratalauncher.core.instance.Instance;
import net.stratalauncher.core.instance.InstanceManager;
import net.stratalauncher.core.loader.LoaderInstaller;
import net.stratalauncher.core.loader.LoaderProfile;
import net.stratalauncher.core.loader.LoaderSelection;
import net.stratalauncher.core.mojang.MojangManifestClient;
import net.stratalauncher.core.runtime.JavaRuntimeManager;
import net.stratalauncher.core.settings.InstanceSettings;

import java.io.IOException;
import java.net.URI;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.function.Consumer;
import java.util.regex.Pattern;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public class GameLauncher {
    private static final Pattern SAFE_VERSION_ID = Pattern.compile("[A-Za-z0-9._-]+");
    private static final String DEFAULT_LIBRARY_REPOSITORY = "https://libraries.minecraft.net/";

    private final Path dataDirectory;
    private final MojangManifestClient manifestClient;
    private final DownloadManager downloadManager;
    private final LoaderInstaller loaderInstaller;
    private final LauncherSettings launcherSettings;
    private final JavaRuntimeManager javaRuntimeManager;

    public GameLauncher(InstanceManager instanceManager, MojangManifestClient manifestClient,
                        DownloadManager downloadManager) {
        this(instanceManager, manifestClient, downloadManager, null, null);
    }

    public GameLauncher(InstanceManager instanceManager, MojangManifestClient manifestClient,
                        DownloadManager downloadManager, LoaderInstaller loaderInstaller) {
        this(instanceManager, manifestClient, downloadManager, loaderInstaller, null);
    }

    public GameLauncher(InstanceManager instanceManager, MojangManifestClient manifestClient,
                        DownloadManager downloadManager, LoaderInstaller loaderInstaller,
                        LauncherSettings launcherSettings) {
        this(instanceManager, manifestClient, downloadManager, loaderInstaller, launcherSettings, null);
    }

    public GameLauncher(InstanceManager instanceManager, MojangManifestClient manifestClient,
                        DownloadManager downloadManager, LoaderInstaller loaderInstaller,
                        LauncherSettings launcherSettings, JavaRuntimeManager javaRuntimeManager) {
        this.dataDirectory = instanceManager.dataDirectory().toAbsolutePath().normalize();
        this.manifestClient = Objects.requireNonNull(manifestClient);
        this.downloadManager = Objects.requireNonNull(downloadManager);
        this.loaderInstaller = loaderInstaller;
        this.launcherSettings = launcherSettings;
        this.javaRuntimeManager = javaRuntimeManager;
    }

    public void install(Instance instance, Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        install(instance, LoaderSelection.vanilla(), progress);
    }

    public void install(Instance instance, LoaderSelection loaderSelection,
                        Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        Consumer<DownloadManager.Progress> status = progress == null ? ignored -> { } : progress;
        if (loaderSelection.type() != net.stratalauncher.core.loader.LoaderType.VANILLA
                && loaderInstaller == null) {
            throw new IOException("Loader installation is not configured in this launcher.");
        }
        if (loaderInstaller != null) {
            loaderInstaller.prepare(instance, loaderSelection);
        }
        Files.deleteIfExists(installedMarker(instance));
        installFiles(instance, status);
        if (loaderInstaller != null) {
            loaderInstaller.install(instance, loaderSelection, status);
        }
        Files.writeString(installedMarker(instance), instance.versionId());
        report(status, "Installation complete");
    }

    public Process launchOffline(Instance instance, String username, Consumer<DownloadManager.Progress> progress)
            throws IOException {
        String playerName = username == null || username.isBlank() ? "Player" : username.trim();
        if (!playerName.matches("[A-Za-z0-9_]{1,16}")) {
            throw new IllegalArgumentException("Offline player name must be 1-16 letters, numbers, or underscores");
        }
        return launchConfigured(instance, playerName, UUIDs.offlineUuid(playerName), "0", "legacy", "", progress);
    }

    public Process launchAccount(Instance instance, AccountManager.LaunchProfile profile,
                                 Consumer<DownloadManager.Progress> progress) throws IOException {
        Account account = Objects.requireNonNull(profile).account();
        if (!account.name().matches("[A-Za-z0-9_]{1,16}")) {
            throw new IllegalArgumentException("Account name cannot be used to launch Minecraft.");
        }
        String userType = account.type() == Account.Type.MICROSOFT ? "msa" : "legacy";
        return launchConfigured(instance, account.name(), account.uuid(), profile.accessToken(),
                userType, account.xuid(), progress);
    }

    public void installInstanceJavaRuntime(Instance instance,
                                           Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        if (javaRuntimeManager == null) {
            throw new IOException("Java runtime management is not configured in this launcher.");
        }
        String component = javaRuntimeManager.install(instance, readInstalledMetadata(instance), progress);
        new InstanceSettings(instance).setJavaRuntimeComponent(component);
    }

    private Process launchConfigured(Instance instance, String playerName, String playerUuid, String accessToken,
                                     String userType, String xuid, Consumer<DownloadManager.Progress> progress)
            throws IOException {
        String versionId = instance.versionId();
        if (!SAFE_VERSION_ID.matcher(versionId).matches()) {
            throw new IOException("Invalid Minecraft version identifier: " + versionId);
        }
        JsonObject metadata = readInstalledMetadata(instance);
        LoaderProfile loaderProfile = loaderInstaller == null ? null : loaderInstaller.installedProfile(instance);
        if (loaderProfile != null) {
            metadata = mergeLoaderMetadata(metadata, loaderProfile);
        }
        Path gameDirectory = instance.gameDirectory();
        String clientVersionId = metadata.has("jar") ? requiredString(metadata, "jar") : versionId;
        Path versionDirectory = gameDirectory.resolve("versions").resolve(clientVersionId);
        Path clientJar = versionDirectory.resolve(clientVersionId + ".jar");
        if (!Files.isRegularFile(clientJar)) {
            throw new IOException("Minecraft is not installed for this instance. Install it before launching.");
        }

        JsonObject assetIndex = requiredObject(metadata, "assetIndex");
        String assetIndexId = requiredString(assetIndex, "id");
        Path assetsDirectory = dataDirectory.resolve("assets");
        Path nativeDirectory = versionDirectory.resolve(versionId + "-natives");
        Path librariesDirectory = dataDirectory.resolve("libraries");
        List<Path> classpath = new ArrayList<>();
        classpath.add(clientJar);
        collectInstalledLibraryClasspath(metadata, List.of(librariesDirectory, gameDirectory.resolve("libraries")),
                classpath);
        Path loggingFile = loggingConfigPath(metadata, assetsDirectory);

        String launchVersionName = loaderProfile == null ? versionId : loaderProfile.profileVersionId();
        Map<String, String> substitutions = createSubstitutions(playerName, playerUuid, accessToken, userType, xuid,
                launchVersionName, gameDirectory, assetsDirectory, assetIndexId, nativeDirectory, loggingFile,
                gameDirectory.resolve("libraries"), classpath);
        InstanceSettings instanceSettings = new InstanceSettings(instance);
        List<String> globalJavaFlags = launcherSettings == null
                ? LauncherSettings.RECOMMENDED_JAVA_FLAGS : launcherSettings.globalJavaLaunchFlags();
        List<String> javaLaunchFlags = instanceSettings.effectiveJavaLaunchFlags(globalJavaFlags);
        List<String> command = new ArrayList<>();
        command.add(javaExecutable(instance, metadata, instanceSettings).toString());
        command.addAll(composeJvmArguments(resolveArguments(metadata, "jvm", substitutions), javaLaunchFlags));
        command.add("-Djava.library.path=" + nativeDirectory);
        command.add("-cp");
        command.add(String.join(System.getProperty("path.separator"),
                classpath.stream().map(Path::toString).toList()));
        command.add(requiredString(metadata, "mainClass"));
        command.addAll(resolveGameArguments(metadata, substitutions));

        if (progress != null) {
            report(progress, "Starting Minecraft " + versionId);
        }
        Files.createDirectories(gameDirectory);
        Path log = gameDirectory.resolve("launcher.log");
        return new ProcessBuilder(command)
                .directory(gameDirectory.toFile())
                .redirectErrorStream(true)
                .redirectOutput(ProcessBuilder.Redirect.appendTo(log.toFile()))
                .start();
    }

    List<String> effectiveJavaLaunchFlags(Instance instance) throws IOException {
        List<String> globalJavaFlags = launcherSettings == null
                ? LauncherSettings.RECOMMENDED_JAVA_FLAGS : launcherSettings.globalJavaLaunchFlags();
        return new InstanceSettings(instance).effectiveJavaLaunchFlags(globalJavaFlags);
    }

    private Path javaExecutable(Instance instance, JsonObject metadata, InstanceSettings settings)
            throws IOException {
        if (settings.javaRuntimeMode() == InstanceSettings.JavaRuntimeMode.MANAGED) {
            if (javaRuntimeManager == null) {
                throw new IOException("Java runtime management is not configured in this launcher.");
            }
            String component = settings.javaRuntimeComponent();
            if (component == null || component.isBlank()) {
                component = javaRuntimeManager.runtimeComponent(
                        LauncherSettings.RuntimeVendor.MOJANG, metadata, instance.versionId());
            }
            return javaRuntimeManager.javaExecutable(instance, component);
        }
        return Path.of(System.getProperty("java.home"), "bin", executableName("java"));
    }

    static List<String> composeJvmArguments(List<String> metadataArguments, List<String> customFlags) {
        boolean customCollectorSelected = customFlags.stream().anyMatch(GameLauncher::isGarbageCollectorFlag);
        List<String> arguments = new ArrayList<>();
        for (String argument : metadataArguments) {
            if (!customCollectorSelected || !isGarbageCollectorFlag(argument)) {
                arguments.add(argument);
            }
        }
        arguments.addAll(customFlags);
        return List.copyOf(arguments);
    }

    private static boolean isGarbageCollectorFlag(String argument) {
        return argument.matches("-XX:[+-]Use[A-Za-z0-9]+GC");
    }

    private void installFiles(Instance instance, Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        String versionId = instance.versionId();
        if (!SAFE_VERSION_ID.matcher(versionId).matches()) {
            throw new IOException("Invalid Minecraft version identifier: " + versionId);
        }
        report(progress, "Fetching Mojang version manifest");
        MojangManifestClient.VersionManifest manifest = manifestClient.fetchManifest();
        MojangManifestClient.Version version = manifest.versions().stream()
                .filter(candidate -> candidate.id().equals(versionId))
                .findFirst()
                .orElseThrow(() -> new IOException("Minecraft version is no longer in the Mojang manifest: " + versionId));

        Path gameDirectory = instance.gameDirectory();
        Path versionDirectory = gameDirectory.resolve("versions").resolve(versionId);
        Files.createDirectories(versionDirectory);
        Path metadataFile = versionDirectory.resolve(versionId + ".json");
        downloadManager.downloadAll(List.of(new DownloadTask(
                version.url(), metadataFile, version.sha1(), "Minecraft " + versionId + " metadata")), progress);
        JsonObject metadata = JsonParser.parseString(Files.readString(metadataFile)).getAsJsonObject();

        Path clientJar = versionDirectory.resolve(versionId + ".jar");
        JsonObject clientDownload = requiredObject(requiredObject(metadata, "downloads"), "client");
        Path librariesDirectory = dataDirectory.resolve("libraries");
        Path nativeDirectory = versionDirectory.resolve(versionId + "-natives");
        List<Path> classpath = new ArrayList<>();
        classpath.add(clientJar);
        List<Path> nativeJars = new ArrayList<>();
        List<DownloadTask> baseDownloads = new ArrayList<>();
        baseDownloads.add(new DownloadTask(URI.create(requiredString(clientDownload, "url")), clientJar,
                optionalString(clientDownload, "sha1"), "Minecraft client"));
        installLibraries(metadata, librariesDirectory, classpath, nativeJars, baseDownloads);

        Path assetsDirectory = dataDirectory.resolve("assets");
        JsonObject assetIndex = requiredObject(metadata, "assetIndex");
        String assetIndexId = requiredString(assetIndex, "id");
        Path assetIndexFile = assetsDirectory.resolve("indexes").resolve(assetIndexId + ".json");
        baseDownloads.add(new DownloadTask(URI.create(requiredString(assetIndex, "url")), assetIndexFile,
                optionalString(assetIndex, "sha1"), "Asset index " + assetIndexId));

        Path loggingFile = loggingConfigPath(metadata, assetsDirectory);
        JsonObject logging = metadata.has("logging") && metadata.get("logging").isJsonObject()
                ? metadata.getAsJsonObject("logging").getAsJsonObject("client") : null;
        if (logging != null && logging.has("file")) {
            JsonObject file = logging.getAsJsonObject("file");
            baseDownloads.add(new DownloadTask(URI.create(requiredString(file, "url")), loggingFile,
                    optionalString(file, "sha1"), "Logging configuration"));
        }

        downloadManager.downloadAll(baseDownloads, progress);
        Files.createDirectories(nativeDirectory);
        for (Path nativeJar : nativeJars) {
            extractNatives(nativeJar, nativeDirectory);
        }
        installAssets(assetIndexFile, assetsDirectory, assetIndexId, progress);
    }

    private JsonObject readInstalledMetadata(Instance instance) throws IOException {
        Path marker = installedMarker(instance);
        if (!Files.isRegularFile(marker) || !Files.readString(marker).equals(instance.versionId())) {
            throw new IOException("Instance is not installed. Use Install / Repair before launching.");
        }
        Path metadataFile = instance.gameDirectory().resolve("versions").resolve(instance.versionId())
                .resolve(instance.versionId() + ".json");
        if (!Files.isRegularFile(metadataFile)) {
            throw new IOException("Installed Minecraft metadata is missing. Use Install / Repair before launching.");
        }
        try {
            return JsonParser.parseString(Files.readString(metadataFile)).getAsJsonObject();
        } catch (RuntimeException exception) {
            throw new IOException("Installed Minecraft metadata is invalid. Use Install / Repair before launching.", exception);
        }
    }

    private static Path installedMarker(Instance instance) {
        return instance.gameDirectory().resolve(".stratalauncher-installed");
    }

    private static Path loggingConfigPath(JsonObject metadata, Path assetsDirectory) throws IOException {
        JsonObject logging = metadata.has("logging") && metadata.get("logging").isJsonObject()
                ? metadata.getAsJsonObject("logging").getAsJsonObject("client") : null;
        if (logging == null || !logging.has("file")) {
            return null;
        }
        JsonObject file = logging.getAsJsonObject("file");
        return assetsDirectory.resolve("log_configs").resolve(requiredString(file, "id"));
    }

    private void collectInstalledLibraryClasspath(JsonObject metadata, List<Path> librariesDirectories,
                                                  List<Path> classpath) throws IOException {
        JsonArray libraries = metadata.getAsJsonArray("libraries");
        if (libraries == null) {
            return;
        }
        String osName = currentOsName();
        for (JsonElement element : libraries) {
            JsonObject library = element.getAsJsonObject();
            if (!allowedByRules(library, osName)) {
                continue;
            }
            JsonObject downloads = library.has("downloads") && library.get("downloads").isJsonObject()
                    ? library.getAsJsonObject("downloads") : new JsonObject();
            JsonObject artifact = downloads.has("artifact") && downloads.get("artifact").isJsonObject()
                    ? downloads.getAsJsonObject("artifact") : null;
            Path relativeArtifact = null;
            if (artifact != null) {
                relativeArtifact = Path.of(requiredString(artifact, "path"));
            } else if (library.has("name")) {
                String[] coordinates = requiredString(library, "name").split(":", 4);
                if (coordinates.length >= 3) {
                    String classifier = coordinates.length == 4 ? "-" + coordinates[3] : "";
                    String relative = coordinates[0].replace('.', '/') + "/" + coordinates[1] + "/"
                            + coordinates[2] + "/" + coordinates[1] + "-" + coordinates[2] + classifier + ".jar";
                    relativeArtifact = Path.of(relative);
                }
            }
            if (relativeArtifact != null) {
                Path artifactPath = null;
                for (Path librariesDirectory : librariesDirectories) {
                    Path candidate = safeResolve(librariesDirectory, relativeArtifact.toString());
                    if (Files.isRegularFile(candidate)) {
                        artifactPath = candidate;
                        break;
                    }
                }
                if (artifactPath == null) {
                    throw new IOException("Installed Minecraft library is missing: " + relativeArtifact
                            + ". Use Install / Repair before launching.");
                }
                classpath.add(artifactPath);
            }
        }
    }

    private void installLibraries(JsonObject metadata, Path librariesDirectory,
                                  List<Path> classpath, List<Path> nativeJars,
                                  List<DownloadTask> downloadTasks) throws IOException {
        JsonArray libraries = metadata.getAsJsonArray("libraries");
        if (libraries == null) {
            return;
        }
        String osName = currentOsName();
        String nativeOsKey = osName.equals("osx") ? "osx" : osName;
        for (JsonElement element : libraries) {
            JsonObject library = element.getAsJsonObject();
            if (!allowedByRules(library, osName)) {
                continue;
            }
            JsonObject downloads = library.has("downloads") && library.get("downloads").isJsonObject()
                    ? library.getAsJsonObject("downloads") : new JsonObject();
            JsonObject artifact = downloads.has("artifact") && downloads.get("artifact").isJsonObject()
                    ? downloads.getAsJsonObject("artifact") : null;
            if (artifact != null) {
                Path artifactPath = safeResolve(librariesDirectory, requiredString(artifact, "path"));
                addDownload(downloadTasks, new DownloadTask(URI.create(requiredString(artifact, "url")), artifactPath,
                        optionalString(artifact, "sha1"), artifactPath.getFileName().toString()));
                classpath.add(artifactPath);
            } else if (library.has("name")) {
                String name = requiredString(library, "name");
                String[] coordinates = name.split(":", 4);
                if (coordinates.length >= 3) {
                    String relative = coordinates[0].replace('.', '/') + "/" + coordinates[1] + "/"
                            + coordinates[2] + "/" + coordinates[1] + "-" + coordinates[2] + ".jar";
                    Path artifactPath = safeResolve(librariesDirectory, relative);
                    String repository = library.has("url") ? requiredString(library, "url") : DEFAULT_LIBRARY_REPOSITORY;
                    addDownload(downloadTasks, new DownloadTask(ensureTrailingSlash(repository).resolve(relative),
                            artifactPath, "", artifactPath.getFileName().toString()));
                    classpath.add(artifactPath);
                }
            }

            JsonObject natives = library.has("natives") && library.get("natives").isJsonObject()
                    ? library.getAsJsonObject("natives") : null;
            if (natives == null || !natives.has(nativeOsKey) || !downloads.has("classifiers")) {
                continue;
            }
            String classifier = natives.get(nativeOsKey).getAsString()
                    .replace("${arch}", System.getProperty("os.arch", "").contains("64") ? "64" : "32");
            JsonObject classifiers = downloads.getAsJsonObject("classifiers");
            if (!classifiers.has(classifier)) {
                continue;
            }
            JsonObject nativeArtifact = classifiers.getAsJsonObject(classifier);
            Path nativeJar = safeResolve(librariesDirectory, requiredString(nativeArtifact, "path"));
            addDownload(downloadTasks, new DownloadTask(URI.create(requiredString(nativeArtifact, "url")), nativeJar,
                    optionalString(nativeArtifact, "sha1"), nativeJar.getFileName().toString()));
            nativeJars.add(nativeJar);
        }
    }

    private void installAssets(Path indexFile, Path assetsDirectory, String indexId,
                               Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        JsonObject index = JsonParser.parseString(Files.readString(indexFile)).getAsJsonObject();
        JsonObject objects = requiredObject(index, "objects");
        List<DownloadTask> assetDownloads = new ArrayList<>();
        for (Map.Entry<String, JsonElement> entry : objects.entrySet()) {
            JsonObject object = entry.getValue().getAsJsonObject();
            String hash = requiredString(object, "hash");
            if (!hash.matches("[0-9a-fA-F]{40}")) {
                throw new IOException("Invalid asset hash in Minecraft asset index " + indexId);
            }
            Path objectFile = safeResolve(assetsDirectory.resolve("objects"), hash.substring(0, 2) + "/" + hash);
            URI objectUri = URI.create("https://resources.download.minecraft.net/" + hash.substring(0, 2) + "/" + hash);
            addDownload(assetDownloads, new DownloadTask(objectUri, objectFile, hash, entry.getKey()));
        }
        downloadManager.downloadAll(assetDownloads, progress);
    }

    static Map<String, String> createSubstitutions(String playerName, String playerUuid, String accessToken,
                                                     String userType, String xuid, String versionId,
                                                     Path gameDirectory, Path assetsDirectory, String assetIndexId,
                                                     Path nativeDirectory, Path loggingFile, Path librariesDirectory,
                                                     List<Path> classpath) {
        Map<String, String> values = new LinkedHashMap<>();
        values.put("auth_player_name", playerName);
        values.put("version_name", versionId);
        values.put("game_directory", gameDirectory.toString());
        values.put("assets_root", assetsDirectory.toString());
        values.put("assets_index_name", assetIndexId);
        values.put("auth_uuid", playerUuid);
        values.put("auth_access_token", accessToken);
        values.put("clientid", "");
        values.put("auth_xuid", xuid);
        values.put("user_type", userType);
        values.put("version_type", "StrataLauncher");
        values.put("natives_directory", nativeDirectory.toString());
        values.put("library_directory", librariesDirectory.toString());
        values.put("classpath", String.join(System.getProperty("path.separator"),
                classpath.stream().map(Path::toString).toList()));
        values.put("launcher_name", "StrataLauncher");
        values.put("launcher_version", "0.1.0");
        values.put("user_properties", "{}");
        if (loggingFile != null) {
            values.put("path", loggingFile.toString());
        }
        return values;
    }

    private List<String> resolveGameArguments(JsonObject metadata, Map<String, String> substitutions) {
        if (metadata.has("arguments") && metadata.getAsJsonObject("arguments").has("game")) {
            return resolveArgumentArray(metadata.getAsJsonObject("arguments").getAsJsonArray("game"), substitutions);
        }
        String legacyArguments = metadata.has("minecraftArguments")
                ? metadata.get("minecraftArguments").getAsString() : "";
        List<String> result = new ArrayList<>();
        for (String token : legacyArguments.split("\\s+")) {
            if (!token.isBlank()) {
                result.add(substitute(token, substitutions));
            }
        }
        return result;
    }

    private List<String> resolveArguments(JsonObject metadata, String key, Map<String, String> substitutions) {
        if (!metadata.has("arguments") || !metadata.getAsJsonObject("arguments").has(key)) {
            return List.of();
        }
        return resolveArgumentArray(metadata.getAsJsonObject("arguments").getAsJsonArray(key), substitutions);
    }

    private List<String> resolveArgumentArray(JsonArray arguments, Map<String, String> substitutions) {
        List<String> result = new ArrayList<>();
        for (JsonElement argument : arguments) {
            if (argument.isJsonPrimitive()) {
                result.add(substitute(argument.getAsString(), substitutions));
            } else if (argument.isJsonObject()) {
                JsonObject conditional = argument.getAsJsonObject();
                if (allowedByRules(conditional, currentOsName())) {
                    JsonElement value = conditional.get("value");
                    if (value.isJsonArray()) {
                        for (JsonElement item : value.getAsJsonArray()) {
                            result.add(substitute(item.getAsString(), substitutions));
                        }
                    } else if (value.isJsonPrimitive()) {
                        result.add(substitute(value.getAsString(), substitutions));
                    }
                }
            }
        }
        return result;
    }

    static boolean allowedByRules(JsonObject item, String currentOs) {
        if (!item.has("rules")) {
            return true;
        }
        boolean allowed = false;
        for (JsonElement ruleElement : item.getAsJsonArray("rules")) {
            JsonObject rule = ruleElement.getAsJsonObject();
            JsonObject os = rule.has("os") && rule.get("os").isJsonObject()
                    ? rule.getAsJsonObject("os") : null;
            boolean matches = os == null || !os.has("name") || os.get("name").getAsString().equals(currentOs);
            JsonObject features = rule.has("features") && rule.get("features").isJsonObject()
                    ? rule.getAsJsonObject("features") : null;
            if (matches && features != null) {
                for (Map.Entry<String, JsonElement> feature : features.entrySet()) {
                    if (!feature.getValue().isJsonPrimitive()
                            || feature.getValue().getAsBoolean() != isFeatureEnabled(feature.getKey())) {
                        matches = false;
                        break;
                    }
                }
            }
            if (matches) {
                allowed = "allow".equals(rule.get("action").getAsString());
            }
        }
        return allowed;
    }

    static JsonObject mergeLoaderMetadata(JsonObject minecraftMetadata, LoaderProfile loaderProfile)
            throws IOException {
        JsonObject profile = loaderProfile.launchMetadata();
        if (!loaderProfile.minecraftVersion().equals(optionalString(profile, "inheritsFrom"))) {
            throw new IOException("Loader launch profile does not inherit from the selected Minecraft version.");
        }
        JsonObject merged = minecraftMetadata.deepCopy();
        if (profile.has("mainClass") && profile.get("mainClass").isJsonPrimitive()) {
            merged.add("mainClass", profile.get("mainClass").deepCopy());
        }
        if (profile.has("jar")) {
            merged.add("jar", profile.get("jar").deepCopy());
        }
        if (profile.has("minecraftArguments")) {
            merged.add("minecraftArguments", profile.get("minecraftArguments").deepCopy());
        }
        if (profile.has("libraries") && profile.get("libraries").isJsonArray()) {
            JsonArray libraries = merged.has("libraries") && merged.get("libraries").isJsonArray()
                    ? merged.getAsJsonArray("libraries") : new JsonArray();
            java.util.Set<String> names = new java.util.HashSet<>();
            for (JsonElement element : libraries) {
                if (element.isJsonObject() && element.getAsJsonObject().has("name")) {
                    names.add(element.getAsJsonObject().get("name").getAsString());
                }
            }
            for (JsonElement element : profile.getAsJsonArray("libraries")) {
                String name = element.isJsonObject() && element.getAsJsonObject().has("name")
                        ? element.getAsJsonObject().get("name").getAsString() : "";
                if (name.isBlank() || names.add(name)) {
                    libraries.add(element.deepCopy());
                }
            }
            merged.add("libraries", libraries);
        }
        if (profile.has("arguments") && profile.get("arguments").isJsonObject()) {
            JsonObject arguments = merged.has("arguments") && merged.get("arguments").isJsonObject()
                    ? merged.getAsJsonObject("arguments") : new JsonObject();
            for (String key : List.of("game", "jvm")) {
                if (!profile.getAsJsonObject("arguments").has(key)
                        || !profile.getAsJsonObject("arguments").get(key).isJsonArray()) {
                    continue;
                }
                JsonArray combined = arguments.has(key) && arguments.get(key).isJsonArray()
                        ? arguments.getAsJsonArray(key) : new JsonArray();
                for (JsonElement element : profile.getAsJsonObject("arguments").getAsJsonArray(key)) {
                    combined.add(element.deepCopy());
                }
                arguments.add(key, combined);
            }
            merged.add("arguments", arguments);
        }
        return merged;
    }

    private static boolean isFeatureEnabled(String feature) {
        return false;
    }

    private static void addDownload(List<DownloadTask> downloads, DownloadTask task) {
        Path destination = task.normalizedDestination();
        if (downloads.stream().noneMatch(existing -> existing.normalizedDestination().equals(destination))) {
            downloads.add(task);
        }
    }

    private static void report(Consumer<DownloadManager.Progress> progress, String message) {
        progress.accept(new DownloadManager.Progress(0, 0, message));
    }

    private static void extractNatives(Path archive, Path destination) throws IOException {
        Files.createDirectories(destination);
        try (ZipInputStream zip = new ZipInputStream(Files.newInputStream(archive))) {
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) {
                String name = entry.getName();
                Path relative = Path.of(name).normalize();
                if (relative.isAbsolute() || relative.startsWith("..") || name.startsWith("META-INF/")) {
                    continue;
                }
                Path file = destination.resolve(relative).normalize();
                if (!file.startsWith(destination)) {
                    throw new IOException("Unsafe native archive entry: " + name);
                }
                if (entry.isDirectory()) {
                    Files.createDirectories(file);
                } else {
                    Files.createDirectories(file.getParent());
                    Files.copy(zip, file, StandardCopyOption.REPLACE_EXISTING);
                }
                zip.closeEntry();
            }
        }
    }

    private static Path safeResolve(Path root, String relative) throws IOException {
        Path normalizedRoot = root.toAbsolutePath().normalize();
        Path result = normalizedRoot.resolve(relative).normalize();
        if (!result.startsWith(normalizedRoot)) {
            throw new IOException("Unsafe path in Minecraft metadata: " + relative);
        }
        return result;
    }

    private static JsonObject requiredObject(JsonObject object, String key) throws IOException {
        JsonElement element = object == null ? null : object.get(key);
        if (element == null || !element.isJsonObject()) {
            throw new IOException("Minecraft version metadata is missing " + key);
        }
        return element.getAsJsonObject();
    }

    private static String requiredString(JsonObject object, String key) throws IOException {
        JsonElement element = object == null ? null : object.get(key);
        if (element == null || element.isJsonNull() || !element.isJsonPrimitive()) {
            throw new IOException("Minecraft version metadata is missing " + key);
        }
        return element.getAsString();
    }

    private static String optionalString(JsonObject object, String key) {
        return object.has(key) && object.get(key).isJsonPrimitive() ? object.get(key).getAsString() : "";
    }

    private static String substitute(String value, Map<String, String> substitutions) {
        String result = value;
        for (Map.Entry<String, String> substitution : substitutions.entrySet()) {
            result = result.replace("${" + substitution.getKey() + "}", substitution.getValue());
        }
        return result;
    }

    private static URI ensureTrailingSlash(String value) {
        return URI.create(value.endsWith("/") ? value : value + "/");
    }

    private static String currentOsName() {
        String os = System.getProperty("os.name", "").toLowerCase(Locale.ROOT);
        if (os.contains("win")) {
            return "windows";
        }
        if (os.contains("mac")) {
            return "osx";
        }
        return "linux";
    }

    private static String executableName(String name) {
        return System.getProperty("os.name", "").toLowerCase(Locale.ROOT).contains("win") ? name + ".exe" : name;
    }

    private static final class UUIDs {
        private static String offlineUuid(String playerName) {
            return java.util.UUID.nameUUIDFromBytes(("OfflinePlayer:" + playerName)
                    .getBytes(java.nio.charset.StandardCharsets.UTF_8)).toString().replace("-", "");
        }
    }
}
