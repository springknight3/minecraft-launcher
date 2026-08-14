package com.strata.launcher;

import javafx.animation.KeyFrame;
import javafx.animation.KeyValue;
import javafx.animation.Timeline;
import javafx.beans.property.DoubleProperty;
import javafx.beans.property.SimpleDoubleProperty;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.layout.HBox;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;
import javafx.scene.text.Text;
import javafx.util.Duration;

public class NavButton extends HBox {

    private final DoubleProperty fade = new SimpleDoubleProperty(0);
    private final Color baseColor;
    private final Color hoverColor;

    public NavButton(String label, Color baseColor, Color hoverColor, Color textColor) {
        this.baseColor = baseColor;
        this.hoverColor = hoverColor;

        setAlignment(Pos.CENTER_LEFT);
        setPadding(new Insets(0, 12, 0, 12));
        setPrefHeight(48);
        setMaxWidth(Double.MAX_VALUE);
        updateBackground(baseColor);

        Text text = new Text(label);
        text.setFill(textColor);
        text.setFont(Font.font(14));

        getChildren().add(text);

        fade.addListener((obs, oldVal, newVal) ->
                updateBackground(baseColor.interpolate(hoverColor, newVal.doubleValue())));

        Timeline timeline = new Timeline(
                new KeyFrame(Duration.ZERO, new KeyValue(fade, 0)),
                new KeyFrame(Duration.millis(200), new KeyValue(fade, 1)));
        timeline.setCycleCount(1);

        setOnMouseEntered(e -> { timeline.stop(); timeline.setRate(1); timeline.play(); });
        setOnMouseExited(e -> { timeline.stop(); timeline.setRate(-1); timeline.play(); });
    }

    private void updateBackground(Color color) {
        String hex = String.format("#%02X%02X%02X",
                (int) (color.getRed() * 255),
                (int) (color.getGreen() * 255),
                (int) (color.getBlue() * 255));
        setStyle("-fx-background-color: " + hex + "; -fx-background-radius: 8;");
    }
}
