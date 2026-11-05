package net.stratalauncher.core.settings;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Map;

import static org.junit.jupiter.api.Assertions.assertEquals;

class OptionsParserTest {
    @TempDir
    Path temporaryDirectory;

    @Test
    void updatesOneOptionWithoutChangingUnknownLinesOrLineEndings() throws Exception {
        String original = "# preserved comment\r\nunknown_future_option:opaque:value\r\nmaxFps:120\r\n";
        Path optionsFile = temporaryDirectory.resolve("options.txt");
        Files.writeString(optionsFile, original);

        OptionsParser.set(optionsFile, "maxFps", "240");

        assertEquals("# preserved comment\r\nunknown_future_option:opaque:value\r\nmaxFps:240\r\n",
                Files.readString(optionsFile));
    }

    @Test
    void updatesOnlyTheLastDuplicateKeyAndAppendsMissingKeys() {
        String original = "maxFps:60\nfutureKey:keep\nmaxFps:120";

        String changed = OptionsParser.update(original, "maxFps", "240");
        changed = OptionsParser.update(changed, "newOption", "enabled");

        assertEquals("maxFps:60\nfutureKey:keep\nmaxFps:240\nnewOption:enabled\n", changed);
    }

    @Test
    void persistsTheMinecraftFovDegreeValueWithoutReformattingIt() {
        assertEquals("fov:110\nfutureOption:keep\n",
                OptionsParser.update("fov:0.0\nfutureOption:keep\n", "fov", "110"));
    }

    @Test
    void savesSeveralFriendlyOptionsInOneLosslessUpdate() throws Exception {
        Path optionsFile = temporaryDirectory.resolve("options.txt");
        Files.writeString(optionsFile, "fov:0.0\r\nfuture_option:keep\r\n");

        OptionsParser.setAll(optionsFile, Map.of("fov", "1.0", "maxFps", "260"));

        assertEquals("fov:1.0\r\nfuture_option:keep\r\nmaxFps:260\r\n", Files.readString(optionsFile));
    }
}
