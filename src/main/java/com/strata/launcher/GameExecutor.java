package com.strata.launcher;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.atomic.AtomicInteger;

public class GameExecutor {

    private static final HttpClient HTTP = HttpClient.newHttpClient();
    private static final String VERSION_MANIFEST = "https://launchermeta.mojang.com/mc/game/version_manifest_v2.json";
    private static final Path CACHE_DIR = Paths.get(InstanceManager.HOME, ".strata", "cache");

    public interface ProgressCallback {
        void onProgress(String status, int percent);
    }

    public void downloadInstance(Instance instance, ProgressCallback callback) throws Exception {
        callback.onProgress("Fetching version manifest...", 0);
        String versionUrl = getVersionUrl(instance.getVersion());
        if (versionUrl == null) throw new RuntimeException("Version not found: " + instance.getVersion());

        callback.onProgress("Downloading version JSON...", 5);
        String versionJson = download(versionUrl);
        Path versionJsonPath = CACHE_DIR.resolve("versions").resolve(instance.getVersion() + ".json");
        Files.createDirectories(versionJsonPath.getParent());
        Files.writeString(versionJsonPath, versionJson);

        String clientUrl = getClientUrl(versionJson);
        Path clientJar = CACHE_DIR.resolve("versions").resolve(instance.getVersion() + ".jar");
        downloadFile(clientUrl, clientJar, callback, "Client JAR", 10, 35);

        callback.onProgress("Downloading libraries...", 35);
        List<String[]> libraries = extractLibraries(versionJson);
        downloadLibraries(libraries, callback, 35, 70);

        callback.onProgress("Downloading asset index...", 70);
        String assetIndexId = extractNestedString(versionJson, "assetIndex", "id");
        String assetIndexUrl = extractNestedString(versionJson, "assetIndex", "url");
        if (assetIndexId != null && assetIndexUrl != null) {
            Path assetIndexPath = CACHE_DIR.resolve("assetIndexes").resolve(assetIndexId + ".json");
            downloadFile(assetIndexUrl, assetIndexPath, callback, "Asset index", 70, 75);
            String assetIndexJson = Files.readString(assetIndexPath);
            downloadAssets(assetIndexJson, callback, 75, 95);
        }

        callback.onProgress("Done!", 100);
    }

    public void launch(Instance instance, ProgressCallback callback) throws Exception {
        Path versionJsonPath = CACHE_DIR.resolve("versions").resolve(instance.getVersion() + ".json");
        if (!Files.exists(versionJsonPath)) {
            downloadInstance(instance, callback);
        }
        String versionJson = Files.readString(versionJsonPath);

        String mainClass = InstanceManager.extractString(versionJson, "mainClass");
        if (mainClass == null) throw new RuntimeException("No mainClass found in version JSON");

        Path clientJar = CACHE_DIR.resolve("versions").resolve(instance.getVersion() + ".jar");
        List<String[]> libraryDefs = extractLibraries(versionJson);
        List<Path> libraryPaths = new ArrayList<>();
        for (String[] lib : libraryDefs) {
            Path p = CACHE_DIR.resolve("libraries").resolve(lib[1]);
            if (Files.exists(p)) libraryPaths.add(p);
        }

        String classpath = buildClasspath(clientJar, libraryPaths);
        Path gameDir = Paths.get(InstanceManager.INSTANCE_PATH, instance.getName());
        Files.createDirectories(gameDir);

        String assetsDir = CACHE_DIR.resolve("assets").toString();
        String assetIndexId = extractNestedString(versionJson, "assetIndex", "id");

        String accessToken = UUID.randomUUID().toString();
        String uuid = UUID.randomUUID().toString().replace("-", "");

        List<String> gameArgs = new ArrayList<>();
        gameArgs.add(mainClass);
        gameArgs.add("--username"); gameArgs.add(instance.getUsername());
        gameArgs.add("--version"); gameArgs.add(instance.getVersion());
        gameArgs.add("--gameDir"); gameArgs.add(gameDir.toString());
        gameArgs.add("--assetsDir"); gameArgs.add(assetsDir);
        if (assetIndexId != null) { gameArgs.add("--assetIndex"); gameArgs.add(assetIndexId); }
        gameArgs.add("--accessToken"); gameArgs.add(accessToken);
        gameArgs.add("--uuid"); gameArgs.add(uuid);
        gameArgs.add("--userType"); gameArgs.add("mojang");

        List<String> cmd = new ArrayList<>();
        cmd.add(instance.getJavaPath());
        cmd.add("-Xms512M");
        cmd.add("-Xmx" + instance.getMaxMemory() + "M");
        cmd.add("-cp");
        cmd.add(classpath);
        cmd.addAll(gameArgs);

        ProcessBuilder pb = new ProcessBuilder(cmd);
        pb.directory(gameDir.toFile());
        pb.inheritIO();
        pb.start();
    }

