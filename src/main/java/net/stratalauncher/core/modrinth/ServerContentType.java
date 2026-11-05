package net.stratalauncher.core.modrinth;

public enum ServerContentType {
    MOD("mod"),
    PLUGIN("plugin");

    private final String projectType;

    ServerContentType(String projectType) {
        this.projectType = projectType;
    }

    public String projectType() {
        return projectType;
    }
}
