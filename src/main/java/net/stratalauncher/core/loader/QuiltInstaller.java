package net.stratalauncher.core.loader;

import net.stratalauncher.core.download.DownloadManager;

import java.net.URI;
import java.nio.file.Path;
import java.util.List;
import java.util.function.Consumer;

public final class QuiltInstaller {
    private final ProfileBasedInstaller installer;

    QuiltInstaller(LoaderMetadataClient metadataClient, DownloadManager downloadManager, Path librariesDirectory) {
        installer = new ProfileBasedInstaller(LoaderType.QUILT, "https://meta.quiltmc.org/v3",
                URI.create("https://maven.quiltmc.org/repository/release/"),
                metadataClient, downloadManager, librariesDirectory) { };
    }

    public List<String> versions(String minecraftVersion) throws java.io.IOException, InterruptedException {
        return installer.versions(minecraftVersion);
    }

    LoaderProfile install(String minecraftVersion, String loaderVersion,
                          Consumer<DownloadManager.Progress> progress)
            throws java.io.IOException, InterruptedException {
        return installer.install(minecraftVersion, loaderVersion, progress);
    }
}
