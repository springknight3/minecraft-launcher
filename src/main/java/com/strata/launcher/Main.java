package com.strata.launcher;

import javafx.application.Application;
import javafx.geometry.Insets;
import javafx.scene.Scene;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.StackPane;
import javafx.scene.layout.VBox;
import javafx.scene.shape.Rectangle;
import javafx.stage.Stage;
import javafx.stage.StageStyle;

public class Main extends Application {

    @Override
    public void start(Stage stage) {
        Theme theme = Theme.get();
        InstanceManager instanceManager = new InstanceManager();

        TitleBar titleBar = new TitleBar(stage, 640);

        VBox sidebar = new VBox(8);
        sidebar.setStyle("-fx-background-color: " + toHex(theme.SIDEBAR) + ";");
        sidebar.setPadding(new Insets(8));
        sidebar.setMinWidth(0);

        NavButton dashboardBtn = new NavButton("Dashboard", theme.ACTION, theme.ACTION_SECONDARY, theme.ACTION_FOREGROUND);
        NavButton contentBtn = new NavButton("Content", theme.ACTION, theme.ACTION_SECONDARY, theme.ACTION_FOREGROUND);

        NavButton instancesBtn = new NavButton("Instances", theme.ACTION, theme.ACTION_SECONDARY, theme.ACTION_FOREGROUND);
        instancesBtn.setOnMouseClicked(e -> {
            InstanceLibrary library = new InstanceLibrary(instanceManager);
            library.show();
        });

        NavButton addBtn = new NavButton("+", theme.ACTION, theme.ACTION_SECONDARY, theme.ACTION_FOREGROUND);
        addBtn.setMaxWidth(48);
        addBtn.setOnMouseClicked(e -> {
            AddInstancePopup popup = new AddInstancePopup(instanceManager, () -> {});
            popup.show();
        });

        HBox span = new HBox(8, instancesBtn, addBtn);
        HBox.setHgrow(instancesBtn, Priority.ALWAYS);
        span.setPickOnBounds(false);

        sidebar.getChildren().addAll(dashboardBtn, contentBtn, span);

        double sidebarMargin = 8;

        StackPane content = new StackPane();
        content.setPickOnBounds(false);

        HBox body = new HBox(sidebar, content);
        HBox.setMargin(sidebar, new Insets(sidebarMargin, 0, sidebarMargin, sidebarMargin));
        body.setPickOnBounds(false);

        VBox layout = new VBox(titleBar, body);
        VBox.setVgrow(body, Priority.ALWAYS);
        layout.setPickOnBounds(false);

        StackPane root = new StackPane();
        root.setStyle("-fx-background-color: " + toHex(theme.BACKGROUND) + ";");
        root.getChildren().add(layout);

        Rectangle clip = new Rectangle();
        clip.widthProperty().bind(root.widthProperty());
        clip.heightProperty().bind(root.heightProperty());
        root.setClip(clip);

        Scene scene = new Scene(root, 1280, 720);

        double initialW = Math.min(720 * 0.3, 500);
        sidebar.setPrefWidth(initialW);
        sidebar.setMaxWidth(initialW);

        stage.widthProperty().addListener((obs, oldVal, newVal) -> {
            double w = Math.min(newVal.doubleValue() * 0.3, 500);
            sidebar.setPrefWidth(w);
            sidebar.setMaxWidth(w);
        });

        stage.setScene(scene);
        stage.setResizable(true);
        stage.centerOnScreen();
        stage.show();
    }

    private String toHex(javafx.scene.paint.Color color) {
        return String.format("#%02X%02X%02X",
                (int) (color.getRed() * 255),
                (int) (color.getGreen() * 255),
                (int) (color.getBlue() * 255));
    }

    public static void main(String[] args) {
        launch(args);
    }
}
