package net.stratalauncher.ui;

import javafx.scene.text.Font;

import java.io.IOException;
import java.io.InputStream;
import java.util.List;

public final class UiTypography {
    private static final List<String> FONTS = List.of(
            "/net/stratalauncher/ui/fonts/DM-Sans.ttf",
            "/net/stratalauncher/ui/fonts/Outfit.ttf",
            "/net/stratalauncher/ui/fonts/JetBrains-Mono.ttf");

    private UiTypography() {
    }

    public static void loadBundledFonts() {
        for (String resource : FONTS) {
            try (InputStream stream = UiTypography.class.getResourceAsStream(resource)) {
                if (stream == null || Font.loadFont(stream, 14) == null) {
                    System.err.println("Could not load bundled font " + resource
                            + "; using the stylesheet fallback font stack.");
                }
            } catch (IOException | IllegalArgumentException exception) {
                System.err.println("Could not read bundled font " + resource
                        + "; using the stylesheet fallback font stack: " + exception.getMessage());
            }
        }
    }
}
