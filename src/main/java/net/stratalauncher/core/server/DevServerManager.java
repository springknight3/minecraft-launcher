package net.stratalauncher.core.server;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import com.google.gson.JsonParser;
import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.download.DownloadTask;
import net.stratalauncher.core.mojang.MojangManifestClient;
import net.stratalauncher.core.runtime.JavaRuntimeManager;
import net.stratalauncher.core.settings.InstanceSettings;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.time.Duration;
import java.time.Instant;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;
import java.util.regex.Pattern;

public final class DevServerManager implements AutoCloseable {
    private static final Pattern SAFE_NAME = Pattern.compile("[^\\\\/:*?\"<>|\\p{Cntrl}]+");
    private static final Pattern SAFE_VERSION = Pattern.compile("[A-Za-z0-9._+-]+");
    private static final int MAX_CONSOLE_LINES = 2_000;
    private static final URI FABRIC_META = URI.create("https://meta.fabricmc.net/v2/");
    private static final URI FABRIC_MAVEN = URI.create("https://maven.fabricmc.net/");
    private static final URI PAPER_API = URI.create("https://fill.papermc.io/v3/");
    private static final GsonHolder JSON = new GsonHolder();

    private final Path serversDirectory;
    private final Path installerCache;
    private final DownloadManager downloadManager;
    private final MojangManifestClient manifestClient;
    private final JavaRuntimeManager javaRuntimeManager;
    private final HttpClient httpClient;
    private final ExecutorService readers = Executors.newCachedThreadPool(runnable -> {
        Thread thread = new Thread(runnable, "stratalauncher-server-console");
        thread.setDaemon(true);
        return thread;
    });
    private final ScheduledExecutorService metrics = Executors.newSingleThreadScheduledExecutor(runnable -> {
        Thread thread = new Thread(runnable, "stratalauncher-server-metrics");
        thread.setDaemon(true);
        return thread;
    });
    private final Map<String, RunningServer> running = new ConcurrentHashMap<>();
    private final Map<String, String> terminalStates = new ConcurrentHashMap<>();

    public DevServerManager(Path dataDirectory, DownloadManager downloadManager,
                            MojangManifestClient manifestClient) {
        this(dataDirectory, downloadManager, manifestClient, null);
    }

    public void terminate(String serverId) throws IOException {
        RunningServer active = requireRunning(serverId);
        active.intentionalStop = true;
        active.process().destroyForcibly();
        try {
            if (!active.process().waitFor(5, TimeUnit.SECONDS)) {
                throw new IOException("The server process did not terminate after a forceful stop.");
            }
        } catch (InterruptedException exception) {
            Thread.currentThread().interrupt();
            throw new IOException("Interrupted while terminating server.", exception);
        }
    }

