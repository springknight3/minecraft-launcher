package net.stratalauncher.core.server;

import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.download.DownloadTask;
import net.stratalauncher.core.modrinth.ContentManager;
import net.stratalauncher.core.modrinth.ModrinthClient;
import net.stratalauncher.core.modrinth.ServerContentType;

import java.io.IOException;
import java.net.URI;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Objects;
import java.util.function.Consumer;

public final class ServerModrinthManager {
    private final ModrinthClient modrinthClient;
    private final DownloadManager downloadManager;
    private final DevServerManager serverManager;

    public ServerModrinthManager(ModrinthClient modrinthClient, DownloadManager downloadManager,
                                 DevServerManager serverManager) {
        this.modrinthClient = Objects.requireNonNull(modrinthClient);
        this.downloadManager = Objects.requireNonNull(downloadManager);
        this.serverManager = Objects.requireNonNull(serverManager);
    }

    public ModrinthClient.SearchPage search(DevServerManager.ServerConfig server, String query,
                                            int offset, int limit)
            throws IOException, InterruptedException {
        return modrinthClient.searchServer(contentType(server), query, server.minecraftVersion(), offset, limit);
    }

    public List<ModrinthClient.ProjectVersion> versions(DevServerManager.ServerConfig server,
                                                        ModrinthClient.Project project)
            throws IOException, InterruptedException {
        ServerContentType contentType = contentType(server);
        if (!project.projectType().equals(contentType.projectType())) {
            throw new IOException("Selected Modrinth project does not match this server distribution.");
        }
        return modrinthClient.serverVersions(project.id(), server.minecraftVersion(), contentType);
    }

    public Path install(DevServerManager.ServerConfig server, ModrinthClient.Project project,
                        ModrinthClient.ProjectVersion version, Consumer<DownloadManager.Progress> progress)
            throws IOException, InterruptedException {
        ServerContentType type = contentType(server);
        if (!project.id().equals(version.projectId()) || !project.projectType().equals(type.projectType())
                || !version.projectType().equals(type.projectType())
                || !version.gameVersions().contains(server.minecraftVersion())) {
            throw new IOException("Selected Modrinth version is incompatible with this server.");
        }
        ModrinthClient.VersionFile file = version.primaryFile();
        ContentManager.validateFile(file);
        Path directory = serverManager.contentDirectory(server).toAbsolutePath().normalize();
        Path target = directory.resolve(file.filename()).normalize();
        if (!target.startsWith(directory) || Files.exists(target)) {
            throw new IOException("A server content file with this name already exists.");
        }
        downloadManager.downloadAll(List.of(new DownloadTask(URI.create(file.url()), target,
                file.sha1(), project.title() + " " + version.versionNumber())), progress);
        return target;
    }

    public List<DevServerManager.InstalledServerContent> installed(DevServerManager.ServerConfig server)
            throws IOException {
        return serverManager.installedContent(server);
    }

    public void remove(DevServerManager.ServerConfig server, DevServerManager.InstalledServerContent content)
            throws IOException {
        serverManager.removeContent(server, content);
    }

    private static ServerContentType contentType(DevServerManager.ServerConfig server) throws IOException {
        return switch (server.distribution()) {
            case FABRIC -> ServerContentType.MOD;
            case PAPER -> ServerContentType.PLUGIN;
            case VANILLA -> throw new IOException("Install Fabric for mods or Paper for plugins.");
        };
    }
}
