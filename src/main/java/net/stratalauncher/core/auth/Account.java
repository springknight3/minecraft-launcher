package net.stratalauncher.core.auth;

import java.util.Objects;

public record Account(String id, String name, String uuid, Type type, String xuid) {
    public Account {
        Objects.requireNonNull(id, "id");
        Objects.requireNonNull(name, "name");
        Objects.requireNonNull(uuid, "uuid");
        Objects.requireNonNull(type, "type");
        xuid = xuid == null ? "" : xuid;
    }

    public enum Type {
        OFFLINE,
        MICROSOFT
    }
}