    private String buildClasspath(Path clientJar, List<Path> libraries) {
        StringBuilder cp = new StringBuilder();
        cp.append(clientJar.toAbsolutePath());
        for (Path lib : libraries) {
            cp.append(System.getProperty("path.separator")).append(lib.toAbsolutePath());
        }
        return cp.toString();
    }

    private void downloadLibraries(List<String[]> libraries, ProgressCallback callback, int startPct, int endPct) throws Exception {
        int total = libraries.size();
        if (total == 0) return;

        ExecutorService executor = Executors.newFixedThreadPool(InstanceManager.PARALLEL_DOWNLOADS);
        AtomicInteger completed = new AtomicInteger(0);
        List<Future<?>> futures = new ArrayList<>();

        for (String[] lib : libraries) {
            String url = lib[0];
            String path = lib[1];
            futures.add(executor.submit(() -> {
                try {
                    Path dest = CACHE_DIR.resolve("libraries").resolve(path);
                    if (!Files.exists(dest)) {
                        downloadFile(url, dest, (s, p) -> {
                            int done = completed.get();
                            int pct = startPct + (int)((double) done / total * (endPct - startPct));
                            callback.onProgress("Library " + (done + 1) + "/" + total, pct);
                        }, "lib", 0, 100);
                    }
                    completed.incrementAndGet();
                    int pct = startPct + (int)((double) completed.get() / total * (endPct - startPct));
                    callback.onProgress("Library " + completed.get() + "/" + total, pct);
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }));
        }

        for (Future<?> f : futures) f.get();
        executor.shutdown();
    }

    private void downloadAssets(String indexJson, ProgressCallback callback, int startPct, int endPct) throws Exception {
        Path objectsDir = CACHE_DIR.resolve("assets").resolve("objects");
        List<String[]> entries = parseAssetEntries(indexJson);
        int total = entries.size();
        if (total == 0) return;

        ExecutorService executor = Executors.newFixedThreadPool(InstanceManager.PARALLEL_DOWNLOADS);
        AtomicInteger completed = new AtomicInteger(0);
        List<Future<?>> futures = new ArrayList<>();

        for (String[] entry : entries) {
            futures.add(executor.submit(() -> {
                try {
                    Path dest = objectsDir.resolve(entry[1]);
                    if (!Files.exists(dest)) {
                        downloadFile(entry[0], dest, (s, p) -> {}, "asset", 0, 0);
                    }
                    completed.incrementAndGet();
                    int pct = startPct + (int)((double) completed.get() / total * (endPct - startPct));
                    callback.onProgress("Asset " + completed.get() + "/" + total, pct);
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }));
        }

        for (Future<?> f : futures) f.get();
        executor.shutdown();
    }

    private List<String[]> parseAssetEntries(String json) {
        List<String[]> entries = new ArrayList<>();
        int objectsIdx = json.indexOf("\"objects\"");
        if (objectsIdx == -1) return entries;
        int bracket = json.indexOf('{', objectsIdx);
        int depth = 0;
        int entryStart = -1;
        for (int i = bracket; i < json.length(); i++) {
            char c = json.charAt(i);
            if (c == '{') {
                if (depth == 1) entryStart = i;
                depth++;
            } else if (c == '}') {
                depth--;
                if (depth == 1 && entryStart != -1) {
                    String obj = json.substring(entryStart, i + 1);
                    String hash = InstanceManager.extractString(obj, "hash");
                    if (hash != null && hash.length() > 2) {
                        String prefix = hash.substring(0, 2);
                        String url = "https://resources.download.minecraft.net/" + prefix + "/" + hash;
                        entries.add(new String[]{url, prefix + "/" + hash});
                    }
                    entryStart = -1;
                }
            }
        }
        return entries;
    }

    private String getClientUrl(String versionJson) {
        int dlIdx = versionJson.indexOf("\"downloads\"");
        if (dlIdx == -1) return null;
        int clientIdx = versionJson.indexOf("\"client\"", dlIdx);
        if (clientIdx == -1) return null;
        int urlIdx = versionJson.indexOf("\"url\":", clientIdx);
        if (urlIdx == -1) return null;
        int start = versionJson.indexOf('"', urlIdx + 6) + 1;
        int end = versionJson.indexOf('"', start);
        if (start > 0 && end > start) return versionJson.substring(start, end);
        return null;
    }

    private List<String[]> extractLibraries(String versionJson) {
        List<String[]> libs = new ArrayList<>();
        int libIdx = versionJson.indexOf("\"libraries\"");
        if (libIdx == -1) return libs;
        int arrStart = versionJson.indexOf('[', libIdx);
        int depth = 0;
        int entryStart = -1;
        for (int i = arrStart; i < versionJson.length(); i++) {
            char c = versionJson.charAt(i);
            if (c == '{') {
                if (depth == 0) entryStart = i;
                depth++;
            } else if (c == '}') {
                depth--;
                if (depth == 0 && entryStart != -1) {
                    String obj = versionJson.substring(entryStart, i + 1);
                    String url = null;
                    String path = null;

                    int dlIdx = obj.indexOf("\"downloads\"");
                    if (dlIdx != -1) {
                        int artIdx = obj.indexOf("\"artifact\"", dlIdx);
                        if (artIdx != -1) {
                            int urlIdx = obj.indexOf("\"url\":", artIdx);
                            int pathIdx = obj.indexOf("\"path\":", artIdx);
                            if (urlIdx != -1) {
                                int s = obj.indexOf('"', urlIdx + 6) + 1;
                                int e = obj.indexOf('"', s);
                                if (s > 0 && e > s) url = obj.substring(s, e);
                            }
                            if (pathIdx != -1) {
                                int s = obj.indexOf('"', pathIdx + 7) + 1;
                                int e = obj.indexOf('"', s);
                                if (s > 0 && e > s) path = obj.substring(s, e);
                            }
                        }
                    }

                    if (url == null) {
                        String name = InstanceManager.extractString(obj, "name");
                        if (name != null) {
                            url = "https://libraries.minecraft.net/" + name.replace('.', '/').replace(':', '/') + ".jar";
                            path = name.replace('.', '/').replace(':', '/') + ".jar";
                        }
                    }

                    if (url != null && path != null) {
                        libs.add(new String[]{url, path});
                    }
                    entryStart = -1;
                }
            }
        }
        return libs;
    }

    private String getVersionUrl(String version) throws Exception {
        String manifest = download(VERSION_MANIFEST);
        int idx = manifest.indexOf("\"versions\"");
        if (idx == -1) return null;
        idx = manifest.indexOf('[', idx);
        int depth = 0;
        int start = -1;
        for (int i = idx; i < manifest.length(); i++) {
            char c = manifest.charAt(i);
            if (c == '{') {
                if (depth == 0) start = i;
                depth++;
            } else if (c == '}') {
                depth--;
                if (depth == 0 && start != -1) {
                    String obj = manifest.substring(start, i + 1);
                    String id = InstanceManager.extractString(obj, "id");
                    String url = InstanceManager.extractString(obj, "url");
                    if (version.equals(id)) return url;
                    start = -1;
                }
            }
        }
        return null;
    }

    private void downloadFile(String url, Path dest, ProgressCallback callback, String label, int startPct, int endPct) throws Exception {
        if (url == null) return;
        Files.createDirectories(dest.getParent());
        HttpRequest req = HttpRequest.newBuilder().uri(URI.create(url)).GET().build();
        HttpResponse<InputStream> resp = HTTP.send(req, HttpResponse.BodyHandlers.ofInputStream());
        long total = resp.headers().firstValueAsLong("Content-Length").orElse(0);
        try (OutputStream out = Files.newOutputStream(dest); InputStream in = resp.body()) {
            byte[] buf = new byte[8192];
            long downloaded = 0;
            int len;
            while ((len = in.read(buf)) != -1) {
                out.write(buf, 0, len);
                downloaded += len;
                if (total > 0) {
                    int pct = (int)((double) downloaded / total * (endPct - startPct));
                    callback.onProgress(label, startPct + pct);
                }
            }
        }
    }

    private String download(String url) throws Exception {
        HttpRequest req = HttpRequest.newBuilder().uri(URI.create(url)).GET().build();
        HttpResponse<String> resp = HTTP.send(req, HttpResponse.BodyHandlers.ofString());
        return resp.body();
    }

    private static String extractNestedString(String json, String outerKey, String innerKey) {
        int outerIdx = json.indexOf("\"" + outerKey + "\"");
        if (outerIdx == -1) return null;
        int braceIdx = json.indexOf('{', outerIdx);
        if (braceIdx == -1) return null;
        int depth = 0;
        for (int i = braceIdx; i < json.length(); i++) {
            char c = json.charAt(i);
            if (c == '{') depth++;
            else if (c == '}') { depth--; if (depth == 0) break; }
        }
        String inner = json.substring(braceIdx, json.indexOf('}', braceIdx) + 1);
        return InstanceManager.extractString(inner, innerKey);
    }
}
