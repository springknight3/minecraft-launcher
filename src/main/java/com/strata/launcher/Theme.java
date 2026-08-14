package com.strata.launcher;

import javafx.scene.paint.Color;

public final class Theme {

    private static final Theme INSTANCE = new Theme();

    public static Theme get() {
        return INSTANCE;
    }

    private Theme() {}

    public Color BACKGROUND = Color.web("#1e1e2e");
    public Color SIDEBAR = Color.web("#11111b");
    public Color DECORATION = Color.web("#181825");
    public Color FOREGROUND = Color.web("#cdd6f4");
    public Color ACTION = Color.web("#89b4fa");
    public Color ACTION_SECONDARY = Color.web("#74c7ec");
    public Color ACTION_FOREGROUND = Color.web("#1e1e2e");
    public boolean BLUR_ENABLED = false;
}
