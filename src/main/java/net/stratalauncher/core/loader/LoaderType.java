package net.stratalauncher.core.loader;

public enum LoaderType {
    VANILLA("Vanilla"),
    FABRIC("Fabric"),
    QUILT("Quilt"),
    FORGE("Forge"),
    NEOFORGE("NeoForge");

    private final String displayName;

    LoaderType(String displayName) {
        this.displayName = displayName;
    }

    @Override
    public String toString() {
        return displayName;
    }
}
