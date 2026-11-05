package net.stratalauncher.core.runtime;

import com.google.gson.JsonParser;
import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.auth.LauncherSettings;
import net.stratalauncher.core.instance.Instance;
import net.stratalauncher.core.instance.InstanceManager;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.net.URI;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.List;
import java.util.HexFormat;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

class JavaRuntimeManagerTest {
    @TempDir
    Path temporaryDirectory;

    @Test
    void usesVersionMetadataAndFallsBackForOlderMinecraftVersions() throws IOException {
        var metadata = JsonParser.parseString("""
                {"javaVersion":{"component":"java-runtime-delta","majorVersion":21}}
                """).getAsJsonObject();

        assertEquals("java-runtime-delta", JavaRuntimeManager.requiredComponent(metadata, "1.20.1"));
        assertEquals("jre-legacy", JavaRuntimeManager.requiredComponent(null, "1.16.5"));
        assertEquals("java-runtime-alpha", JavaRuntimeManager.requiredComponent(null, "1.17.1"));
        assertEquals("java-runtime-gamma", JavaRuntimeManager.requiredComponent(null, "1.20.4"));
        assertEquals("java-runtime-delta", JavaRuntimeManager.requiredComponent(null, "1.20.5"));
        assertEquals("java-runtime-epsilon", JavaRuntimeManager.requiredComponent(null, "26.1"));
        assertEquals("jre-legacy", JavaRuntimeManager.requiredComponent(null, "19w14a"));
    }

