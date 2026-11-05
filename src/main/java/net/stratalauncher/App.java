package net.stratalauncher;

import javafx.application.Application;
import javafx.scene.Scene;
import javafx.scene.layout.StackPane;
import javafx.geometry.Pos;
import javafx.stage.Stage;
import javafx.animation.PauseTransition;
import javafx.application.Platform;
import javafx.util.Duration;
import net.stratalauncher.core.auth.AccountManager;
import net.stratalauncher.core.auth.LauncherSettings;
import net.stratalauncher.core.auth.MicrosoftAuthService;
import net.stratalauncher.core.auth.NativeCredentialStore;
import net.stratalauncher.core.download.DownloadManager;
import net.stratalauncher.core.instance.InstanceManager;
import net.stratalauncher.core.logging.LauncherLogging;
import net.stratalauncher.core.loader.LoaderInstaller;
import net.stratalauncher.core.launch.GameLauncher;
import net.stratalauncher.core.mojang.MojangManifestClient;
import net.stratalauncher.core.modrinth.ContentManager;
import net.stratalauncher.core.modrinth.ModrinthClient;
import net.stratalauncher.core.runtime.JavaRuntimeManager;
import net.stratalauncher.core.server.DevServerManager;
import net.stratalauncher.core.server.ServerModrinthManager;
import net.stratalauncher.ui.MainLayout;
import net.stratalauncher.ui.FlyoutManager;
import net.stratalauncher.ui.FrameTimeOverlay;
import net.stratalauncher.ui.AnimationBenchmark;
import net.stratalauncher.ui.MotionConstants;
import net.stratalauncher.ui.StartupOverlay;
import net.stratalauncher.ui.ThemeManager;
import net.stratalauncher.ui.UiTypography;
import net.stratalauncher.ui.animations.ButtonMotion;

import java.io.IOException;
import java.nio.file.Path;
import java.util.concurrent.CompletableFuture;
import java.util.logging.Level;
import java.util.logging.Logger;

public class App extends Application {
    private static final Logger LOGGER = Logger.getLogger(App.class.getName());
    private final ThemeManager themeManager = new ThemeManager();
    private final DownloadManager downloadManager = new DownloadManager();
    private DevServerManager serverManager;
    private FrameTimeOverlay frameTimeOverlay;
    private Path dataDirectory;

    @Override
    public void start(Stage stage) throws IOException {
        UiTypography.loadBundledFonts();
        InstanceManager instanceManager = new InstanceManager();
        dataDirectory = instanceManager.dataDirectory();
        LauncherLogging.configure(instanceManager.dataDirectory());
        AccountManager accountManager = new AccountManager(instanceManager.dataDirectory(),
                new NativeCredentialStore());
        LauncherSettings settings = new LauncherSettings(instanceManager.dataDirectory());
        frameTimeOverlay = new FrameTimeOverlay(settings, settings.frameTimeOverlayEnabled()
                || Boolean.getBoolean("stratalauncher.debugFrameTime"));
        MotionConstants.reduceMotion = settings.reduceMotion();
        MicrosoftAuthService microsoftAuthService = new MicrosoftAuthService();
        MojangManifestClient manifestClient = new MojangManifestClient();
        LoaderInstaller loaderInstaller = new LoaderInstaller(instanceManager.dataDirectory(), downloadManager);
        JavaRuntimeManager javaRuntimeManager = new JavaRuntimeManager(downloadManager,
                instanceManager.dataDirectory(), settings);
        GameLauncher gameLauncher = new GameLauncher(instanceManager, manifestClient, downloadManager,
                loaderInstaller, settings, javaRuntimeManager);
        ModrinthClient modrinthClient = new ModrinthClient("StrataLauncher", "0.1.0");
        ContentManager contentManager = new ContentManager(modrinthClient, downloadManager);
        serverManager = new DevServerManager(instanceManager.dataDirectory(), downloadManager, manifestClient,
                javaRuntimeManager);
        ServerModrinthManager serverModrinthManager = new ServerModrinthManager(modrinthClient, downloadManager,
                serverManager);
        FlyoutManager flyouts = new FlyoutManager();
        MainLayout root = new MainLayout(themeManager, instanceManager, manifestClient, gameLauncher,
                accountManager, microsoftAuthService, settings, loaderInstaller, modrinthClient, contentManager,
                serverManager, serverModrinthManager, javaRuntimeManager, frameTimeOverlay, flyouts);
        StartupOverlay startupOverlay = new StartupOverlay();
        root.setStartupOverlay(startupOverlay);
        StackPane sceneRoot = new StackPane(root, flyouts.overlay(), startupOverlay, frameTimeOverlay);
        StackPane.setAlignment(frameTimeOverlay, Pos.TOP_RIGHT);
        StackPane.setMargin(frameTimeOverlay, new javafx.geometry.Insets(10));
        ButtonMotion.attach(sceneRoot);
        Scene scene = new Scene(sceneRoot, 1280, 780);
        frameTimeOverlay.attach(scene);
        themeManager.apply(scene, settings.darkMode());
        root.refreshIconTheme();

        stage.setTitle("StrataLauncher");
        stage.setScene(scene);
        stage.setMinWidth(980);
        stage.setMinHeight(680);
        stage.show();
        if (getParameters().getNamed().containsKey("bench")) {
            AnimationBenchmark benchmark = new AnimationBenchmark(stage, scene, root, flyouts, startupOverlay,
                    getParameters().getNamed().get("bench"), getParameters().getNamed());
            benchmark.start();
            return;
        }
        PauseTransition visiblePulse = new PauseTransition(Duration.millis(180));
        visiblePulse.setOnFinished(event -> {
            startupOverlay.setVisible(true);
            startupOverlay.playEntrance();
            PauseTransition minimumDuration = new PauseTransition(Duration.seconds(1));
            CompletableFuture<Void> initialization = root.initializeStartup();
            CompletableFuture<Void> animationMinimum = new CompletableFuture<>();
            minimumDuration.setOnFinished(done -> animationMinimum.complete(null));
            minimumDuration.play();
            CompletableFuture.allOf(initialization, animationMinimum).whenComplete((ignored, failure) ->
                    Platform.runLater(() -> {
                        if (failure != null) {
                            Throwable cause = failure;
                            while (cause.getCause() != null && cause.getCause() != cause) {
                                cause = cause.getCause();
                            }
                            LOGGER.log(Level.SEVERE, "Startup initialization failed.", failure);
                            String message = cause.getMessage() == null ? cause.toString() : cause.getMessage();
                            flyouts.showError("Startup initialization failed", message);
                        }
                        startupOverlay.fadeOut(() -> { });
                    }));
        });
        visiblePulse.play();
    }

    @Override
    public void stop() {
        if (frameTimeOverlay != null) {
            frameTimeOverlay.stop();
            if (dataDirectory != null && frameTimeOverlay.isVisible()) {
                try {
                    frameTimeOverlay.writeReport(dataDirectory.resolve("logs"));
                } catch (IOException exception) {
                    LOGGER.log(Level.WARNING, "Could not write frame-time diagnostic report.", exception);
                }
            }
        }
        if (serverManager != null) {
            serverManager.close();
        }
        downloadManager.close();
    }

}
