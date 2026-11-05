package net.stratalauncher.core.auth;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import com.google.gson.JsonParser;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

public final class VSyncConfiguration {
    private VSyncConfiguration() {
    }

    public static Boolean readConfiguredValue(Path settingsFile) throws IOException {
        if (!Files.isRegularFile(settingsFile)) {
            return null;
        }
        try {
            JsonElement root = JsonParser.parseString(Files.readString(settingsFile));
            if (!root.isJsonObject()) {
                throw new IOException("Launcher settings must contain a JSON object.");
            }
            JsonElement value = root.getAsJsonObject().get("vsyncEnabled");
            if (value == null || value.isJsonNull()) {
                return null;
            }
            if (!value.isJsonPrimitive() || !value.getAsJsonPrimitive().isBoolean()) {
                throw new IOException("The VSync setting must be true or false.");
            }
            return value.getAsBoolean();
        } catch (JsonParseException exception) {
            throw new IOException("Launcher settings contain invalid JSON: " + settingsFile, exception);
        }
    }

    public static Resolution resolve(boolean linuxDefault, Boolean configuredValue, String explicitOverride) {
        if (explicitOverride != null) {
            return new Resolution(Boolean.parseBoolean(explicitOverride), Source.COMMAND_LINE_OVERRIDE);
        }
        if (configuredValue != null) {
            return new Resolution(configuredValue, Source.CONFIG);
        }
        return new Resolution(!linuxDefault, Source.DEFAULT);
    }

    public record Resolution(boolean enabled, Source source) {
    }

    public enum Source {
        CONFIG("config"),
        DEFAULT("default"),
        COMMAND_LINE_OVERRIDE("command-line override");

        private final String label;

        Source(String label) {
            this.label = label;
        }

        public String label() {
            return label;
        }
    }
}
