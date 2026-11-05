package net.stratalauncher.core.loader;

import com.google.gson.JsonElement;
import com.google.gson.JsonParseException;
import com.google.gson.JsonParser;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;

final class LoaderMetadataClient {
    private final HttpClient client;

    LoaderMetadataClient(HttpClient client) {
        this.client = client;
    }

    String getText(URI uri) throws IOException, InterruptedException {
        HttpRequest request = HttpRequest.newBuilder(uri)
                .timeout(Duration.ofSeconds(30))
                .header("Accept", "application/json, application/xml, text/plain")
                .header("User-Agent", "StrataLauncher/0.1.0")
                .GET()
                .build();
        HttpResponse<String> response = client.send(request,
                HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));
        if (response.statusCode() < 200 || response.statusCode() >= 300) {
            throw new IOException("Loader service returned HTTP " + response.statusCode() + ": " + uri);
        }
        return response.body();
    }

    JsonElement getJson(URI uri) throws IOException, InterruptedException {
        try {
            return JsonParser.parseString(getText(uri));
        } catch (JsonParseException exception) {
            throw new IOException("Loader service returned invalid JSON: " + uri, exception);
        }
    }
}
