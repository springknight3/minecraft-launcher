package net.stratalauncher.ui;

import javafx.animation.ParallelTransition;
import javafx.animation.ScaleTransition;
import javafx.animation.TranslateTransition;
import javafx.geometry.Pos;
import javafx.scene.Node;
import javafx.scene.control.CheckBox;
import javafx.scene.control.Label;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Region;
import javafx.scene.layout.StackPane;
import javafx.scene.shape.Circle;
import javafx.util.Duration;

public final class ToggleSwitchStyle {
    private static final String APPLIED_KEY = "strata-toggle-switch";
    private static final String LABEL_KEY = "strata-toggle-label";
    private static final String ICON_KEY = "strata-toggle-icon";
    private static final Duration TOGGLE_DURATION = Duration.millis(180);

    private ToggleSwitchStyle() {
    }

    public static void apply(CheckBox checkBox) {
        Object existing = checkBox.getProperties().get(APPLIED_KEY);
        if (existing instanceof HBox) {
            return;
        }

        String text = checkBox.getText();
        Node icon = checkBox.getGraphic();
        Object iconName = checkBox.getProperties().get("strata-icon");
        Label label = new Label(text);
        label.getStyleClass().add("toggle-switch-label");
        HBox content = new HBox(10);
        content.setAlignment(Pos.CENTER_LEFT);
        if (icon != null) {
            content.getChildren().add(icon);
        }
        content.getChildren().add(label);

        Region track = new Region();
        track.getStyleClass().add("toggle-switch-track");
        Circle thumb = new Circle(8);
        thumb.getStyleClass().add("toggle-switch-thumb");
        StackPane graphic = new StackPane(track, thumb);
        graphic.getStyleClass().add("toggle-switch-graphic");
        graphic.setMinSize(44, 26);
        graphic.setPrefSize(44, 26);
        graphic.setMaxSize(44, 26);
        content.getProperties().put("strata-switch-graphic", graphic);
        content.getChildren().add(graphic);

        checkBox.getProperties().put(APPLIED_KEY, content);
        checkBox.getProperties().put(LABEL_KEY, label);
        if (iconName instanceof String name) {
            checkBox.getProperties().put(ICON_KEY, name);
        }
        checkBox.getStyleClass().add("toggle-switch");
        checkBox.setAccessibleText(text);
        checkBox.setText("");
        checkBox.setContentDisplay(javafx.scene.control.ContentDisplay.GRAPHIC_ONLY);
        checkBox.setGraphic(content);
        thumb.setTranslateX(checkBox.isSelected() ? 9 : -9);
        checkBox.selectedProperty().addListener((observable, oldValue, selected) ->
                animateToggle(thumb, track, selected));
    }

    public static void setIcon(CheckBox checkBox, String name, boolean darkMode) {
        checkBox.getProperties().put("strata-icon", name);
        checkBox.getProperties().put(ICON_KEY, name);
        if (checkBox.getProperties().get(APPLIED_KEY) instanceof HBox content) {
            Node label = (Node) checkBox.getProperties().get(LABEL_KEY);
            Node switchGraphic = (Node) content.getProperties().get("strata-switch-graphic");
            content.getChildren().setAll(IconAssets.graphic(name, darkMode), label, switchGraphic);
            checkBox.setGraphic(content);
        } else {
            IconAssets.apply(checkBox, name, darkMode);
        }
    }

    public static void refresh(Node root, boolean darkMode) {
        if (root instanceof CheckBox checkBox
                && checkBox.getProperties().get(APPLIED_KEY) instanceof HBox content) {
            Object name = checkBox.getProperties().get(ICON_KEY);
            Label label = (Label) checkBox.getProperties().get(LABEL_KEY);
            Node icon = name instanceof String iconName ? IconAssets.graphic(iconName, darkMode) : null;
            content.getChildren().clear();
            if (icon != null) {
                content.getChildren().add(icon);
            }
            content.getChildren().add(label);
            Node switchGraphic = (Node) content.getProperties().get("strata-switch-graphic");
            content.getChildren().add(switchGraphic);
            checkBox.setGraphic(content);
        }
        if (root instanceof javafx.scene.Parent parent) {
            for (Node child : parent.getChildrenUnmodifiable()) {
                refresh(child, darkMode);
            }
        }
    }

    private static void animateToggle(Circle thumb, Region track, boolean selected) {
        double target = selected ? 9 : -9;
        if (MotionConstants.reduceMotion) {
            thumb.setTranslateX(target);
            return;
        }
        TranslateTransition slide = new TranslateTransition(TOGGLE_DURATION, thumb);
        slide.setToX(target);
        slide.setInterpolator(MotionConstants.EMPHASIS);
        ScaleTransition bounce = new ScaleTransition(TOGGLE_DURATION, thumb);
        bounce.setFromX(1);
        bounce.setToX(1.12);
        bounce.setAutoReverse(true);
        bounce.setCycleCount(2);
        bounce.setInterpolator(MotionConstants.STANDARD);
        ParallelTransition transition = new ParallelTransition(slide, bounce);
        transition.play();
        track.setOpacity(0.86);
        transition.setOnFinished(event -> track.setOpacity(1));
    }
}
