package net.stratalauncher.ui;

import atlantafx.base.theme.PrimerDark;
import atlantafx.base.theme.PrimerLight;
import javafx.application.Application;
import javafx.scene.Scene;

public class ThemeManager {
    private boolean darkMode = true;
    private Scene scene;

    public void apply(Scene scene, boolean darkMode) {
        this.darkMode = darkMode;
        if (scene != null) {
            this.scene = scene;
        }
        if (darkMode) {
            Application.setUserAgentStylesheet(new PrimerDark().getUserAgentStylesheet());
        } else {
            Application.setUserAgentStylesheet(new PrimerLight().getUserAgentStylesheet());
        }

        if (this.scene != null) {
            String commonStylesheet = stylesheetUrl("styles.css");
            String themeStylesheet = stylesheetUrl(darkMode ? "styles-dark.css" : "styles-light.css");
            this.scene.getStylesheets().setAll(commonStylesheet, themeStylesheet);
        }
    }

    private String stylesheetUrl(String stylesheet) {
        var resource = getClass().getResource("/net/stratalauncher/ui/" + stylesheet);
        if (resource == null) {
            throw new IllegalStateException("Launcher stylesheet is missing: " + stylesheet);
        }
        return resource.toExternalForm();
    }

    public boolean isDarkMode() {
        return darkMode;
    }

    public void toggle() {
        apply(scene, !darkMode);
    }
}
