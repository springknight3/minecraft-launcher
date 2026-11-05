package net.stratalauncher.core.modrinth;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import com.google.gson.JsonParser;
import net.stratalauncher.core.loader.LoaderType;

import java.io.IOException;
import java.net.URI;
import java.net.URLEncoder;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

public final class ModrinthClient {
    private static final URI API_ROOT = URI.create("https://api.modrinth.com/v2/");
    private static final int MAX_PAGE_SIZE = 100;
    private final HttpClient httpClient;
    private final URI apiRoot;
    private final String userAgent;

    public ModrinthClient(String projectName, String version) {
        this(HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(15))
                .followRedirects(HttpClient.Redirect.NORMAL).build(), API_ROOT,
                formatUserAgent(projectName, version));
    }

    ModrinthClient(HttpClient httpClient, URI apiRoot, String userAgent) {
        this.httpClient = Objects.requireNonNull(httpClient);
        this.apiRoot = Objects.requireNonNull(apiRoot);
        this.userAgent = requireUserAgent(userAgent);
    }

    public SearchPage search(ContentType type, String query, String minecraftVersion,
                             LoaderType loader, int offset, int limit)
            throws IOException, InterruptedException {
        Objects.requireNonNull(type);
        if (offset < 0 || limit < 1 || limit > MAX_PAGE_SIZE) {
            throw new IllegalArgumentException("Modrinth pagination must use an offset >= 0 and limit 1-100.");
        }
        if (minecraftVersion == null || minecraftVersion.isBlank()) {
            throw new IllegalArgumentException("Choose a Minecraft version before searching Modrinth.");
        }
        if (type == ContentType.MOD && loader == LoaderType.VANILLA) {
            throw new IOException("Mods require an installed Fabric, Quilt, Forge, or NeoForge loader.");
        }

        JsonArray facets = new JsonArray();
        facets.add(singleFacet("project_type:" + type.projectType()));
        facets.add(singleFacet("versions:" + minecraftVersion));
        if (type == ContentType.MOD && loader != null) {
            facets.add(singleFacet("categories:" + modrinthLoader(loader)));
        }
        String encodedFacets = URLEncoder.encode(facets.toString(), StandardCharsets.UTF_8);
        String url = apiRoot.resolve("search").toString() + "?query="
                + URLEncoder.encode(query == null ? "" : query.trim(), StandardCharsets.UTF_8)
                + "&facets=" + encodedFacets + "&index=downloads&offset=" + offset + "&limit=" + limit;
        JsonElement json = getJson(URI.create(url));
        if (!json.isJsonObject()) {
            throw new IOException("Modrinth returned an invalid search response.");
        }
        JsonObject response = json.getAsJsonObject();
        JsonArray hits = requiredArray(response, "hits");
        List<Project> projects = new ArrayList<>();
        for (JsonElement element : hits) {
            if (element.isJsonObject()) {
                projects.add(parseProject(element.getAsJsonObject()));
            }
        }
        int total = requiredInt(response, response.has("total_hits") ? "total_hits" : "total");
        return new SearchPage(List.copyOf(projects), total, offset, limit);
    }

    public SearchPage searchServer(ServerContentType type, String query, String minecraftVersion,
                                   int offset, int limit) throws IOException, InterruptedException {
        Objects.requireNonNull(type);
        if (offset < 0 || limit < 1 || limit > MAX_PAGE_SIZE) {
            throw new IllegalArgumentException("Modrinth pagination must use an offset >= 0 and limit 1-100.");
        }
        if (minecraftVersion == null || minecraftVersion.isBlank()) {
            throw new IllegalArgumentException("Choose a Minecraft version before searching Modrinth.");
        }
        JsonArray facets = new JsonArray();
        facets.add(singleFacet("project_type:" + type.projectType()));
        facets.add(singleFacet("versions:" + minecraftVersion));
        String encodedFacets = URLEncoder.encode(facets.toString(), StandardCharsets.UTF_8);
        String url = apiRoot.resolve("search").toString() + "?query="
                + URLEncoder.encode(query == null ? "" : query.trim(), StandardCharsets.UTF_8)
                + "&facets=" + encodedFacets + "&offset=" + offset + "&limit=" + limit;
        JsonElement json = getJson(URI.create(url));
        if (!json.isJsonObject()) {
            throw new IOException("Modrinth returned an invalid server-content search response.");
        }
        JsonObject response = json.getAsJsonObject();
        List<Project> projects = new ArrayList<>();
        for (JsonElement element : requiredArray(response, "hits")) {
            if (element.isJsonObject()) {
                Project project = parseProject(element.getAsJsonObject());
                if (project.projectType().equals(type.projectType())) {
                    projects.add(project);
                }
            }
        }
        int total = requiredInt(response, response.has("total_hits") ? "total_hits" : "total");
        return new SearchPage(List.copyOf(projects), total, offset, limit);
    }

    public List<ProjectVersion> serverVersions(String projectId, String minecraftVersion,
                                               ServerContentType type)
            throws IOException, InterruptedException {
        requireIdentifier(projectId, "project identifier");
        Objects.requireNonNull(type);
        if (minecraftVersion == null || minecraftVersion.isBlank()) {
            throw new IllegalArgumentException("Minecraft version is required.");
        }
        String endpoint = "project/" + projectId + "/version?game_versions="
                + URLEncoder.encode("[\"" + minecraftVersion + "\"]", StandardCharsets.UTF_8);
        JsonElement response = getJson(apiRoot.resolve(endpoint));
        if (!response.isJsonArray()) {
            throw new IOException("Modrinth returned an invalid server-content version list.");
        }
        List<ProjectVersion> versions = new ArrayList<>();
        for (JsonElement element : response.getAsJsonArray()) {
            if (element.isJsonObject()) {
                ProjectVersion version = parseVersion(element.getAsJsonObject(), type.projectType());
                if (version.projectType().equals(type.projectType())
                        && version.gameVersions().contains(minecraftVersion)) {
                    versions.add(version);
                }
            }
        }
        return List.copyOf(versions);
    }

    public List<ProjectVersion> versions(String projectId, String minecraftVersion, LoaderType loader,
                                         ContentType type) throws IOException, InterruptedException {
        requireIdentifier(projectId, "project identifier");
        Objects.requireNonNull(type);
        if (minecraftVersion == null || minecraftVersion.isBlank()) {
            throw new IllegalArgumentException("Minecraft version is required.");
        }
        if (type == ContentType.MOD && (loader == null || loader == LoaderType.VANILLA)) {
            throw new IOException("Mods require an installed mod loader.");
        }
        String requiredLoader = type == ContentType.MOD ? modrinthLoader(loader) : null;
        StringBuilder endpoint = new StringBuilder("project/").append(projectId).append("/version?game_versions=")
                .append(URLEncoder.encode("[\"" + minecraftVersion + "\"]", StandardCharsets.UTF_8));
        if (requiredLoader != null) {
            endpoint.append("&loaders=").append(URLEncoder.encode("[\"" + requiredLoader + "\"]",
                    StandardCharsets.UTF_8));
        }
        JsonElement response = getJson(apiRoot.resolve(endpoint.toString()));
        if (!response.isJsonArray()) {
            throw new IOException("Modrinth returned an invalid project version list.");
        }
        List<ProjectVersion> result = new ArrayList<>();
        for (JsonElement element : response.getAsJsonArray()) {
            if (element.isJsonObject()) {
                ProjectVersion version = parseVersion(element.getAsJsonObject(), type.projectType());
                if (version.projectType().equals(type.projectType())
                        && version.gameVersions().contains(minecraftVersion)
                        && (requiredLoader == null || version.loaders().contains(requiredLoader))) {
                    result.add(version);
                }
            }
        }
        return List.copyOf(result);
    }

    static Project parseProject(JsonObject json) throws IOException {
        String id = requiredString(json, "project_id");
        String slug = optionalString(json, "slug");
        String title = requiredString(json, "title");
        String description = optionalString(json, "description");
        String projectType = requiredString(json, "project_type");
        if (java.util.Arrays.stream(ContentType.values()).noneMatch(type -> type.projectType().equals(projectType))
                && java.util.Arrays.stream(ServerContentType.values())
                .noneMatch(type -> type.projectType().equals(projectType))) {
            throw new IOException("Modrinth returned a project with an unsupported content type.");
        }
        long downloads = optionalLong(json, "downloads", 0);
        String iconUrl = optionalString(json, "icon_url");
        List<String> categories = stringList(json, "categories");
        return new Project(id, slug, title, description, projectType, downloads, iconUrl, categories);
    }

    static ProjectVersion parseVersion(JsonObject json, String expectedProjectType) throws IOException {
        String id = requiredString(json, "id");
        String projectId = requiredString(json, "project_id");
        String versionNumber = requiredString(json, "version_number");
        String projectType = json.has("project_type")
                ? requiredString(json, "project_type") : expectedProjectType;
        if (!expectedProjectType.equals(projectType)) {
            throw new IOException("Modrinth returned a version for a different content category.");
        }
        List<String> gameVersions = stringList(json, "game_versions");
        JsonArray files = requiredArray(json, "files");
        List<VersionFile> parsedFiles = new ArrayList<>();
        for (JsonElement element : files) {
            if (!element.isJsonObject()) {
                continue;
            }
            JsonObject file = element.getAsJsonObject();
            JsonObject hashes = file.has("hashes") && file.get("hashes").isJsonObject()
                    ? file.getAsJsonObject("hashes") : new JsonObject();
            boolean primary = file.has("primary") && file.get("primary").getAsBoolean();
            parsedFiles.add(new VersionFile(requiredString(file, "filename"),
                    requiredString(file, "url"), optionalString(hashes, "sha1"), primary,
                    optionalLong(file, "size", -1)));
        }
        if (parsedFiles.isEmpty()) {
            throw new IOException("Modrinth version has no downloadable files.");
        }
        List<String> loaders = stringList(json, "loaders");
        return new ProjectVersion(id, projectId, versionNumber, projectType, gameVersions,
                loaders, List.copyOf(parsedFiles));
    }

    private JsonElement getJson(URI uri) throws IOException, InterruptedException {
        HttpRequest request = HttpRequest.newBuilder(uri)
                .timeout(Duration.ofSeconds(30))
                .header("Accept", "application/json")
                .header("User-Agent", userAgent)
                .GET()
                .build();
        HttpResponse<String> response = httpClient.send(request,
                HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));
        if (response.statusCode() < 200 || response.statusCode() >= 300) {
            throw new IOException("Modrinth API returned HTTP " + response.statusCode() + ".");
        }
        try {
            JsonElement parsed = JsonParser.parseString(response.body());
            if (!parsed.isJsonObject() && !parsed.isJsonArray()) {
                throw new IOException("Modrinth returned an invalid JSON response.");
            }
            return parsed;
        } catch (JsonParseException exception) {
            throw new IOException("Modrinth returned malformed JSON.", exception);
        }
    }

    private static JsonArray singleFacet(String value) {
        JsonArray inner = new JsonArray();
        inner.add(value);
        return inner;
    }

    private static String modrinthLoader(LoaderType loader) throws IOException {
        return switch (loader) {
            case FABRIC -> "fabric";
            case QUILT -> "quilt";
            case FORGE -> "forge";
            case NEOFORGE -> "neoforge";
            case VANILLA -> throw new IOException("Mods require an installed mod loader.");
        };
    }

    private static String requiredString(JsonObject json, String key) throws IOException {
        if (!json.has(key) || !json.get(key).isJsonPrimitive()
                || !json.getAsJsonPrimitive(key).isString() || json.get(key).getAsString().isBlank()) {
            throw new IOException("Modrinth response is missing " + key + ".");
        }
        return json.get(key).getAsString();
    }

    private static String optionalString(JsonObject json, String key) {
        return json.has(key) && json.get(key).isJsonPrimitive() && json.getAsJsonPrimitive(key).isString()
                ? json.get(key).getAsString() : "";
    }

    private static long optionalLong(JsonObject json, String key, long fallback) {
        try {
            return json.has(key) ? json.get(key).getAsLong() : fallback;
        } catch (NumberFormatException exception) {
            return fallback;
        }
    }

    private static int requiredInt(JsonObject json, String key) throws IOException {
        long value = optionalLong(json, key, -1);
        if (value < 0 || value > Integer.MAX_VALUE) {
            throw new IOException("Modrinth response contains an invalid " + key + ".");
        }
        return (int) value;
    }

    private static JsonArray requiredArray(JsonObject json, String key) throws IOException {
        if (!json.has(key) || !json.get(key).isJsonArray()) {
            throw new IOException("Modrinth response is missing " + key + ".");
        }
        return json.getAsJsonArray(key);
    }

    private static List<String> stringList(JsonObject json, String key) throws IOException {
        if (!json.has(key)) {
            return List.of();
        }
        if (!json.get(key).isJsonArray()) {
            throw new IOException("Modrinth response contains an invalid " + key + " list.");
        }
        List<String> values = new ArrayList<>();
        for (JsonElement element : json.getAsJsonArray(key)) {
            if (element.isJsonPrimitive() && element.getAsJsonPrimitive().isString()) {
                values.add(element.getAsString());
            }
        }
        return List.copyOf(values);
    }

    private static void requireIdentifier(String value, String label) {
        if (value == null || !value.matches("[A-Za-z0-9_-]{1,128}")) {
            throw new IllegalArgumentException("Invalid Modrinth " + label + ".");
        }
    }

    private static String formatUserAgent(String projectName, String version) {
        if (projectName == null || !projectName.matches("[A-Za-z0-9._-]+")
                || version == null || !version.matches("[A-Za-z0-9._+-]+")) {
            throw new IllegalArgumentException("Invalid Modrinth application name or version.");
        }
        return requireUserAgent(projectName + "/" + version
                + " (https://github.com/springknight3/minecraft-launcher)");
    }

    private static String requireUserAgent(String value) {
        if (value == null || value.isBlank() || value.contains("\r") || value.contains("\n")) {
            throw new IllegalArgumentException("Modrinth User-Agent must be a non-empty single line.");
        }
        return value;
    }

    public record Project(String id, String slug, String title, String description,
                          String projectType, long downloads, String iconUrl, List<String> categories) {
    }

    public record SearchPage(List<Project> projects, int total, int offset, int limit) {
    }

    public record ProjectVersion(String id, String projectId, String versionNumber, String projectType,
                                 List<String> gameVersions, List<String> loaders, List<VersionFile> files) {
        public VersionFile primaryFile() {
            return files.stream().filter(VersionFile::primary).findFirst().orElse(files.getFirst());
        }
    }

    public record VersionFile(String filename, String url, String sha1, boolean primary, long size) {
    }
}
