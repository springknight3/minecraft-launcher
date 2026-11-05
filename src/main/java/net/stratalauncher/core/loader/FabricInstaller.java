package net.stratalauncher.core.loader;

import net.stratalauncher.core.download.DownloadManager;

import java.net.URI;
import java.nio.file.Path;
import java.util.List;
import java.util.function.Consumer;

public final class FabricInstaller {
    private final ProfileBasedInstaller installer;

    FabricInstaller(LoaderMetadataClient metadataClient, DownloadManager downloadManager, Path librariesDirectory) {
        installer = new ProfileBasedInstaller(LoaderType.FABRIC, "https://meta.fabricmc.net/v2",
                URI.create("https://maven.fabricmc.net/"), metadataClient, downloadManager, librariesDirectory) { };
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
