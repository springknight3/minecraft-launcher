package net.stratalauncher.ui;

import javafx.animation.FadeTransition;
import javafx.animation.ParallelTransition;
import javafx.animation.ScaleTransition;
import javafx.animation.TranslateTransition;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.Label;
import javafx.scene.layout.StackPane;
import javafx.scene.layout.VBox;
import javafx.util.Duration;

public final class StartupOverlay extends StackPane {
    private final VBox content = new VBox(12);
    private ParallelTransition entrance;
    private FadeTransition exit;

    public StartupOverlay() {
        getStyleClass().add("startup-overlay");
        Label logo = new Label("STRATA");
        logo.getStyleClass().add("startup-logo");
        Label product = new Label("MINECRAFT LAUNCHER");
        product.getStyleClass().add("startup-product");
        Label warning = new Label("A hobby project — not an official Minecraft or Mojang product.");
        warning.getStyleClass().add("startup-warning");
        warning.setWrapText(true);
        Label copyright = new Label("Minecraft is a trademark of Mojang Studios.");
        copyright.getStyleClass().add("startup-copyright");
        content.getChildren().addAll(logo, product, warning, copyright);
        content.setAlignment(Pos.CENTER);
        content.setPadding(new Insets(36));
        content.setMaxWidth(560);
        getChildren().add(content);
        setAlignment(Pos.CENTER);
        setVisible(false);
    }

    public void playEntrance() {
        if (entrance != null) {
            entrance.stop();
        }
        if (exit != null) {
            exit.stop();
        }
        content.setScaleX(1);
        content.setScaleY(1);
        content.setTranslateY(0);
        setOpacity(1);
        setVisible(true);
        if (MotionConstants.reduceMotion) {
            return;
        }
        FrameTimeOverlay.measure("Startup animation", Duration.seconds(2));
        ScaleTransition scale = new ScaleTransition(MotionConstants.SLOW, content);
        scale.setFromX(0.97);
        scale.setFromY(0.97);
        scale.setToX(1);
        scale.setToY(1);
        scale.setInterpolator(MotionConstants.EMPHASIS);
        TranslateTransition translate = new TranslateTransition(MotionConstants.SLOW, content);
        translate.setFromY(10);
        translate.setToY(0);
        translate.setInterpolator(MotionConstants.EMPHASIS);
        entrance = new ParallelTransition(scale, translate);
        entrance.playFromStart();
    }

    public void fadeOut(Runnable complete) {
        if (entrance != null) {
            entrance.stop();
        }
        if (exit != null) {
            exit.stop();
        }
        if (MotionConstants.reduceMotion) {
            setVisible(false);
            complete.run();
            return;
        }
        exit = new FadeTransition(MotionConstants.NORMAL, this);
        exit.setToValue(0);
        exit.setInterpolator(MotionConstants.EXPRESS);
        exit.setOnFinished(event -> {
            setVisible(false);
            complete.run();
        });
        exit.playFromStart();
    }
}
