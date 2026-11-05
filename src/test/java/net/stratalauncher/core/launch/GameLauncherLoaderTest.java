package net.stratalauncher.core.launch;

import com.google.gson.JsonParser;
import net.stratalauncher.core.loader.LoaderProfile;
import net.stratalauncher.core.loader.LoaderType;
import org.junit.jupiter.api.Test;

import java.io.IOException;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

class GameLauncherLoaderTest {
    @Test
    void mergesLoaderClassLibrariesAndArgumentsIntoVanillaMetadata() throws IOException {
        var vanilla = JsonParser.parseString("""
                {
                  "mainClass":"net.minecraft.client.main.Main",
                  "libraries":[{"name":"com.example:minecraft:1"}],
                  "arguments":{"game":["--username","${auth_player_name}"],"jvm":["-Xmx2G"]}
                }
                """).getAsJsonObject();
        var loaderJson = JsonParser.parseString("""
                {
                  "id":"fabric-loader-0.16.14-1.21.1",
                  "inheritsFrom":"1.21.1",
                  "mainClass":"net.fabricmc.loader.impl.launch.knot.KnotClient",
                  "libraries":[{"name":"net.fabricmc:fabric-loader:0.16.14"}],
                  "arguments":{"game":["--fabric"],"jvm":["-Dloader=true"]}
                }
                """).getAsJsonObject();
        LoaderProfile profile = new LoaderProfile(
                LoaderType.FABRIC, "1.21.1", "0.16.14", "fabric-loader-0.16.14-1.21.1", loaderJson);

        var merged = GameLauncher.mergeLoaderMetadata(vanilla, profile);

        assertEquals("net.fabricmc.loader.impl.launch.knot.KnotClient", merged.get("mainClass").getAsString());
        assertEquals(2, merged.getAsJsonArray("libraries").size());
        assertEquals(3, merged.getAsJsonObject("arguments").getAsJsonArray("game").size());
        assertTrue(merged.getAsJsonObject("arguments").getAsJsonArray("jvm").toString().contains("-Dloader=true"));
    }
}
