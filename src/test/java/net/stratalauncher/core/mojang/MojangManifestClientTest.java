package net.stratalauncher.core.mojang;

import org.junit.jupiter.api.Test;

import java.io.IOException;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

class MojangManifestClientTest {
    @Test
    void parsesVersionsAndSelectsTheLatestRelease() throws IOException {
        String json = """
                {
                  "latest": {"release": "1.21.1", "snapshot": "24w33a"},
                  "versions": [
                    {"id": "24w33a", "type": "snapshot", "url": "https://example.test/snapshot.json", "releaseTime": "2024-08-14T12:00:00+00:00"},
                    {"id": "1.21.1", "type": "release", "url": "https://example.test/release.json", "releaseTime": "2024-08-08T12:00:00+00:00", "sha1": "abc"}
                  ]
                }
                """;

        MojangManifestClient.VersionManifest manifest = MojangManifestClient.parseManifest(json);

        assertEquals("1.21.1", manifest.latestReleaseVersion().id());
        assertEquals(2, manifest.versions().size());
        assertEquals("abc", manifest.latestReleaseVersion().sha1());
    }

    @Test
    void rejectsManifestWithoutLatestReleaseEntry() {
        String json = """
                {
                  "latest": {"release": "missing", "snapshot": "snapshot"},
                  "versions": [
                    {"id": "snapshot", "type": "snapshot", "url": "https://example.test/snapshot.json", "releaseTime": "2024-08-14T12:00:00+00:00"}
                  ]
                }
                """;

        assertThrows(IOException.class, () -> MojangManifestClient.parseManifest(json));
    }
}
