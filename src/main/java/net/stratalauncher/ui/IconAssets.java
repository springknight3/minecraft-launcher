package net.stratalauncher.ui;

import javafx.scene.shape.SVGPath;
import javafx.scene.Node;
import javafx.scene.Parent;
import javafx.scene.control.Button;
import javafx.scene.control.Labeled;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class IconAssets {
    private static final Pattern PATH = Pattern.compile("<path[^>]*d=\"([^\"]+)\"");

    private IconAssets() {
    }

    public static SVGPath graphic(String name, boolean darkMode) {
        String variant = darkMode ? "light" : "dark";
        String resourcePath = "/net/stratalauncher/ui/icons/" + name + "-" + variant + ".svg";
        try (InputStream stream = IconAssets.class.getResourceAsStream(resourcePath)) {
            if (stream == null) {
                throw new IllegalStateException("Launcher icon is missing: " + resourcePath);
            }
            String svg = new String(stream.readAllBytes(), StandardCharsets.UTF_8);
            Matcher path = PATH.matcher(svg);
            Matcher fill = Pattern.compile("fill=\"(#[A-Fa-f0-9]{6})\"").matcher(svg);
            if (!path.find() || !fill.find()) {
                throw new IllegalStateException("Launcher icon is invalid: " + resourcePath);
            }
            SVGPath icon = new SVGPath();
            icon.setContent(path.group(1));
            icon.setFill(javafx.scene.paint.Color.web(fill.group(1)));
            icon.setScaleX(0.85);
            icon.setScaleY(0.85);
            return icon;
        } catch (IOException exception) {
            throw new IllegalStateException("Could not read launcher icon: " + resourcePath, exception);
        }
    }

    public static void apply(Button button, String name, boolean darkMode) {
        apply((Labeled) button, name, darkMode);
    }

    public static void apply(Labeled control, String name, boolean darkMode) {
        control.getProperties().put("strata-icon", name);
        control.setGraphic(graphic(name, darkMode));
        control.setContentDisplay(javafx.scene.control.ContentDisplay.LEFT);
        control.setGraphicTextGap(8);
    }

    public static void refresh(Node root, boolean darkMode) {
        if (root instanceof Labeled labeled) {
            Object icon = labeled.getProperties().get("strata-icon");
            if (icon instanceof String name) {
                apply(labeled, name, darkMode);
            }
        }
        if (root instanceof Parent parent) {
            for (Node child : parent.getChildrenUnmodifiable()) {
                refresh(child, darkMode);
            }
        }
    }
}
