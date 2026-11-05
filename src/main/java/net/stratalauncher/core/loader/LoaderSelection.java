package net.stratalauncher.core.loader;

import java.util.Objects;

public record LoaderSelection(LoaderType type, String version) {
    public LoaderSelection {
        Objects.requireNonNull(type, "type");
        version = version == null ? "" : version.trim();
        if (type == LoaderType.VANILLA && !version.isEmpty()) {
            throw new IllegalArgumentException("Vanilla instances do not have a loader version.");
        }
        if (type != LoaderType.VANILLA && version.isBlank()) {
            throw new IllegalArgumentException("Choose a loader version.");
        }
    }

    public static LoaderSelection vanilla() {
        return new LoaderSelection(LoaderType.VANILLA, "");
    }
}
