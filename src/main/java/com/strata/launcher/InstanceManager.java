package com.strata.launcher;

import java.io.IOException;
import java.io.Reader;
import java.io.Writer;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class InstanceManager {

    public static String INSTANCE_PATH = System.getProperty("user.home") + "/.strata/instances";
    public static final String HOME = System.getProperty("user.home");
    public static int PARALLEL_DOWNLOADS = 4;

    private static final Path INSTANCES_DIR = Paths.get(INSTANCE_PATH);
    private static final Path INSTANCES_JSON = INSTANCES_DIR.resolve("instances.json");
    private static final HttpClient HTTP = HttpClient.newHttpClient();
    private static final String VERSION_MANIFEST_URL = "https://launchermeta.mojang.com/mc/game/version_manifest_v2.json";

    private final List<Instance> instances = new ArrayList<>();

    public InstanceManager() {
        load();
    }

    public List<Instance> getInstances() {
        return Collections.unmodifiableList(instances);
    }

    public void addInstance(Instance instance) {
        instances.add(instance);
        save();
    }

    public void removeInstance(Instance instance) {
        instances.remove(instance);
        save();
    }

    public void save() {
        try {
            Files.createDirectories(INSTANCES_DIR);
            StringBuilder json = new StringBuilder("[");
            for (int i = 0; i < instances.size(); i++) {
                Instance inst = instances.get(i);
                if (i > 0) json.append(",");
                json.append("{");
                json.append("\"name\":\"").append(escapeJson(inst.getName())).append("\",");
                json.append("\"version\":\"").append(escapeJson(inst.getVersion())).append("\",");
                json.append("\"username\":\"").append(escapeJson(inst.getUsername())).append("\",");
                json.append("\"maxMemory\":").append(inst.getMaxMemory()).append(",");
                json.append("\"javaPath\":\"").append(escapeJson(inst.getJavaPath())).append("\",");
                json.append("\"separateJvm\":").append(inst.isSeparateJvm()).append(",");
                json.append("\"downloaded\":").append(inst.isDownloaded());
                json.append("}");
            }
            json.append("]");
            try (Writer w = Files.newBufferedWriter(INSTANCES_JSON)) {
                w.write(json.toString());
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public void load() {
        instances.clear();
        if (!Files.exists(INSTANCES_JSON)) return;
        try (Reader r = Files.newBufferedReader(INSTANCES_JSON)) {
            StringBuilder sb = new StringBuilder();
            char[] buf = new char[1024];
            int len;
            while ((len = r.read(buf)) != -1) sb.append(buf, 0, len);
            String json = sb.toString().trim();
            if (json.isEmpty() || json.equals("[]")) return;
            json = json.substring(1, json.length() - 1);
            String[] entries = splitJsonObjects(json);
            for (String entry : entries) {
                String name = extractString(entry, "name");
                String version = extractString(entry, "version");
                if (name != null && version != null) {
                    Instance inst = new Instance(name, version);
                    String username = extractString(entry, "username");
                    if (username != null) inst.setUsername(username);
                    Integer maxMem = extractInt(entry, "maxMemory");
                    if (maxMem != null) inst.setMaxMemory(maxMem);
                    String javaPath = extractString(entry, "javaPath");
                    if (javaPath != null) inst.setJavaPath(javaPath);
                    Boolean sep = extractBool(entry, "separateJvm");
                    if (sep != null) inst.setSeparateJvm(sep);
                    Boolean dl = extractBool(entry, "downloaded");
                    if (dl != null) inst.setDownloaded(dl);
                    instances.add(inst);
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public Path getInstanceDir(Instance instance) {
        return INSTANCES_DIR.resolve(instance.getName());
    }

    public static List<String> fetchVersions() {
        try {
            HttpRequest req = HttpRequest.newBuilder().uri(URI.create(VERSION_MANIFEST_URL)).GET().build();
            HttpResponse<String> resp = HTTP.send(req, HttpResponse.BodyHandlers.ofString());
            return parseVersions(resp.body());
        } catch (IOException | InterruptedException e) {
            e.printStackTrace();
            return List.of("1.21");
        }
    }

    private static List<String> parseVersions(String json) {
        List<String> versions = new ArrayList<>();
        int idx = json.indexOf("\"versions\"");
        if (idx == -1) return versions;
        idx = json.indexOf('[', idx);
        int depth = 0;
        int start = -1;
        for (int i = idx; i < json.length(); i++) {
            char c = json.charAt(i);
            if (c == '{') {
                if (depth == 0) start = i;
                depth++;
            } else if (c == '}') {
                depth--;
                if (depth == 0 && start != -1) {
                    String obj = json.substring(start, i + 1);
                    String id = extractString(obj, "id");
                    String type = extractString(obj, "type");
                    if (id != null && "release".equals(type)) {
                        versions.add(id);
                    }
                    start = -1;
                }
            }
        }
        return versions;
    }

    private static String escapeJson(String s) {
        return s.replace("\\", "\\\\").replace("\"", "\\\"");
    }

    public static String extractString(String json, String key) {
        String search = "\"" + key + "\"";
        int idx = json.indexOf(search);
        if (idx == -1) return null;
        idx = json.indexOf(':', idx) + 1;
        while (idx < json.length() && json.charAt(idx) == ' ') idx++;
        if (idx >= json.length() || json.charAt(idx) != '"') return null;
        idx++;
        int end = json.indexOf('"', idx);
        if (end == -1) return null;
        return json.substring(idx, end);
    }

    private static Integer extractInt(String json, String key) {
        String search = "\"" + key + "\"";
        int idx = json.indexOf(search);
        if (idx == -1) return null;
        idx = json.indexOf(':', idx) + 1;
        while (idx < json.length() && json.charAt(idx) == ' ') idx++;
        int end = idx;
        while (end < json.length() && Character.isDigit(json.charAt(end))) end++;
        if (end == idx) return null;
        try { return Integer.parseInt(json.substring(idx, end)); } catch (Exception e) { return null; }
    }

    private static Boolean extractBool(String json, String key) {
        String search = "\"" + key + "\"";
        int idx = json.indexOf(search);
        if (idx == -1) return null;
        idx = json.indexOf(':', idx) + 1;
        while (idx < json.length() && json.charAt(idx) == ' ') idx++;
        if (json.startsWith("true", idx)) return true;
        if (json.startsWith("false", idx)) return false;
        return null;
    }

    private static String[] splitJsonObjects(String json) {
        List<String> objects = new ArrayList<>();
        int depth = 0;
        int start = -1;
        for (int i = 0; i < json.length(); i++) {
            char c = json.charAt(i);
            if (c == '{') {
                if (depth == 0) start = i;
                depth++;
            } else if (c == '}') {
                depth--;
                if (depth == 0 && start != -1) {
                    objects.add(json.substring(start, i + 1));
                    start = -1;
                }
            }
        }
        return objects.toArray(new String[0]);
    }
}
