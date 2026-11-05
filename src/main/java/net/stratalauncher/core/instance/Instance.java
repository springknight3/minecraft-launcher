package net.stratalauncher.core.instance;

import java.nio.file.Path;
import java.time.Instant;

public record Instance(String id, String name, String versionId, Instant createdAt, Path directory, boolean favorite) {
    public Instance(String id, String name, String versionId, Instant createdAt, Path directory) {
        this(id, name, versionId, createdAt, directory, false);
    }

    public Path gameDirectory() {
        return directory.resolve("game");
    }
}
