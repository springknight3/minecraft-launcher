package net.stratalauncher.ui.components;

import javafx.animation.KeyFrame;
import javafx.animation.KeyValue;
import javafx.animation.Timeline;
import javafx.scene.layout.StackPane;
import javafx.scene.shape.Circle;
import javafx.util.Duration;
import net.stratalauncher.ui.MotionConstants;

public final class StatusIndicator extends StackPane {
    private final Circle light = new Circle(5);
    private final Circle glow = new Circle(9);
    private Timeline runningAnimation;
    private Status status;

    public StatusIndicator() {
        getStyleClass().add("status-indicator");
        glow.getStyleClass().add("status-glow");
        light.getStyleClass().add("status-light");
        getChildren().addAll(glow, light);
        setStatus(Status.GREY);
    }

    public void setStatus(Status status) {
        boolean shouldAnimate = status == Status.GREEN && !MotionConstants.reduceMotion;
        if (this.status == status && shouldAnimate == (runningAnimation != null)) {
            return;
        }
        this.status = status;
        if (runningAnimation != null) {
            runningAnimation.stop();
            runningAnimation = null;
        }
        getStyleClass().removeAll("status-grey", "status-blue", "status-green", "status-red");
        getStyleClass().add("status-" + status.name().toLowerCase());
        glow.setOpacity(0);
        glow.setScaleX(0.55);
        glow.setScaleY(0.55);
        if (shouldAnimate) {
            Duration halfPulse = Duration.seconds(1.2);
            runningAnimation = new Timeline(
                    new KeyFrame(Duration.ZERO,
                            new KeyValue(light.opacityProperty(), 0.55, MotionConstants.STANDARD),
                            new KeyValue(glow.opacityProperty(), 0, MotionConstants.STANDARD),
                            new KeyValue(glow.scaleXProperty(), 0.55, MotionConstants.STANDARD),
                            new KeyValue(glow.scaleYProperty(), 0.55, MotionConstants.STANDARD)),
                    new KeyFrame(halfPulse,
                            new KeyValue(light.opacityProperty(), 1, MotionConstants.STANDARD),
                            new KeyValue(glow.opacityProperty(), 0.45, MotionConstants.STANDARD),
                            new KeyValue(glow.scaleXProperty(), 1.25, MotionConstants.STANDARD),
                            new KeyValue(glow.scaleYProperty(), 1.25, MotionConstants.STANDARD)));
            runningAnimation.setAutoReverse(true);
            runningAnimation.setCycleCount(Timeline.INDEFINITE);
            runningAnimation.play();
        } else {
            light.setOpacity(1);
        }
    }

    public enum Status {
        GREY,
        BLUE,
        GREEN,
        RED
    }
}
