package net.stratalauncher.core.loader;

import com.google.gson.JsonParser;
import org.junit.jupiter.api.Test;

import java.io.IOException;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

class ProfileBasedInstallerTest {
    @Test
    void parsesLoaderProfileForTheRequestedMinecraftVersion() throws IOException {
        var json = JsonParser.parseString("""
                {
                  "id":"fabric-loader-0.16.14-1.21.1",
                  "inheritsFrom":"1.21.1",
                  "mainClass":"net.fabricmc.loader.impl.launch.knot.KnotClient",
                  "libraries":[{"name":"net.fabricmc:fabric-loader:0.16.14"}]
                }
                """);

        LoaderProfile profile = ProfileBasedInstaller.parse(
                LoaderType.FABRIC, "1.21.1", "0.16.14", json);

        assertEquals("fabric-loader-0.16.14-1.21.1", profile.profileVersionId());
        assertEquals("1.21.1", profile.minecraftVersion());
        assertEquals(1, profile.launchMetadata().getAsJsonArray("libraries").size());
    }

    @Test
    void rejectsProfilesThatInheritFromAnotherMinecraftVersion() {
        var json = JsonParser.parseString("""
                {
                  "id":"fabric-loader-0.16.14-1.20.4",
                  "inheritsFrom":"1.20.4",
                  "mainClass":"net.fabricmc.loader.impl.launch.knot.KnotClient"
                }
                """);

        assertThrows(IOException.class,
                () -> ProfileBasedInstaller.parse(LoaderType.FABRIC, "1.21.1", "0.16.14", json));
    }
}
