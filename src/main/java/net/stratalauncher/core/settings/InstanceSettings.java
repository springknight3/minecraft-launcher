package net.stratalauncher.core.settings;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonParseException;
import net.stratalauncher.core.instance.Instance;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class InstanceSettings {
    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();
    private static final Pattern GC_COLLECTOR = Pattern.compile("-XX:([+-])(Use[A-Za-z0-9]+GC)");

    private final Path settingsFile;
    private List<String> javaLaunchFlagsOverride;
    private JavaRuntimeMode javaRuntimeMode = JavaRuntimeMode.SYSTEM;
    private String javaRuntimeComponent;

    public InstanceSettings(Instance instance) throws IOException {
        Objects.requireNonNull(instance);
        settingsFile = instance.directory().toAbsolutePath().normalize().resolve("settings.json");
        if (!Files.isRegularFile(settingsFile)) {
            return;
        }
        try {
            SettingsData data = GSON.fromJson(Files.readString(settingsFile), SettingsData.class);
            if (data != null) {
                javaLaunchFlagsOverride = data.javaLaunchFlagsOverride == null
                        ? null : validateFlags(data.javaLaunchFlagsOverride);
                if (data.javaRuntimeMode != null) {
                    javaRuntimeMode = JavaRuntimeMode.valueOf(data.javaRuntimeMode);
                }
                javaRuntimeComponent = data.javaRuntimeComponent;
            }
        } catch (JsonParseException exception) {
            throw new IOException("Instance settings are invalid: " + settingsFile, exception);
        } catch (IllegalArgumentException exception) {
            throw new IOException("Instance settings contain invalid values: " + settingsFile, exception);
        }
    }

    public synchronized boolean hasJavaLaunchFlagsOverride() {
        return javaLaunchFlagsOverride != null;
    }

    public synchronized List<String> javaLaunchFlagsOverride() {
        return javaLaunchFlagsOverride == null ? List.of() : List.copyOf(javaLaunchFlagsOverride);
    }

    public synchronized List<String> effectiveJavaLaunchFlags(List<String> globalFlags) {
        return javaLaunchFlagsOverride == null ? List.copyOf(globalFlags) : List.copyOf(javaLaunchFlagsOverride);
    }

    public synchronized void setJavaLaunchFlagsOverride(List<String> flags) throws IOException {
        List<String> normalized = validateFlags(flags);
        persist(normalized, javaRuntimeMode, javaRuntimeComponent);
        javaLaunchFlagsOverride = normalized;
    }

    public synchronized void clearJavaLaunchFlagsOverride() throws IOException {
        persist(null, javaRuntimeMode, javaRuntimeComponent);
        javaLaunchFlagsOverride = null;
    }

    public synchronized JavaRuntimeMode javaRuntimeMode() {
        return javaRuntimeMode;
    }

    public synchronized void setJavaRuntimeMode(JavaRuntimeMode mode) throws IOException {
        JavaRuntimeMode normalized = Objects.requireNonNull(mode, "Java runtime mode");
        persist(javaLaunchFlagsOverride, normalized, javaRuntimeComponent);
        javaRuntimeMode = normalized;
    }

    public synchronized String javaRuntimeComponent() {
        return javaRuntimeComponent;
    }

    public synchronized void setJavaRuntimeComponent(String component) throws IOException {
        if (component != null && !component.matches("[a-z0-9-]+")) {
            throw new IllegalArgumentException("Invalid managed Java runtime identifier.");
        }
        persist(javaLaunchFlagsOverride, javaRuntimeMode, component);
        javaRuntimeComponent = component;
    }

    private void persist(List<String> flags, JavaRuntimeMode mode, String runtimeComponent) throws IOException {
        Files.createDirectories(settingsFile.getParent());
        Path temporary = Files.createTempFile(settingsFile.getParent(), "instance-settings-", ".tmp");
        try {
            Files.writeString(temporary, GSON.toJson(new SettingsData(flags, mode.name(), runtimeComponent)));
            try {
                Files.move(temporary, settingsFile, StandardCopyOption.ATOMIC_MOVE,
                        StandardCopyOption.REPLACE_EXISTING);
            } catch (java.nio.file.AtomicMoveNotSupportedException exception) {
                Files.move(temporary, settingsFile, StandardCopyOption.REPLACE_EXISTING);
            }
        } finally {
            Files.deleteIfExists(temporary);
        }
    }

    public static List<String> validateFlags(List<String> flags) {
        Objects.requireNonNull(flags, "Java launch flags");
        List<String> normalized = new ArrayList<>();
        for (String flag : flags) {
            if (flag == null || flag.isBlank()) {
                continue;
            }
            String value = flag.trim();
            if (!value.startsWith("-") || value.startsWith("@")
                    || value.equals("-cp") || value.equals("-classpath")
                    || value.equals("--class-path") || value.equals("-jar")
                    || value.indexOf('\n') >= 0 || value.indexOf('\r') >= 0 || value.indexOf('\0') >= 0) {
                throw new IllegalArgumentException("Each Java launch flag must be one JVM option beginning with '-'.");
            }
            normalized.add(value);
        }
        long enabledCollectors = normalized.stream()
                .map(GC_COLLECTOR::matcher)
                .filter(Matcher::matches)
                .filter(matcher -> matcher.group(1).equals("+"))
                .map(matcher -> matcher.group(2))
                .distinct()
                .count();
        if (enabledCollectors > 1) {
            throw new IllegalArgumentException("Select only one Java garbage collector in the launch flags.");
        }
        return List.copyOf(normalized);
    }

    public enum JavaRuntimeMode {
        SYSTEM,
        MANAGED
    }

    private record SettingsData(List<String> javaLaunchFlagsOverride, String javaRuntimeMode,
                                String javaRuntimeComponent) {
    }
}
