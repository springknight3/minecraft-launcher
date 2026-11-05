package net.stratalauncher.core.logging;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.logging.FileHandler;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.logging.SimpleFormatter;

public final class LauncherLogging {
    private LauncherLogging() {
    }

    public static void configure(Path dataDirectory) throws IOException {
        Path logDirectory = dataDirectory.toAbsolutePath().normalize().resolve("logs");
        Files.createDirectories(logDirectory);
        FileHandler fileHandler = new FileHandler(logDirectory.resolve("stratalauncher-%g.log").toString(),
                1_000_000, 3, true);
        fileHandler.setFormatter(new SimpleFormatter());
        Logger rootLogger = Logger.getLogger("");
        rootLogger.addHandler(fileHandler);
        Thread.setDefaultUncaughtExceptionHandler((thread, exception) ->
                rootLogger.log(Level.SEVERE, "Uncaught exception on thread " + thread.getName(), exception));
    }
}
