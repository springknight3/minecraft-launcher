package net.stratalauncher;

import net.stratalauncher.core.auth.VSyncConfiguration;
import net.stratalauncher.core.instance.InstanceManager;

import java.lang.management.ManagementFactory;
import java.lang.reflect.InvocationTargetException;
import java.nio.file.Path;
import java.util.List;
import java.util.logging.Logger;
import java.util.regex.Pattern;

public final class LauncherBootstrap {
    private static final Logger LOGGER = Logger.getLogger(LauncherBootstrap.class.getName());
    private static final Pattern VSYNC_OPTION = Pattern.compile(
            "(?:^|\\s)-Dprism\\.vsync(?:=|(?=\\s|$))");

    private LauncherBootstrap() {
    }

    public static void main(String[] args) {
        VSyncConfiguration.Resolution resolution = resolveVsync();
        if (resolution.source() != VSyncConfiguration.Source.COMMAND_LINE_OVERRIDE) {
            System.setProperty("prism.vsync", Boolean.toString(resolution.enabled()));
        }
        System.setProperty("javafx.animation.pulse", "60");
        System.setProperty("prism.order", "es2,sw");
        System.setProperty("stratalauncher.vsync.source", resolution.source().label());
        LOGGER.info("Effective prism.vsync: " + resolution.enabled()
                + " (source: " + resolution.source().label() + ")");
        launchJavaFx(args);
    }

    private static VSyncConfiguration.Resolution resolveVsync() {
        try {
            boolean explicit = hasExplicitVsyncOption();
            Boolean configuredValue = explicit ? null
                    : VSyncConfiguration.readConfiguredValue(
                            InstanceManager.defaultDataDirectory().resolve("settings.json"));
            String explicitOverride = explicit ? System.getProperty("prism.vsync", "") : null;
            boolean linuxDefault = System.getProperty("os.name", "")
                    .toLowerCase(java.util.Locale.ROOT).contains("linux");
            return VSyncConfiguration.resolve(linuxDefault, configuredValue, explicitOverride);
        } catch (java.io.IOException exception) {
            throw new IllegalStateException("Could not read the launcher VSync setting before JavaFX startup.",
                    exception);
        }
    }

    private static boolean hasExplicitVsyncOption() {
        List<String> inputArguments = ManagementFactory.getRuntimeMXBean().getInputArguments();
        if (inputArguments.stream().anyMatch(argument -> VSYNC_OPTION.matcher(argument).find())) {
            return true;
        }
        return List.of("JAVA_TOOL_OPTIONS", "JDK_JAVA_OPTIONS", "_JAVA_OPTIONS").stream()
                .map(System::getenv)
                .filter(java.util.Objects::nonNull)
                .anyMatch(options -> VSYNC_OPTION.matcher(options).find());
    }

    private static void launchJavaFx(String[] args) {
        try {
            Class<?> application = Class.forName("javafx.application.Application");
            Class<?> launcher = Class.forName("net.stratalauncher.App");
            application.getMethod("launch", Class.class, String[].class).invoke(null, launcher, args);
        } catch (ClassNotFoundException | NoSuchMethodException | IllegalAccessException exception) {
            throw new IllegalStateException("Could not launch JavaFX.", exception);
        } catch (InvocationTargetException exception) {
            Throwable cause = exception.getCause();
            if (cause instanceof RuntimeException runtimeException) {
                throw runtimeException;
            }
            if (cause instanceof Error error) {
                throw error;
            }
            throw new IllegalStateException("JavaFX launch failed.", cause);
        }
    }
}