    @Test
    void selectsTheNewestOfficialRuntimeForAPlatform() throws IOException {
        var index = JsonParser.parseString("""
                {
                  "linux": {
                    "java-runtime-gamma": [
                      {"manifest":{"url":"https://piston-meta.mojang.com/old.json","sha1":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
                       "version":{"name":"17.0.1","released":"2021-01-01T00:00:00Z"}},
                      {"manifest":{"url":"https://piston-meta.mojang.com/new.json","sha1":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
                       "version":{"name":"17.0.2","released":"2022-01-01T00:00:00Z"}}
                    ]
                  }
                }
                """).getAsJsonObject();

        assertEquals(new JavaRuntimeManager.RuntimeManifestInfo(
                        URI.create("https://piston-meta.mojang.com/new.json"),
                        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", "17.0.2"),
                JavaRuntimeManager.selectRuntime(index, "linux", "java-runtime-gamma"));
    }

    @Test
    void parsesRuntimeFilesAndRejectsTraversalPaths() throws IOException {
        var manifest = JsonParser.parseString("""
                {
                  "files": {
                    "bin": {"type":"directory"},
                    "bin/java": {
                      "type":"file", "executable":true,
                      "downloads":{"raw":{"url":"https://piston-data.mojang.com/java","sha1":"cccccccccccccccccccccccccccccccccccccccc"}}
                    },
                    "bin/java-link": {"type":"link","target":"java"}
                  }
                }
                """).getAsJsonObject();

        List<JavaRuntimeManager.RuntimeFile> files = JavaRuntimeManager.parseRuntimeFiles(manifest);

        assertEquals(List.of("bin", "bin/java", "bin/java-link"), files.stream()
                .map(JavaRuntimeManager.RuntimeFile::path).sorted().toList());
        var unsafeManifest = JsonParser.parseString("""
                {"files":{"../outside":{"type":"directory"}}}
                """).getAsJsonObject();
        assertThrows(IOException.class, () -> JavaRuntimeManager.parseRuntimeFiles(unsafeManifest));
    }

    @Test
    void resolvesOnlyAnInstalledManagedJavaExecutableWithAMatchingChecksum() throws Exception {
        Instance instance = new InstanceManager(temporaryDirectory).create("Test", "1.20.1");
        Path runtimeDirectory = instance.directory().resolve("java-runtimes/java-runtime-gamma");
        Path executable = runtimeDirectory.resolve("bin/java");
        Files.createDirectories(executable.getParent());
        Files.writeString(executable, "runtime");
        executable.toFile().setExecutable(true, true);
        String checksum = HexFormat.of().formatHex(MessageDigest.getInstance("SHA-1")
                .digest(Files.readAllBytes(executable)));
        Files.writeString(runtimeDirectory.resolve(".stratalauncher-java-path"), "bin/java\n" + checksum + "\n");
        Files.writeString(runtimeDirectory.resolve(".stratalauncher-runtime-version"), "17.0.15");
        var metadata = JsonParser.parseString("""
                {"javaVersion":{"component":"java-runtime-gamma","majorVersion":17}}
                """).getAsJsonObject();

        try (DownloadManager downloads = new DownloadManager()) {
            JavaRuntimeManager manager = new JavaRuntimeManager(downloads);
            assertEquals(executable, manager.javaExecutable(instance, metadata));
            Files.writeString(executable, "modified");
            assertThrows(IOException.class, () -> manager.javaExecutable(instance, metadata));
        }
    }

    @Test
    void selectsExternalRuntimeComponentFromTheSavedProvider() throws Exception {
        LauncherSettings settings = new LauncherSettings(temporaryDirectory);
        settings.setRuntimePreferences(LauncherSettings.RuntimeVendor.TEMURIN, false);
        var metadata = JsonParser.parseString("""
                {"javaVersion":{"component":"java-runtime-delta","majorVersion":21}}
                """).getAsJsonObject();
        try (DownloadManager downloads = new DownloadManager()) {
            JavaRuntimeManager manager = new JavaRuntimeManager(downloads, temporaryDirectory, settings);

            assertEquals("temurin-java-21",
                    manager.runtimeComponent(LauncherSettings.RuntimeVendor.TEMURIN, metadata, "1.21.1"));
            assertEquals("zulu-java-21",
                    manager.runtimeComponent(LauncherSettings.RuntimeVendor.ZULU, metadata, "1.21.1"));
        }
    }

    @Test
    void listsSharedRuntimesAndPreventsDeletingOneThatIsInUse() throws Exception {
        Path dataDirectory = temporaryDirectory.resolve("launcher-data");
        Path ownerDirectory = dataDirectory.resolve("instances/test-instance");
        Path runtimeDirectory = dataDirectory.resolve("java-runtimes/temurin-java-21");
        Files.createDirectories(runtimeDirectory);
        Files.writeString(runtimeDirectory.resolve(".stratalauncher-java-path"), "bin/java\nchecksum\n");
        Files.writeString(runtimeDirectory.resolve(".stratalauncher-runtime-version"), "21.0.7");
        Files.writeString(runtimeDirectory.resolve(".stratalauncher-runtime-vendor"), "TEMURIN");

        try (DownloadManager downloads = new DownloadManager()) {
            JavaRuntimeManager manager = new JavaRuntimeManager(downloads, dataDirectory, null);
            List<JavaRuntimeManager.RuntimeOwner> owners = List.of(
                    new JavaRuntimeManager.RuntimeOwner("Instance: Test", ownerDirectory, true, "temurin-java-21"));

            List<JavaRuntimeManager.InstalledRuntime> runtimes = manager.listInstalledRuntimes(owners);

            assertEquals(1, runtimes.size());
            assertEquals("21.0.7", runtimes.getFirst().version());
            assertEquals(List.of("Instance: Test"), runtimes.getFirst().users());
            assertThrows(IOException.class, () -> manager.deleteRuntime(runtimes.getFirst()));

            JavaRuntimeManager.InstalledRuntime unused = new JavaRuntimeManager.InstalledRuntime(
                    runtimeDirectory, "temurin-java-21", "TEMURIN", "21.0.7", List.of());
            manager.deleteRuntime(unused);
            assertEquals(false, Files.exists(runtimeDirectory));
        }
    }
}
