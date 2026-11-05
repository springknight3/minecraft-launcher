package net.stratalauncher.core.loader;

import org.junit.jupiter.api.Test;

import java.io.IOException;
import java.util.List;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

class MavenVersionsTest {
    @Test
    void filtersByMinecraftPrefixAndSortsNewestFirst() throws IOException {
        String metadata = """
                <metadata><versioning><versions>
                  <version>1.21.1-52.0.1</version>
                  <version>1.21.1-52.1.0</version>
                  <version>1.20.1-47.4.0</version>
                </versions></versioning></metadata>
                """;

        assertEquals(List.of("1.21.1-52.1.0", "1.21.1-52.0.1"),
                MavenVersions.parse(metadata, "1.21.1-"));
    }

    @Test
    void rejectsXmlWithExternalEntityDeclarations() {
        String metadata = """
                <!DOCTYPE metadata [<!ENTITY xxe SYSTEM "file:///etc/passwd">]>
                <metadata><versioning><versions><version>&xxe;</version></versions></versioning></metadata>
                """;

        assertThrows(IOException.class, () -> MavenVersions.parse(metadata, "1.21.1-"));
    }

    @Test
    void mapsNeoForgeMinecraftVersionsToCompatibleMavenLines() {
        assertEquals("21.1.", NeoForgeInstaller.compatibilityPrefix("1.21.1"));
        assertEquals("20.4.", NeoForgeInstaller.compatibilityPrefix("1.20.4"));
        assertEquals("26.3.", NeoForgeInstaller.compatibilityPrefix("26.3"));
        assertEquals("", NeoForgeInstaller.compatibilityPrefix("1.19.4"));
    }
}
