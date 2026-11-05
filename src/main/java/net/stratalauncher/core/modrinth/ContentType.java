package net.stratalauncher.core.modrinth;

import java.nio.file.Path;

public enum ContentType {
    MOD("mod", "Mods", "mods"),
    RESOURCE_PACK("resourcepack", "Resource packs", "resourcepacks"),
    DATA_PACK("datapack", "Data packs", "datapacks"),
    SHADER("shader", "Shader packs", "shaderpacks");

    private final String projectType;
    private final String displayName;
    private final String directoryName;

    ContentType(String projectType, String displayName, String directoryName) {
        this.projectType = projectType;
        this.displayName = displayName;
        this.directoryName = directoryName;
    }

    public String projectType() {
        return projectType;
    }

    public Path directory(Path gameDirectory) {
        return gameDirectory.resolve(directoryName);
    }

    public Path directory(Path gameDirectory, String worldName) {
        if (this != DATA_PACK) {
            return directory(gameDirectory);
        }
        if (worldName == null || worldName.isBlank()) {
            throw new IllegalArgumentException("Select a world before installing a data pack.");
        }
        return gameDirectory.resolve("saves").resolve(worldName).resolve(directoryName);
    }

    @Override
    public String toString() {
        return displayName;
    }
}
