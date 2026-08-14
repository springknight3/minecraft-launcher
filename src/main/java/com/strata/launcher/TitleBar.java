package com.strata.launcher;

import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.Region;
import javafx.scene.layout.StackPane;
import javafx.scene.paint.Color;
import javafx.scene.shape.Rectangle;
import javafx.scene.text.Font;
import javafx.scene.text.Text;
import javafx.stage.Stage;

public class TitleBar extends StackPane {

    private double dragX;
    private double dragY;

    public TitleBar(Stage stage, double width) {
        Theme theme = Theme.get();

        Rectangle bg = new Rectangle();
        bg.setFill(theme.DECORATION);
        bg.widthProperty().bind(stage.widthProperty());
        bg.heightProperty().bind(super.heightProperty());
        bg.setMouseTransparent(true);

        HBox content = new HBox();
        content.setAlignment(Pos.CENTER_LEFT);
        content.setPadding(new Insets(0, 8, 0, 12));
        content.setPickOnBounds(false);

        Text title = new Text("Strata Launcher");
        title.setFill(theme.FOREGROUND);
        title.setFont(Font.font(20));

        Region spacer = new Region();
        HBox.setHgrow(spacer, Priority.ALWAYS);

        Text closeBtn = new Text("\u2715");
        closeBtn.setFill(theme.FOREGROUND);
        closeBtn.setFont(Font.font(14));
        closeBtn.setOnMouseClicked(e -> stage.close());
        closeBtn.setOnMouseEntered(e -> closeBtn.setFill(Color.web("#f38ba8")));
        closeBtn.setOnMouseExited(e -> closeBtn.setFill(theme.FOREGROUND));

        content.getChildren().addAll(title, spacer, closeBtn);

        setOnMousePressed(e -> {
            dragX = e.getScreenX() - stage.getX();
            dragY = e.getScreenY() - stage.getY();
        });

        setOnMouseDragged(e -> {
            stage.setX(e.getScreenX() - dragX);
            stage.setY(e.getScreenY() - dragY);
        });

        getChildren().addAll(bg, content);
    }
}