    public DevServerManager(Path dataDirectory, DownloadManager downloadManager,
                            MojangManifestClient manifestClient, JavaRuntimeManager javaRuntimeManager) {
        this.serversDirectory = Objects.requireNonNull(dataDirectory).toAbsolutePath().normalize().resolve("servers");
        this.installerCache = dataDirectory.toAbsolutePath().normalize().resolve("server-installers");
        this.downloadManager = Objects.requireNonNull(downloadManager);
        this.manifestClient = Objects.requireNonNull(manifestClient);
        this.javaRuntimeManager = javaRuntimeManager;
        httpClient = HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(15))
                .followRedirects(HttpClient.Redirect.NORMAL).build();
        metrics.scheduleAtFixedRate(this::publishMetrics, 1, 1, TimeUnit.SECONDS);
    }

    public List<ServerConfig> listServers() throws IOException {
        Files.createDirectories(serversDirectory);
        List<ServerConfig> servers = new ArrayList<>();
        try (var entries = Files.list(serversDirectory)) {
            for (Path directory : entries.filter(path -> !Files.isSymbolicLink(path))
                    .filter(Files::isDirectory).toList()) {
                Path configFile = directory.resolve("server.json");
                if (!Files.isRegularFile(configFile)) {
                    continue;
                }
                if (Files.isSymbolicLink(configFile)) {
                    throw new IOException("Refusing to read a symbolic-link server configuration: " + configFile);
                }
                ServerConfig config = readConfig(configFile);
                if (!directory.getFileName().toString().equals(config.id())) {
                    throw new IOException("Server configuration identifier does not match its directory.");
                }
                servers.add(config);
            }
        }
        return servers.stream().sorted(java.util.Comparator.comparing(
                ServerConfig::name, String.CASE_INSENSITIVE_ORDER)).toList();
    }

    public ServerConfig create(String name, String minecraftVersion, ServerDistribution distribution)
            throws IOException {
        String normalizedName = requireName(name);
        String normalizedVersion = requireVersion(minecraftVersion);
        Objects.requireNonNull(distribution, "server distribution");
        Files.createDirectories(serversDirectory);
        String id = UUID.randomUUID().toString();
        Path directory = serversDirectory.resolve(id);
        Files.createDirectory(directory);
        ServerConfig config = new ServerConfig(id, normalizedName, normalizedVersion, distribution,
                false, false, List.of(), Instant.now().toString());
        try {
            Files.createDirectories(config.serverDirectory(directory));
            writeConfig(config, directory);
            Files.writeString(config.serverDirectory(directory).resolve("eula.txt"),
                    "# Accept the Minecraft EULA from the server manager before starting this server.\neula=false\n");
            writeServerProperties(config);
        } catch (IOException exception) {
            deleteTree(directory);
            throw exception;
        }
        return config;
    }

    public ServerConfig updateSettings(ServerConfig server, String name, boolean eulaAccepted, List<String> javaFlags)
            throws IOException {
        ServerConfig stored = requireServer(server.id());
        return updateSettings(stored, name, eulaAccepted, javaFlags, stored.viewDistance(), stored.onlineMode());
    }

    public ServerConfig updateSettings(ServerConfig server, String name, boolean eulaAccepted, List<String> javaFlags,
                                       int viewDistance, boolean onlineMode) throws IOException {
        return updateSettings(server, name, eulaAccepted, javaFlags, viewDistance, onlineMode,
                server.useManagedJavaRuntime());
    }

    public ServerConfig updateSettings(ServerConfig server, String name, boolean eulaAccepted, List<String> javaFlags,
                                       int viewDistance, boolean onlineMode, boolean useManagedJavaRuntime)
            throws IOException {
        ServerConfig stored = requireServer(server.id());
        requireViewDistance(viewDistance);
        ServerConfig updated = new ServerConfig(stored.id(), requireName(name), stored.minecraftVersion(),
                stored.distribution(), stored.installed(), eulaAccepted,
                InstanceSettings.validateFlags(javaFlags), stored.createdAt(), viewDistance, onlineMode,
                useManagedJavaRuntime, useManagedJavaRuntime ? stored.javaRuntimeComponent() : null);
        Files.writeString(serverDirectory(updated).resolve("eula.txt"),
                eulaAccepted ? "eula=true\n" : "eula=false\n");
        writeServerProperties(updated);
        writeConfig(updated, serverRoot(stored.id()));
        return updated;
    }

    public ServerConfig installManagedJavaRuntime(ServerConfig server,
                                                  Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        if (javaRuntimeManager == null) {
            throw new IOException("Managed Java runtime support is not available.");
        }
        ServerConfig stored = requireServer(server.id());
        MojangManifestClient.Version version = manifestClient.fetchManifest().versions().stream()
                .filter(candidate -> candidate.id().equals(stored.minecraftVersion()))
                .findFirst()
                .orElseThrow(() -> new IOException("Minecraft version is not available in the Mojang manifest."));
        JsonObject metadata = fetchJson(version.url());
        String component = javaRuntimeManager.install(serverDirectory(stored), stored.minecraftVersion(), metadata,
                progress);
        ServerConfig updated = new ServerConfig(stored.id(), stored.name(), stored.minecraftVersion(),
                stored.distribution(), stored.installed(), stored.eulaAccepted(), stored.javaFlags(),
                stored.createdAt(), stored.viewDistance(), stored.onlineMode(), true, component);
        writeConfig(updated, serverRoot(stored.id()));
        return updated;
    }

    public void install(ServerConfig server, Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        ServerConfig stored = requireServer(server.id());
        Path serverDirectory = serverDirectory(stored);
        Files.createDirectories(serverDirectory);
        Consumer<DownloadManager.Progress> listener = progress == null ? ignored -> { } : progress;
        listener.accept(new DownloadManager.Progress(0, 1, "Resolving Minecraft " + stored.minecraftVersion()));
        MojangManifestClient.Version version = manifestClient.fetchManifest().versions().stream()
                .filter(candidate -> candidate.id().equals(stored.minecraftVersion()))
                .findFirst()
                .orElseThrow(() -> new IOException("Minecraft version is not available in the Mojang manifest."));
        JsonObject metadata = fetchJson(version.url());
        switch (stored.distribution()) {
            case VANILLA -> installVanilla(metadata, serverDirectory, listener);
            case PAPER -> installPaper(stored.minecraftVersion(), serverDirectory, listener);
            case FABRIC -> installFabric(stored.minecraftVersion(), serverDirectory, listener);
        }
        ServerConfig installed = new ServerConfig(stored.id(), stored.name(), stored.minecraftVersion(),
                stored.distribution(), true, stored.eulaAccepted(), stored.javaFlags(), stored.createdAt(),
                stored.viewDistance(), stored.onlineMode(), stored.useManagedJavaRuntime(),
                stored.javaRuntimeComponent());
        writeConfig(installed, serverRoot(stored.id()));
        listener.accept(new DownloadManager.Progress(1, 1,
                stored.distribution() + " server installation complete"));
    }

    public RunningServer start(ServerConfig server) throws IOException {
        ServerConfig stored = requireServer(server.id());
        if (!stored.installed()) {
            throw new IOException("Install this server before starting it.");
        }
        if (!stored.eulaAccepted()) {
            throw new IOException("Accept the Minecraft EULA in server settings before starting.");
        }
        RunningServer existing = running.get(stored.id());
        if (existing != null && existing.process().isAlive()) {
            throw new IOException("This server is already running.");
        }
        Path serverDirectory = serverDirectory(stored);
        Path jar = serverDirectory.resolve(stored.distribution() == ServerDistribution.FABRIC
                ? "fabric-server-launch.jar" : "server.jar");
        if (!Files.isRegularFile(jar)) {
            throw new IOException("The installed server JAR is missing. Reinstall this server.");
        }
        List<String> command = new ArrayList<>();
        Path javaExecutable;
        if (stored.useManagedJavaRuntime()) {
            if (javaRuntimeManager == null || stored.javaRuntimeComponent() == null) {
                throw new IOException("Install a managed Java runtime for this server or select System Java.");
            }
            javaExecutable = javaRuntimeManager.javaExecutable(serverDirectory, stored.javaRuntimeComponent(),
                    stored.minecraftVersion());
        } else {
            javaExecutable = Path.of(System.getProperty("java.home"), "bin", executableName("java"));
        }
        command.add(javaExecutable.toString());
        command.addAll(stored.javaFlags());
        command.add("-jar");
        command.add(jar.toString());
        command.add("--nogui");
        Process process = new ProcessBuilder(command)
                .directory(serverDirectory.toFile())
                .redirectErrorStream(true)
                .start();
        RunningServer started = new RunningServer(stored, process, Instant.now());
        terminalStates.put(stored.id(), "STARTING");
        running.put(stored.id(), started);
        readers.submit(() -> readConsole(started));
        process.onExit().thenAcceptAsync(ignored -> onExit(started), readers);
        publishSnapshot(started);
        return started;
    }

    public void stop(String serverId) throws IOException {
        RunningServer active = requireRunning(serverId);
        sendCommand(serverId, "stop");
        active.intentionalStop = true;
        try {
            if (!active.process().waitFor(10, TimeUnit.SECONDS)) {
                active.process().destroy();
                if (!active.process().waitFor(3, TimeUnit.SECONDS)) {
                    active.process().destroyForcibly();
                }
            }
        } catch (InterruptedException exception) {
            active.process().destroy();
            Thread.currentThread().interrupt();
            throw new IOException("Interrupted while stopping server.", exception);
        }
    }

    public void sendCommand(String serverId, String command) throws IOException {
        if (command == null || command.isBlank() || command.indexOf('\n') >= 0
                || command.indexOf('\r') >= 0 || command.indexOf('\0') >= 0) {
            throw new IllegalArgumentException("Enter one non-empty server command per line.");
        }
        RunningServer active = requireRunning(serverId);
        synchronized (active.input()) {
            active.input().println(command);
            if (active.input().checkError()) {
                throw new IOException("Could not send command to the server process.");
            }
        }
    }

    public void addConsoleListener(String serverId, Consumer<ConsoleEvent> listener) {
        RunningServer active = running.get(serverId);
        if (active != null) {
            active.listeners().add(Objects.requireNonNull(listener));
            List<String> buffered;
            synchronized (active.console()) {
                buffered = List.copyOf(active.console());
            }
            buffered.forEach(line -> listener.accept(new ConsoleEvent(serverId, line, Instant.now())));
            listener.accept(new ConsoleEvent(serverId,
                    "JVM flags: " + String.join(" ", active.config().javaFlags()), Instant.now()));
        }
    }

    public void removeConsoleListener(String serverId, Consumer<ConsoleEvent> listener) {
        RunningServer active = running.get(serverId);
        if (active != null) {
            active.listeners().remove(listener);
        }
    }

    public void addMetricsListener(String serverId, Consumer<ServerSnapshot> listener) {
        RunningServer active = running.get(serverId);
        if (active != null) {
            active.metricListeners().add(Objects.requireNonNull(listener));
            listener.accept(snapshotOf(active));
        }
    }

    public void removeMetricsListener(String serverId, Consumer<ServerSnapshot> listener) {
        RunningServer active = running.get(serverId);
        if (active != null) {
            active.metricListeners().remove(listener);
        }
    }

    public Optional<ServerSnapshot> snapshot(String serverId) {
        RunningServer active = running.get(serverId);
        if (active == null) {
            String state = terminalStates.get(serverId);
            return state == null ? Optional.empty()
                    : Optional.of(new ServerSnapshot(serverId, false, state, Instant.now(), Duration.ZERO,
                    -1, 0, Duration.ZERO, List.of()));
        }
        return Optional.of(snapshotOf(active));
    }

    public List<InstalledServerContent> installedContent(ServerConfig server) throws IOException {
        Path directory = contentDirectory(server);
        if (!Files.isDirectory(directory)) {
            return List.of();
        }
        try (var files = Files.list(directory)) {
            return files.filter(Files::isRegularFile)
                    .map(path -> new InstalledServerContent(path.getFileName().toString(), path))
                    .sorted(java.util.Comparator.comparing(InstalledServerContent::filename,
                            String.CASE_INSENSITIVE_ORDER)).toList();
        }
    }

    public Path contentDirectory(ServerConfig server) throws IOException {
        ServerConfig stored = requireServer(server.id());
        Path root = serverDirectory(stored);
        Path directory = root.resolve(stored.distribution() == ServerDistribution.PAPER
                ? "plugins" : "mods").normalize();
        if (stored.distribution() == ServerDistribution.VANILLA) {
            throw new IOException("Install Paper for plugins or Fabric for mods.");
        }
        if (!directory.startsWith(root)) {
            throw new IOException("Server content path escapes the server directory.");
        }
        Files.createDirectories(directory);
        if (Files.isSymbolicLink(directory)) {
            throw new IOException("Refusing to use a symbolic link as the server content directory.");
        }
        return directory;
    }

    public void removeContent(ServerConfig server, InstalledServerContent content) throws IOException {
        Path directory = contentDirectory(server).toAbsolutePath().normalize();
        Path target = directory.resolve(content.filename()).normalize();
        if (!target.startsWith(directory) || Files.isSymbolicLink(target)) {
            throw new IOException("Refusing to remove an unsafe server content path.");
        }
        Files.deleteIfExists(target);
    }

    public void delete(ServerConfig server) throws IOException {
        ServerConfig stored = requireServer(server.id());
        RunningServer active = running.get(stored.id());
        if (active != null && active.process().isAlive()) {
            throw new IOException("Stop the server before deleting it.");
        }
        if (active != null) {
            running.remove(stored.id(), active);
        }
        deleteTree(serverRoot(stored.id()));
    }

    private void installVanilla(JsonObject metadata, Path serverDirectory,
                                Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        JsonObject downloads = object(metadata, "downloads");
        JsonObject server = object(downloads, "server");
        URI source = URI.create(requiredString(server, "url"));
        String sha1 = requiredString(server, "sha1");
        Path target = serverDirectory.resolve("server.jar");
        downloadManager.downloadAll(List.of(new DownloadTask(source, target, sha1, "Minecraft server JAR")), progress);
    }

    private void installPaper(String minecraftVersion, Path serverDirectory,
                              Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        String versionPath = encodePathSegment(minecraftVersion);
        JsonArray builds = fetchArray(PAPER_API.resolve("projects/paper/versions/" + versionPath + "/builds"));
        PaperBuild build = latestStablePaperBuild(builds);
        Path target = serverDirectory.resolve("server.jar");
        downloadManager.downloadAll(List.of(new DownloadTask(build.url(), target, "",
                "Paper " + minecraftVersion + " build " + build.id())), progress);
        if (!build.sha256().equals(sha256(target))) {
            Files.deleteIfExists(target);
            throw new IOException("Paper server JAR checksum verification failed.");
        }
    }

    static PaperBuild latestStablePaperBuild(JsonArray builds) throws IOException {
        PaperBuild latest = null;
        for (JsonElement element : builds) {
            if (!element.isJsonObject()) {
                throw new IOException("Paper returned an invalid build entry.");
            }
            JsonObject build = element.getAsJsonObject();
            if (!"STABLE".equals(requiredString(build, "channel"))) {
                continue;
            }
            int id;
            try {
                id = build.get("id").getAsInt();
            } catch (NullPointerException | NumberFormatException | IllegalStateException exception) {
                throw new IOException("Paper returned an invalid stable build number.", exception);
            }
            JsonObject download = object(object(build, "downloads"), "server:default");
            String filename = requiredString(download, "name");
            JsonObject checksums = object(download, "checksums");
            String checksum = requiredString(checksums, "sha256").toLowerCase(Locale.ROOT);
            URI url;
            try {
                url = URI.create(requiredString(download, "url"));
            } catch (IllegalArgumentException exception) {
                throw new IOException("Paper returned an invalid server download URL.", exception);
            }
            if (id < 1 || !filename.matches("[A-Za-z0-9._+-]+\\.jar")
                    || !checksum.matches("[0-9a-f]{64}") || !isPaperDownloadUrl(url)) {
                throw new IOException("Paper returned invalid stable build metadata.");
            }
            if (latest == null || id > latest.id()) {
                latest = new PaperBuild(id, filename, url, checksum);
            }
        }
        if (latest == null) {
            throw new IOException("Paper has no stable builds for this Minecraft version.");
        }
        return latest;
    }

    private static boolean isPaperDownloadUrl(URI uri) {
        return "https".equalsIgnoreCase(uri.getScheme())
                && "fill-data.papermc.io".equalsIgnoreCase(uri.getHost())
                && uri.getRawUserInfo() == null && (uri.getPort() == -1 || uri.getPort() == 443);
    }

    private void installFabric(String minecraftVersion, Path serverDirectory,
                               Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        JsonArray loaders = fetchArray(FABRIC_META.resolve("versions/loader/" + encodePathSegment(minecraftVersion)));
        JsonObject first = loaders.isEmpty() ? null : loaders.get(0).getAsJsonObject();
        String loaderVersion = first == null ? "" : requiredString(object(first, "loader"), "version");
        if (loaderVersion.isBlank()) {
            throw new IOException("Fabric has no server loader for Minecraft " + minecraftVersion + ".");
        }
        JsonArray installers = fetchArray(FABRIC_META.resolve("versions/installer"));
        JsonObject installer = installers.isEmpty() ? null : installers.get(0).getAsJsonObject();
        if (installer == null) {
            throw new IOException("Fabric did not return an installer version.");
        }
        String installerVersion = requiredString(installer, "version");
        URI installerJar = FABRIC_MAVEN.resolve("net/fabricmc/fabric-installer/" + installerVersion
                + "/fabric-installer-" + installerVersion + ".jar");
        String checksum = getText(URI.create(installerJar + ".sha1")).trim().split("\\s+")[0];
        if (!checksum.matches("(?i)[0-9a-f]{40}")) {
            throw new IOException("Fabric installer checksum is invalid.");
        }
        Files.createDirectories(installerCache);
        Path installerFile = installerCache.resolve("fabric-installer-" + installerVersion + ".jar");
        downloadManager.downloadAll(List.of(new DownloadTask(installerJar, installerFile, checksum,
                "Fabric server installer " + installerVersion)), progress);
        List<String> command = List.of(
                Path.of(System.getProperty("java.home"), "bin", executableName("java")).toString(),
                "-jar", installerFile.toString(), "server",
                "-mcversion", minecraftVersion, "-loader", loaderVersion, "-dir", serverDirectory.toString(),
                "-downloadMinecraft");
        Path installLog = serverDirectory.resolve("fabric-installer.log");
        Process installerProcess = new ProcessBuilder(command).directory(serverDirectory.toFile())
                .redirectErrorStream(true).redirectOutput(installLog.toFile()).start();
        try {
            if (!installerProcess.waitFor(5, TimeUnit.MINUTES)) {
                installerProcess.destroyForcibly();
                throw new IOException("Fabric server installer timed out. See " + installLog);
            }
        } catch (InterruptedException exception) {
            installerProcess.destroyForcibly();
            Thread.currentThread().interrupt();
            throw exception;
        }
        if (installerProcess.exitValue() != 0
                || !Files.isRegularFile(serverDirectory.resolve("fabric-server-launch.jar"))) {
            String output = Files.isRegularFile(installLog) ? Files.readString(installLog) : "";
            throw new IOException("Fabric server installation failed. See " + installLog + ": "
                    + output.lines().limit(80).reduce((left, right) -> left + System.lineSeparator() + right)
                    .orElse("No installer output."));
        }
    }

    private void readConsole(RunningServer active) {
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                active.process().getInputStream(), StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) {
                emit(active, line);
            }
        } catch (IOException exception) {
            emit(active, "Console stream failed: " + exception.getMessage());
        }
    }

    private void onExit(RunningServer active) {
        int exitCode;
        try {
            exitCode = active.process().exitValue();
        } catch (IllegalThreadStateException exception) {
            return;
        }
        active.exitCode = exitCode;
        terminalStates.put(active.config().id(), active.intentionalStop || exitCode == 0 ? "STOPPED" : "CRASHED");
        emit(active, "Server process exited with code " + exitCode + ".");
        publishSnapshot(active);
        running.remove(active.config().id(), active);
    }

    private void emit(RunningServer active, String line) {
        synchronized (active.console()) {
            if (active.console().size() == MAX_CONSOLE_LINES) {
                active.console().removeFirst();
            }
            active.console().addLast(line);
        }
        ConsoleEvent event = new ConsoleEvent(active.config().id(), line, Instant.now());
        active.listeners().forEach(listener -> listener.accept(event));
        if (isServerReadyMarker(line) && active.process().isAlive() && !active.ready) {
            active.ready = true;
            publishSnapshot(active);
        }
    }

    private static boolean isServerReadyMarker(String line) {
        return line.contains("Done (") || line.contains("For help, type \"help\"");
    }

    private void publishMetrics() {
        running.values().forEach(this::publishSnapshot);
    }

    private void publishSnapshot(RunningServer active) {
        ServerSnapshot snapshot = snapshotOf(active);
        active.metricListeners().forEach(listener -> listener.accept(snapshot));
    }

    private static ServerSnapshot snapshotOf(RunningServer active) {
        long processId = active.process().pid();
        long memoryBytes = processMemoryBytes(processId);
        Duration cpuDuration = active.process().toHandle().info().totalCpuDuration().orElse(Duration.ZERO);
        double cpuPercent = active.sampleCpu(cpuDuration.toNanos());
        String state = active.exitCode != null
                ? active.intentionalStop || active.exitCode == 0 ? "STOPPED" : "CRASHED"
                : active.ready ? "RUNNING" : "STARTING";
        return new ServerSnapshot(active.config().id(), active.process().isAlive(), state,
                active.startedAt(), Duration.between(active.startedAt(), Instant.now()),
                memoryBytes, cpuPercent, cpuDuration, active.config().javaFlags());
    }

    private static long processMemoryBytes(long pid) {
        Path procStatus = Path.of("/proc", Long.toString(pid), "status");
        if (Files.isRegularFile(procStatus)) {
            try {
                for (String line : Files.readAllLines(procStatus)) {
                    if (line.startsWith("VmRSS:")) {
                        String[] parts = line.trim().split("\\s+");
                        return Long.parseLong(parts[1]) * 1024;
                    }
                }
            } catch (IOException | NumberFormatException ignored) {
                return -1;
            }
        }
        if (System.getProperty("os.name", "").toLowerCase(Locale.ROOT).contains("win")) {
            try {
                Process process = new ProcessBuilder("tasklist", "/FI", "PID eq " + pid, "/FO", "CSV", "/NH")
                        .redirectErrorStream(true).start();
                String output;
                try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream(),
                        StandardCharsets.UTF_8))) {
                    output = reader.readLine();
                }
                if (process.waitFor(2, TimeUnit.SECONDS) && output != null) {
                    java.util.regex.Matcher matcher = Pattern.compile(
                            "^\"[^\"]*\",\"[^\"]*\",\"[^\"]*\",\"[^\"]*\",\"([0-9,]+)\\s*K\"$")
                            .matcher(output.trim());
                    if (matcher.matches()) {
                        return Long.parseLong(matcher.group(1).replace(",", "")) * 1024;
                    }
                }
            } catch (IOException | InterruptedException | NumberFormatException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
            }
        } else {
            try {
                Process process = new ProcessBuilder("ps", "-o", "rss=", "-p", Long.toString(pid))
                        .redirectErrorStream(true).start();
                String output;
                try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream(),
                        StandardCharsets.UTF_8))) {
                    output = reader.readLine();
                }
                if (process.waitFor(2, TimeUnit.SECONDS) && output != null && !output.isBlank()) {
                    return Long.parseLong(output.trim()) * 1024;
                }
            } catch (IOException | InterruptedException | NumberFormatException exception) {
                if (exception instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                }
            }
        }
        return -1;
    }

    private RunningServer requireRunning(String serverId) throws IOException {
        RunningServer active = running.get(serverId);
        if (active == null || !active.process().isAlive()) {
            throw new IOException("The selected server is not running.");
        }
        return active;
    }

    private ServerConfig requireServer(String id) throws IOException {
        if (id == null || !id.matches("[0-9a-fA-F-]{36}")) {
            throw new IOException("Invalid server identifier.");
        }
        Path root = serverRoot(id);
        Path config = root.resolve("server.json");
        if (!config.normalize().startsWith(serversDirectory) || Files.isSymbolicLink(config)
                || !Files.isRegularFile(config)) {
            throw new IOException("Server does not exist: " + id);
        }
        ServerConfig server = readConfig(config);
        if (!server.id().equals(id)) {
            throw new IOException("Server configuration identifier does not match its directory.");
        }
        return server;
    }

    private Path serverRoot(String id) throws IOException {
        if (id == null || !id.matches("[0-9a-fA-F-]{36}")) {
            throw new IOException("Invalid server identifier.");
        }
        Path root = serversDirectory.resolve(id).normalize();
        if (!root.startsWith(serversDirectory) || Files.isSymbolicLink(root)) {
            throw new IOException("Server path escapes the launcher data directory.");
        }
        return root;
    }

    private Path serverDirectory(ServerConfig server) throws IOException {
        Path root = serverRoot(server.id());
        Path directory = server.serverDirectory(root).normalize();
        if (!directory.startsWith(root) || Files.isSymbolicLink(directory)) {
            throw new IOException("Server directory escapes the launcher data directory.");
        }
        return directory;
    }

    public Path serverDirectoryPath(ServerConfig server) throws IOException {
        return serverDirectory(requireServer(server.id()));
    }

    public String readServerProperties(ServerConfig server) throws IOException {
        ServerConfig stored = requireServer(server.id());
        Path file = serverDirectory(stored).resolve("server.properties");
        if (Files.isSymbolicLink(file)) {
            throw new IOException("Server properties file must not be a symbolic link.");
        }
        return Files.isRegularFile(file) ? Files.readString(file) : "";
    }

    public ServerConfig saveServerProperties(ServerConfig server, String contents) throws IOException {
        ServerConfig stored = requireServer(server.id());
        if (snapshot(stored.id()).map(ServerSnapshot::running).orElse(false)) {
            throw new IOException("Stop the server before editing server.properties.");
        }
        if (contents == null || contents.length() > 1_000_000 || contents.indexOf('\0') >= 0) {
            throw new IllegalArgumentException("Server properties text is empty or too large to save safely.");
        }
        int viewDistance = stored.viewDistance();
        boolean onlineMode = stored.onlineMode();
        boolean hasViewDistance = false;
        boolean hasOnlineMode = false;
        for (String line : contents.lines().toList()) {
            String trimmed = line.trim();
            if (trimmed.isEmpty() || trimmed.startsWith("#")) {
                continue;
            }
            int separator = trimmed.indexOf('=');
            if (separator <= 0) {
                continue;
            }
            String key = trimmed.substring(0, separator).trim();
            String value = trimmed.substring(separator + 1).trim();
            if (key.equals("view-distance")) {
                if (hasViewDistance) {
                    throw new IllegalArgumentException("server.properties contains duplicate view-distance entries.");
                }
                try {
                    viewDistance = Integer.parseInt(value);
                } catch (NumberFormatException exception) {
                    throw new IllegalArgumentException("View distance must be a whole number between 2 and 32.",
                            exception);
                }
                requireViewDistance(viewDistance);
                hasViewDistance = true;
            } else if (key.equals("online-mode")) {
                if (hasOnlineMode || !(value.equalsIgnoreCase("true") || value.equalsIgnoreCase("false"))) {
                    throw new IllegalArgumentException("online-mode must appear once and be true or false.");
                }
                onlineMode = Boolean.parseBoolean(value);
                hasOnlineMode = true;
            }
        }
        Path file = serverDirectory(stored).resolve("server.properties");
        if (Files.isSymbolicLink(file)) {
            throw new IOException("Server properties file must not be a symbolic link.");
        }
        writeTextAtomically(file, contents);
        ServerConfig updated = new ServerConfig(stored.id(), stored.name(), stored.minecraftVersion(),
                stored.distribution(), stored.installed(), stored.eulaAccepted(), stored.javaFlags(),
                stored.createdAt(), viewDistance, onlineMode, stored.useManagedJavaRuntime(),
                stored.javaRuntimeComponent());
        writeConfig(updated, serverRoot(stored.id()));
        return updated;
    }

    private static void writeTextAtomically(Path target, String contents) throws IOException {
        Path temporary = Files.createTempFile(target.getParent(), target.getFileName().toString(), ".tmp");
        try {
            Files.writeString(temporary, contents);
            try {
                Files.move(temporary, target, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
            } catch (java.nio.file.AtomicMoveNotSupportedException exception) {
                Files.move(temporary, target, StandardCopyOption.REPLACE_EXISTING);
            }
        } finally {
            Files.deleteIfExists(temporary);
        }
    }

    private void writeServerProperties(ServerConfig server) throws IOException {
        Path file = serverDirectory(server).resolve("server.properties");
        if (Files.isSymbolicLink(file)) {
            throw new IOException("Server properties file must not be a symbolic link.");
        }
        List<String> lines = Files.exists(file) ? Files.readAllLines(file) : new java.util.ArrayList<>();
        boolean hasDistance = false;
        boolean hasOnlineMode = false;
        List<String> updated = new java.util.ArrayList<>(lines.size() + 2);
        for (String line : lines) {
            if (line.startsWith("view-distance=")) {
                updated.add("view-distance=" + server.viewDistance());
                hasDistance = true;
            } else if (line.startsWith("online-mode=")) {
                updated.add("online-mode=" + server.onlineMode());
                hasOnlineMode = true;
            } else {
                updated.add(line);
            }
        }
        if (!hasDistance) {
            updated.add("view-distance=" + server.viewDistance());
        }
        if (!hasOnlineMode) {
            updated.add("online-mode=" + server.onlineMode());
        }
        Files.write(file, updated);
    }

    private static int requireViewDistance(int viewDistance) {
        if (viewDistance < 2 || viewDistance > 32) {
            throw new IllegalArgumentException("View distance must be between 2 and 32 chunks.");
        }
        return viewDistance;
    }

    private static ServerConfig readConfig(Path path) throws IOException {
        try {
            JsonObject stored = JsonParser.parseString(Files.readString(path)).getAsJsonObject();
            String id = requiredConfigString(stored, "id");
            String name = requiredConfigString(stored, "name");
            String minecraftVersion = requiredConfigString(stored, "minecraftVersion");
            String createdAt = requiredConfigString(stored, "createdAt");
            ServerDistribution distribution = ServerDistribution.valueOf(requiredConfigString(stored, "distribution"));
            if (id.isBlank() || name.isBlank() || minecraftVersion.isBlank() || createdAt.isBlank()) {
                throw new IOException("Server configuration is incomplete: " + path);
            }
            requireName(name);
            requireVersion(minecraftVersion);
            boolean installed = optionalBoolean(stored, "installed", false);
            boolean eulaAccepted = optionalBoolean(stored, "eulaAccepted", false);
            int viewDistance = optionalInteger(stored, "viewDistance", 10);
            requireViewDistance(viewDistance);
            boolean onlineMode = optionalBoolean(stored, "onlineMode", true);
            boolean useManagedJavaRuntime = optionalBoolean(stored, "useManagedJavaRuntime", false);
            String javaRuntimeComponent = optionalString(stored, "javaRuntimeComponent");
            List<String> javaFlags = stored.has("javaFlags") && stored.get("javaFlags").isJsonArray()
                    ? JSON.gson.fromJson(stored.getAsJsonArray("javaFlags"), new com.google.gson.reflect.TypeToken<
                    List<String>>() { }.getType()) : List.of();
            InstanceSettings.validateFlags(javaFlags);
            return new ServerConfig(id, name, minecraftVersion, distribution, installed, eulaAccepted,
                    javaFlags, createdAt, viewDistance, onlineMode, useManagedJavaRuntime, javaRuntimeComponent);
        } catch (JsonParseException | IllegalStateException | IllegalArgumentException exception) {
            throw new IOException("Server configuration is invalid: " + path, exception);
        }
    }

    private static String requiredConfigString(JsonObject object, String key) throws IOException {
        String value = optionalString(object, key);
        if (value == null) {
            throw new IOException("Server configuration is missing " + key + ".");
        }
        return value;
    }

    private static String optionalString(JsonObject object, String key) {
        JsonElement value = object.get(key);
        return value != null && value.isJsonPrimitive() && value.getAsJsonPrimitive().isString()
                ? value.getAsString() : null;
    }

    private static boolean optionalBoolean(JsonObject object, String key, boolean fallback) {
        JsonElement value = object.get(key);
        return value != null && value.isJsonPrimitive() && value.getAsJsonPrimitive().isBoolean()
                ? value.getAsBoolean() : fallback;
    }

    private static int optionalInteger(JsonObject object, String key, int fallback) {
        JsonElement value = object.get(key);
        return value != null && value.isJsonPrimitive() && value.getAsJsonPrimitive().isNumber()
                ? value.getAsInt() : fallback;
    }

    private static void writeConfig(ServerConfig config, Path root) throws IOException {
        Files.createDirectories(root);
        Path target = root.resolve("server.json");
        Path temporary = Files.createTempFile(root, "server-", ".tmp");
        try {
            Files.writeString(temporary, JSON.gson.toJson(config));
            try {
                Files.move(temporary, target, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
            } catch (java.nio.file.AtomicMoveNotSupportedException exception) {
                Files.move(temporary, target, StandardCopyOption.REPLACE_EXISTING);
            }
        } finally {
            Files.deleteIfExists(temporary);
        }
    }

    private JsonObject fetchJson(URI uri) throws IOException, InterruptedException {
        String body = getText(uri);
        try {
            return JsonParser.parseString(body).getAsJsonObject();
        } catch (JsonParseException | IllegalStateException exception) {
            throw new IOException("Could not parse server metadata from " + uri.getHost() + ".", exception);
        }
    }

    private JsonArray fetchArray(URI uri) throws IOException, InterruptedException {
        String body = getText(uri);
        try {
            return JsonParser.parseString(body).getAsJsonArray();
        } catch (JsonParseException | IllegalStateException exception) {
            throw new IOException("Could not parse server metadata from " + uri.getHost() + ".", exception);
        }
    }

    private String getText(URI uri) throws IOException, InterruptedException {
        String host = uri.getHost();
        if (!"https".equalsIgnoreCase(uri.getScheme()) || host == null
                || !(host.equals("mojang.com") || host.endsWith(".mojang.com")
                || host.equals("fabricmc.net") || host.endsWith(".fabricmc.net")
                || host.equals("papermc.io") || host.endsWith(".papermc.io")
                || host.equals("fill.papermc.io"))) {
            throw new IOException("Server metadata must be fetched from official Mojang, Fabric, or Paper HTTPS hosts.");
        }
        HttpRequest request = HttpRequest.newBuilder(uri).timeout(Duration.ofSeconds(30))
                .header("Accept", "application/json")
                .header("User-Agent", "StrataLauncher/0.1.0")
                .GET().build();
        HttpResponse<String> response = httpClient.send(request,
                HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));
        if (response.statusCode() < 200 || response.statusCode() >= 300) {
            throw new IOException("Server metadata request to " + host + " failed with HTTP "
                    + response.statusCode() + ".");
        }
        return response.body();
    }

    private static JsonObject object(JsonObject object, String key) throws IOException {
        JsonElement element = object == null ? null : object.get(key);
        if (element == null || !element.isJsonObject()) {
            throw new IOException("Server metadata is missing " + key + ".");
        }
        return element.getAsJsonObject();
    }

    private static JsonArray array(JsonObject object, String key) throws IOException {
        JsonElement element = object == null ? null : object.get(key);
        if (element == null || !element.isJsonArray()) {
            throw new IOException("Server metadata is missing " + key + ".");
        }
        return element.getAsJsonArray();
    }

    private static String requiredString(JsonObject object, String key) throws IOException {
        JsonElement element = object == null ? null : object.get(key);
        if (element == null || !element.isJsonPrimitive() || element.getAsString().isBlank()) {
            throw new IOException("Server metadata is missing " + key + ".");
        }
        return element.getAsString();
    }

    private static String encodePathSegment(String value) {
        return java.net.URLEncoder.encode(value, StandardCharsets.UTF_8).replace("+", "%20");
    }

    private static String requireName(String value) {
        if (value == null || value.isBlank() || !SAFE_NAME.matcher(value.trim()).matches()
                || value.trim().length() > 80 || value.trim().equals(".") || value.trim().equals("..")) {
            throw new IllegalArgumentException("Server name must be 1-80 characters and cannot contain path characters.");
        }
        return value.trim();
    }

    private static String requireVersion(String value) {
        if (value == null || !SAFE_VERSION.matcher(value.trim()).matches()) {
            throw new IllegalArgumentException("Minecraft server version contains unsupported characters.");
        }
        return value.trim();
    }

    private static String executableName(String executable) {
        return System.getProperty("os.name", "").toLowerCase(Locale.ROOT).contains("win")
                ? executable + ".exe" : executable;
    }

    private static String sha256(Path file) throws IOException {
        try {
            var digest = java.security.MessageDigest.getInstance("SHA-256");
            try (var input = Files.newInputStream(file)) {
                byte[] buffer = new byte[8192];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    digest.update(buffer, 0, count);
                }
            }
            return java.util.HexFormat.of().formatHex(digest.digest());
        } catch (java.security.NoSuchAlgorithmException exception) {
            throw new IOException("SHA-256 is not available.", exception);
        }
    }

    private static void deleteTree(Path target) throws IOException {
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

    @Override
    public void close() {
        running.values().forEach(active -> {
            if (active.process().isAlive()) {
                try {
                    active.input().println("stop");
                    if (!active.process().waitFor(5, TimeUnit.SECONDS)) {
                        active.process().destroy();
                        if (!active.process().waitFor(2, TimeUnit.SECONDS)) {
                            active.process().destroyForcibly();
                        }
                    }
                } catch (InterruptedException exception) {
                    active.process().destroyForcibly();
                    Thread.currentThread().interrupt();
                }
            }
        });
        readers.shutdownNow();
        metrics.shutdownNow();
    }

    public record ServerConfig(String id, String name, String minecraftVersion, ServerDistribution distribution,
                               boolean installed, boolean eulaAccepted, List<String> javaFlags, String createdAt,
                               int viewDistance, boolean onlineMode, boolean useManagedJavaRuntime,
                               String javaRuntimeComponent) {
        public ServerConfig(String id, String name, String minecraftVersion, ServerDistribution distribution,
                            boolean installed, boolean eulaAccepted, List<String> javaFlags, String createdAt) {
            this(id, name, minecraftVersion, distribution, installed, eulaAccepted, javaFlags, createdAt,
                    10, true, false, null);
        }

        public ServerConfig(String id, String name, String minecraftVersion, ServerDistribution distribution,
                            boolean installed, boolean eulaAccepted, List<String> javaFlags, String createdAt,
                            int viewDistance, boolean onlineMode) {
            this(id, name, minecraftVersion, distribution, installed, eulaAccepted, javaFlags, createdAt,
                    viewDistance, onlineMode, false, null);
        }

        public ServerConfig {
            javaFlags = javaFlags == null ? List.of() : List.copyOf(javaFlags);
            requireViewDistance(viewDistance);
        }

        public Path serverDirectory(Path serverRoot) {
            return serverRoot.resolve("server");
        }
    }

    public record ConsoleEvent(String serverId, String line, Instant timestamp) {
    }

    public record ServerSnapshot(String serverId, boolean running, String state, Instant startedAt,
                                 Duration uptime, long residentMemoryBytes, double cpuPercent,
                                 Duration totalCpuTime, List<String> javaFlags) {
        public ServerSnapshot {
            javaFlags = List.copyOf(javaFlags);
        }
    }

    public record InstalledServerContent(String filename, Path path) {
    }

    record PaperBuild(int id, String filename, URI url, String sha256) {
    }

    private static final class RunningServer {
        private final ServerConfig config;
        private final Process process;
        private final Instant startedAt;
        private final PrintWriter input;
        private final ArrayDeque<String> console = new ArrayDeque<>();
        private final CopyOnWriteArrayList<Consumer<ConsoleEvent>> listeners = new CopyOnWriteArrayList<>();
        private final CopyOnWriteArrayList<Consumer<ServerSnapshot>> metricListeners = new CopyOnWriteArrayList<>();
        private long previousCpuNanos;
        private long previousSampleNanos = System.nanoTime();
        private double cpuPercent;
        private volatile boolean ready;
        private volatile Integer exitCode;
        private volatile boolean intentionalStop;

        private RunningServer(ServerConfig config, Process process, Instant startedAt) {
            this.config = config;
            this.process = process;
            this.startedAt = startedAt;
            input = new PrintWriter(new OutputStreamWriter(process.getOutputStream(), StandardCharsets.UTF_8), true);
        }

        private ServerConfig config() {
            return config;
        }

        private Process process() {
            return process;
        }

        private Instant startedAt() {
            return startedAt;
        }

        private PrintWriter input() {
            return input;
        }

        private ArrayDeque<String> console() {
            return console;
        }

        private CopyOnWriteArrayList<Consumer<ConsoleEvent>> listeners() {
            return listeners;
        }

        private CopyOnWriteArrayList<Consumer<ServerSnapshot>> metricListeners() {
            return metricListeners;
        }

        private synchronized double sampleCpu(long cpuNanos) {
            long now = System.nanoTime();
            long elapsed = now - previousSampleNanos;
            long used = Math.max(0, cpuNanos - previousCpuNanos);
            previousSampleNanos = now;
            previousCpuNanos = cpuNanos;
            cpuPercent = elapsed <= 0 ? 0 : Math.min(100.0,
                    (double) used * 100.0 / elapsed / Math.max(1,
                            Runtime.getRuntime().availableProcessors()));
            return cpuPercent;
        }
    }

    private static final class GsonHolder {
        private final com.google.gson.Gson gson = new com.google.gson.GsonBuilder().setPrettyPrinting().create();
    }
}
