package net.stratalauncher.core.mojang;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import com.google.gson.JsonParser;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;
import java.time.Instant;
import java.util.ArrayList;
import java.util.List;

public class MojangManifestClient {
    public static final URI MANIFEST_URI = URI.create("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");

    private final HttpClient httpClient;
    private final URI manifestUri;

    public MojangManifestClient() {
        this(HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(15)).build(), MANIFEST_URI);
    }

    public MojangManifestClient(HttpClient httpClient, URI manifestUri) {
        this.httpClient = httpClient;
        this.manifestUri = manifestUri;
    }

    public VersionManifest fetchManifest() throws IOException, InterruptedException {
        HttpRequest request = HttpRequest.newBuilder(manifestUri)
                .timeout(Duration.ofSeconds(30))
                .header("Accept", "application/json")
                .header("User-Agent", "StrataLauncher/0.1.0")
                .GET()
                .build();
        HttpResponse<String> response = httpClient.send(request, HttpResponse.BodyHandlers.ofString());
        if (response.statusCode() < 200 || response.statusCode() >= 300) {
            throw new IOException("Mojang version manifest request failed with HTTP " + response.statusCode());
        }

        return parseManifest(response.body());
    }

    static VersionManifest parseManifest(String json) throws IOException {
        try {
            JsonObject root = JsonParser.parseString(json).getAsJsonObject();
            JsonObject latestJson = root.getAsJsonObject("latest");
            String latestRelease = requiredString(latestJson, "release");
            String latestSnapshot = requiredString(latestJson, "snapshot");
            JsonArray versionsJson = root.getAsJsonArray("versions");
            if (versionsJson == null) {
                throw new IOException("Mojang version manifest has no versions list");
            }

            List<Version> versions = new ArrayList<>();
            for (JsonElement element : versionsJson) {
                JsonObject item = element.getAsJsonObject();
                versions.add(new Version(
                        requiredString(item, "id"),
                        requiredString(item, "type"),
                        URI.create(requiredString(item, "url")),
                        Instant.parse(requiredString(item, "releaseTime")),
                        item.has("sha1") ? item.get("sha1").getAsString() : ""));
            }
            if (versions.stream().noneMatch(version -> version.id().equals(latestRelease))) {
                throw new IOException("Latest release is missing from the Mojang version list");
            }
            return new VersionManifest(latestRelease, latestSnapshot, List.copyOf(versions));
        } catch (JsonParseException | IllegalStateException | IllegalArgumentException exception) {
            throw new IOException("Could not parse Mojang version manifest", exception);
        }
    }

    private static String requiredString(JsonObject object, String key) throws IOException {
        JsonElement element = object == null ? null : object.get(key);
        if (element == null || element.isJsonNull() || !element.isJsonPrimitive()) {
            throw new IOException("Mojang version manifest is missing " + key);
        }
        return element.getAsString();
    }

    public record VersionManifest(String latestRelease, String latestSnapshot, List<Version> versions) {
        public Version latestReleaseVersion() {
            return versions.stream()
                    .filter(version -> version.id().equals(latestRelease))
                    .findFirst()
                    .orElseThrow(() -> new IllegalStateException("Latest release is missing"));
        }
    }

    public record Version(String id, String type, URI url, Instant releaseTime, String sha1) {
        @Override
        public String toString() {
            return id + ("release".equals(type) ? " (release)" : " (" + type + ")");
        }
    }
}
