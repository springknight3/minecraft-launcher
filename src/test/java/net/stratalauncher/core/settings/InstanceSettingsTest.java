package net.stratalauncher.core.settings;

import net.stratalauncher.core.instance.Instance;
import net.stratalauncher.core.instance.InstanceManager;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Path;
import java.util.List;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

class InstanceSettingsTest {
    @TempDir
    Path temporaryDirectory;

    @Test
    void inheritsGlobalFlagsUntilAnInstanceOverrideIsSaved() throws IOException {
        Instance instance = new InstanceManager(temporaryDirectory).create("Test", "1.21.1");
        List<String> global = List.of("-Xmx2G", "-XX:+UseG1GC");
        InstanceSettings settings = new InstanceSettings(instance);

        assertFalse(settings.hasJavaLaunchFlagsOverride());
        assertEquals(InstanceSettings.JavaRuntimeMode.SYSTEM, settings.javaRuntimeMode());
        assertEquals(global, settings.effectiveJavaLaunchFlags(global));

        settings.setJavaLaunchFlagsOverride(List.of("-Xmx4G"));
        InstanceSettings reloaded = new InstanceSettings(instance);
        assertTrue(reloaded.hasJavaLaunchFlagsOverride());
        assertEquals(List.of("-Xmx4G"), reloaded.effectiveJavaLaunchFlags(global));

        reloaded.clearJavaLaunchFlagsOverride();
        assertEquals(global, new InstanceSettings(instance).effectiveJavaLaunchFlags(global));
    }

    @Test
    void rejectsNonJvmAndClassPathControlArguments() {
        assertThrows(IllegalArgumentException.class,
                () -> InstanceSettings.validateFlags(List.of("not-a-flag")));
        assertThrows(IllegalArgumentException.class,
                () -> InstanceSettings.validateFlags(List.of("-cp")));
        assertThrows(IllegalArgumentException.class,
                () -> InstanceSettings.validateFlags(List.of("@args.txt")));
    }

    @Test
    void persistsThePerInstanceJavaRuntimeChoiceAlongsideFlagOverrides() throws IOException {
        Instance instance = new InstanceManager(temporaryDirectory).create("Test", "1.20.1");
        InstanceSettings settings = new InstanceSettings(instance);
        settings.setJavaLaunchFlagsOverride(List.of("-Xmx4G"));
        settings.setJavaRuntimeMode(InstanceSettings.JavaRuntimeMode.MANAGED);

        InstanceSettings reloaded = new InstanceSettings(instance);

        assertEquals(InstanceSettings.JavaRuntimeMode.MANAGED, reloaded.javaRuntimeMode());
        assertEquals(List.of("-Xmx4G"), reloaded.javaLaunchFlagsOverride());
    }

    @Test
    void persistsTheManagedRuntimeComponentForStableLaunchSelection() throws IOException {
        Instance instance = new InstanceManager(temporaryDirectory).create("Test", "1.21.1");
        InstanceSettings settings = new InstanceSettings(instance);
        settings.setJavaRuntimeMode(InstanceSettings.JavaRuntimeMode.MANAGED);
        settings.setJavaRuntimeComponent("temurin-java-21");

        InstanceSettings reloaded = new InstanceSettings(instance);

        assertEquals("temurin-java-21", reloaded.javaRuntimeComponent());
        assertEquals(InstanceSettings.JavaRuntimeMode.MANAGED, reloaded.javaRuntimeMode());
    }
}
