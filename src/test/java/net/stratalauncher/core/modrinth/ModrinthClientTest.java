package net.stratalauncher.core.modrinth;

import com.sun.net.httpserver.HttpServer;
import net.stratalauncher.core.loader.LoaderType;
import org.junit.jupiter.api.Test;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.URI;
import java.net.URLDecoder;
import java.net.http.HttpClient;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicReference;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

class ModrinthClientTest {
    @Test
    void searchesWithProperUserAgentAndInstanceCompatibilityFacets() throws Exception {
        AtomicReference<String> requestUri = new AtomicReference<>();
        AtomicReference<String> userAgent = new AtomicReference<>();
        HttpServer server = HttpServer.create(new InetSocketAddress("127.0.0.1", 0), 0);
        server.createContext("/v2/search", exchange -> {
            requestUri.set(exchange.getRequestURI().toString());
            userAgent.set(exchange.getRequestHeaders().getFirst("User-Agent"));
            byte[] body = """
                    {"hits":[{"project_id":"abc123","slug":"example-mod","title":"Example Mod",
                    "description":"A test mod","project_type":"mod","downloads":12,"categories":["fabric"]}],
                    "total_hits":1}
                    """.getBytes(StandardCharsets.UTF_8);
            exchange.getResponseHeaders().set("Content-Type", "application/json");
            exchange.sendResponseHeaders(200, body.length);
            exchange.getResponseBody().write(body);
            exchange.close();
        });
        server.start();
        try {
            URI apiRoot = URI.create("http://127.0.0.1:" + server.getAddress().getPort() + "/v2/");
            ModrinthClient client = new ModrinthClient(HttpClient.newHttpClient(), apiRoot,
                    "StrataLauncher/0.1.0 (https://github.com/springknight3/minecraft-launcher)");

            var result = client.search(ContentType.MOD, "example mod", "1.21.1",
                    LoaderType.FABRIC, 20, 20);

            assertEquals(1, result.total());
            assertEquals("abc123", result.projects().getFirst().id());
            assertTrue(userAgent.get().contains("StrataLauncher/0.1.0"));
            String decodedQuery = URLDecoder.decode(requestUri.get(), StandardCharsets.UTF_8);
            assertTrue(decodedQuery.contains("project_type:mod"));
            assertTrue(decodedQuery.contains("versions:1.21.1"));
            assertTrue(decodedQuery.contains("categories:fabric"));
            assertTrue(decodedQuery.contains("index=downloads"));
            assertTrue(decodedQuery.contains("offset=20"));
        } finally {
            server.stop(0);
        }
    }

    @Test
    void loadsCompatibleVersionsFromModrinthVersionEndpoint() throws Exception {
        HttpServer server = HttpServer.create(new InetSocketAddress("127.0.0.1", 0), 0);
        server.createContext("/v2/project/abc123/version", exchange -> {
            byte[] body = """
                    [{
                      "id":"version-id","project_id":"abc123","version_number":"1.0.0",
                      "game_versions":["1.21.1"],"loaders":[],
                      "files":[{"filename":"pack.zip","url":"https://cdn.modrinth.com/data/a/pack.zip",
                        "primary":true,"size":42,"hashes":{"sha1":"0123456789abcdef0123456789abcdef01234567"}}]
                    }]
                    """.getBytes(StandardCharsets.UTF_8);
            exchange.getResponseHeaders().set("Content-Type", "application/json");
            exchange.sendResponseHeaders(200, body.length);
            exchange.getResponseBody().write(body);
            exchange.close();
        });
        server.start();
        try {
            ModrinthClient client = new ModrinthClient(HttpClient.newHttpClient(),
                    URI.create("http://127.0.0.1:" + server.getAddress().getPort() + "/v2/"),
                    "StrataLauncher/0.1.0 (https://github.com/springknight3/minecraft-launcher)");

            var versions = client.versions("abc123", "1.21.1", LoaderType.VANILLA,
                    ContentType.RESOURCE_PACK);

            assertEquals(1, versions.size());
            assertEquals("pack.zip", versions.getFirst().primaryFile().filename());
        } finally {
            server.stop(0);
        }
    }

