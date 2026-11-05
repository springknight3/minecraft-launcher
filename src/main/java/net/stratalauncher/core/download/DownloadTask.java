package net.stratalauncher.core.download;

import java.net.URI;
import java.nio.file.Path;
import java.util.Locale;
import java.util.Objects;

public record DownloadTask(URI source, Path destination, String expectedSha1, String description) {
    public DownloadTask {
        Objects.requireNonNull(source, "source");
        Objects.requireNonNull(destination, "destination");
        Objects.requireNonNull(description, "description");
        expectedSha1 = expectedSha1 == null ? "" : expectedSha1.trim().toLowerCase(Locale.ROOT);
        if (!"https".equalsIgnoreCase(source.getScheme()) && !"http".equalsIgnoreCase(source.getScheme())) {
            throw new IllegalArgumentException("Download source must use HTTP or HTTPS");
        }
        if (!expectedSha1.isEmpty() && !expectedSha1.matches("[0-9a-f]{40}")) {
            throw new IllegalArgumentException("Expected SHA-1 must contain 40 hexadecimal characters");
        }
        if (description.isBlank()) {
            throw new IllegalArgumentException("Download description must not be blank");
        }
    }

    public Path normalizedDestination() {
        return destination.toAbsolutePath().normalize();
    }
}
