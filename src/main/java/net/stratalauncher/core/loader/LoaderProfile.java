package net.stratalauncher.core.loader;

import com.google.gson.JsonObject;

import java.util.Objects;

public record LoaderProfile(LoaderType type, String minecraftVersion, String loaderVersion,
                            String profileVersionId, JsonObject launchMetadata) {
    public LoaderProfile {
        Objects.requireNonNull(type);
        Objects.requireNonNull(minecraftVersion);
        Objects.requireNonNull(loaderVersion);
        Objects.requireNonNull(profileVersionId);
        launchMetadata = Objects.requireNonNull(launchMetadata).deepCopy();
    }

    @Override
    public JsonObject launchMetadata() {
        return launchMetadata.deepCopy();
    }
}
