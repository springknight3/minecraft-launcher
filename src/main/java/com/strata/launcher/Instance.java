package com.strata.launcher;

import java.io.Serializable;

public class Instance implements Serializable {

    private static final long serialVersionUID = 1L;

    private String name;
    private String version;
    private String username = "Player";
    private int maxMemory = 2048;
    private String javaPath = System.getProperty("java.home") + "/bin/java";
    private boolean separateJvm = true;
    private boolean downloaded = false;

    public Instance(String name, String version) {
        this.name = name;
        this.version = version;
    }

    public String getName() { return name; }
    public void setName(String name) { this.name = name; }

    public String getVersion() { return version; }
    public void setVersion(String version) { this.version = version; }

    public String getUsername() { return username; }
    public void setUsername(String username) { this.username = username; }

    public int getMaxMemory() { return maxMemory; }
    public void setMaxMemory(int maxMemory) { this.maxMemory = maxMemory; }

    public String getJavaPath() { return javaPath; }
    public void setJavaPath(String javaPath) { this.javaPath = javaPath; }

    public boolean isSeparateJvm() { return separateJvm; }
    public void setSeparateJvm(boolean separateJvm) { this.separateJvm = separateJvm; }

    public boolean isDownloaded() { return downloaded; }
    public void setDownloaded(boolean downloaded) { this.downloaded = downloaded; }
}