    @Test
    void excludesModVersionsForDifferentLoadersOrMinecraftVersions() throws Exception {
        HttpServer server = HttpServer.create(new InetSocketAddress("127.0.0.1", 0), 0);
        server.createContext("/v2/project/abc123/version", exchange -> {
            byte[] body = """
                    [
                      {"id":"fabric-version","project_id":"abc123","version_number":"fabric",
                       "game_versions":["1.21.1"],"loaders":["fabric"],
                       "files":[{"filename":"fabric.jar","url":"https://example.test/fabric.jar",
                         "primary":true,"size":1,"hashes":{}}]},
                      {"id":"forge-version","project_id":"abc123","version_number":"forge",
                       "game_versions":["1.21.1"],"loaders":["forge"],
                       "files":[{"filename":"forge.jar","url":"https://example.test/forge.jar",
                         "primary":true,"size":1,"hashes":{}}]},
                      {"id":"old-version","project_id":"abc123","version_number":"old",
                       "game_versions":["1.20.1"],"loaders":["fabric"],
                       "files":[{"filename":"old.jar","url":"https://example.test/old.jar",
                         "primary":true,"size":1,"hashes":{}}]}
                    ]
                    """.getBytes(StandardCharsets.UTF_8);
            exchange.getResponseHeaders().set("Content-Type", "application/json");
            exchange.sendResponseHeaders(200, body.length);
            exchange.getResponseBody().write(body);
            exchange.close();
        });
        server.start();
        try {
            ModrinthClient client = new ModrinthClient(HttpClient.newHttpClient(),
                    URI.create("http://127.0.0.1:" + server.getAddress().getPort() + "/v2/"),
                    "StrataLauncher/0.1.0 (https://github.com/springknight3/minecraft-launcher)");

            var versions = client.versions("abc123", "1.21.1", LoaderType.FABRIC, ContentType.MOD);

            assertEquals(1, versions.size());
            assertEquals("fabric-version", versions.getFirst().id());
        } finally {
            server.stop(0);
        }
    }

    @Test
    void searchesPaperPluginsAndLoadsCompatibleServerVersions() throws Exception {
        HttpServer server = HttpServer.create(new InetSocketAddress("127.0.0.1", 0), 0);
        server.createContext("/v2/search", exchange -> {
            byte[] body = """
                    {"hits":[{"project_id":"plugin123","slug":"example-plugin","title":"Example Plugin",
                    "description":"A Paper plugin","project_type":"plugin","downloads":8,"categories":[]}],
                    "total_hits":1}
                    """.getBytes(StandardCharsets.UTF_8);
            exchange.getResponseHeaders().set("Content-Type", "application/json");
            exchange.sendResponseHeaders(200, body.length);
            exchange.getResponseBody().write(body);
            exchange.close();
        });
        server.createContext("/v2/project/plugin123/version", exchange -> {
            byte[] body = """
                    [{
                      "id":"plugin-version","project_id":"plugin123","version_number":"2.0.0",
                      "game_versions":["1.21.1"],"loaders":["paper"],
                      "files":[{"filename":"example-plugin.jar",
                        "url":"https://cdn.modrinth.com/data/a/example-plugin.jar",
                        "primary":true,"size":42,
                        "hashes":{"sha1":"0123456789abcdef0123456789abcdef01234567"}}]
                    }]
                    """.getBytes(StandardCharsets.UTF_8);
            exchange.getResponseHeaders().set("Content-Type", "application/json");
            exchange.sendResponseHeaders(200, body.length);
            exchange.getResponseBody().write(body);
            exchange.close();
        });
        server.start();
        try {
            ModrinthClient client = new ModrinthClient(HttpClient.newHttpClient(),
                    URI.create("http://127.0.0.1:" + server.getAddress().getPort() + "/v2/"),
                    "StrataLauncher/0.1.0 (https://github.com/springknight3/minecraft-launcher)");

            var search = client.searchServer(ServerContentType.PLUGIN, "example", "1.21.1", 0, 20);
            var versions = client.serverVersions("plugin123", "1.21.1", ServerContentType.PLUGIN);

            assertEquals(1, search.projects().size());
            assertEquals("plugin", search.projects().getFirst().projectType());
            assertEquals("2.0.0", versions.getFirst().versionNumber());
            assertEquals("example-plugin.jar", versions.getFirst().primaryFile().filename());
        } finally {
            server.stop(0);
        }
    }
}
