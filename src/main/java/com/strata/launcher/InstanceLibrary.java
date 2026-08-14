package com.strata.launcher;

import javafx.application.Platform;
import javafx.concurrent.Task;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.control.ScrollPane;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Priority;
import javafx.scene.layout.StackPane;
import javafx.scene.layout.VBox;
import javafx.scene.paint.Color;
import javafx.scene.shape.Rectangle;
import javafx.scene.text.Font;
import javafx.scene.text.Text;
import javafx.stage.Stage;

import java.util.List;

public class InstanceLibrary extends Stage {

    private final InstanceManager manager;
    private final VBox listContainer;

    public InstanceLibrary(InstanceManager manager) {
        this.manager = manager;

        Theme theme = Theme.get();

        VBox root = new VBox(0);
        root.setStyle("-fx-background-color: " + toHex(theme.BACKGROUND) + ";");

        Rectangle clip = new Rectangle();
        clip.widthProperty().bind(root.widthProperty());
        clip.heightProperty().bind(root.heightProperty());

        StackPane clipPane = new StackPane();
        clipPane.setClip(clip);

        VBox content = new VBox(8);
        content.setPadding(new Insets(16));

        Text title = new Text("Instance Library");
        title.setFill(theme.FOREGROUND);
        title.setFont(Font.font(18));

        listContainer = new VBox(4);
        refreshList();

        ScrollPane scroll = new ScrollPane(listContainer);
        scroll.setFitToWidth(true);
        scroll.setStyle("-fx-background-color: transparent; -fx-background: transparent;");
        scroll.setPrefHeight(400);

        content.getChildren().addAll(title, scroll);
        clipPane.getChildren().add(content);

        root.getChildren().add(clipPane);

        Scene scene = new Scene(root, 480, 500);
        setScene(scene);
        setTitle("Instance Library");
    }

    private void refreshList() {
        Theme theme = Theme.get();
        listContainer.getChildren().clear();

        List<Instance> instances = manager.getInstances();
        if (instances.isEmpty()) {
            Label empty = new Label("No instances. Click \"+\" to create one.");
            empty.setTextFill(theme.FOREGROUND);
            empty.setOpacity(0.5);
            listContainer.getChildren().add(empty);
            return;
        }

        for (Instance inst : instances) {
            listContainer.getChildren().add(createInstanceRow(inst));
        }
    }

    private HBox createInstanceRow(Instance inst) {
        Theme theme = Theme.get();

        StackPane rowBg = new StackPane();
        rowBg.setStyle("-fx-background-color: " + toHex(theme.DECORATION) + "; -fx-background-radius: 8;");
        rowBg.setPadding(new Insets(12));

        VBox info = new VBox(2);
        Text name = new Text(inst.getName());
        name.setFill(theme.FOREGROUND);
        name.setFont(Font.font(14));

        Text version = new Text(inst.getVersion());
        version.setFill(theme.FOREGROUND);
        version.setOpacity(0.6);
        version.setFont(Font.font(11));

        info.getChildren().addAll(name, version);

        Button settingsBtn = new Button("\u2699");
        settingsBtn.setTextFill(theme.FOREGROUND);
        settingsBtn.setFont(Font.font(12));
        settingsBtn.setStyle("-fx-background-color: transparent; -fx-cursor: hand;");
        settingsBtn.setOnAction(e -> {
            InstanceSettingsPopup popup = new InstanceSettingsPopup(inst, manager);
            popup.show();
        });

        Button launchBtn = new Button("\u25B6");
        launchBtn.setTextFill(theme.ACTION);
        launchBtn.setFont(Font.font(12));
        launchBtn.setStyle("-fx-background-color: transparent; -fx-cursor: hand;");
        launchBtn.setOnAction(e -> {
            Task<Void> launchTask = new Task<>() {
                @Override
                protected Void call() throws Exception {
                    GameExecutor executor = new GameExecutor();
                    executor.launch(inst, (status, percent) -> {});
                    return null;
                }

                @Override
                protected void failed() {
                    Platform.runLater(() -> {
                        javafx.scene.control.Alert alert = new javafx.scene.control.Alert(javafx.scene.control.Alert.AlertType.ERROR);
                        alert.setTitle("Launch Failed");
                        alert.setHeaderText(null);
                        alert.setContentText("Failed to launch: " + getException().getMessage());
                        alert.showAndWait();
                    });
                }
            };
            Thread t = new Thread(launchTask);
            t.setDaemon(true);
            t.start();
        });

        Button deleteBtn = new Button("\u2715");
        deleteBtn.setTextFill(Color.web("#f38ba8"));
        deleteBtn.setFont(Font.font(12));
        deleteBtn.setStyle("-fx-background-color: transparent; -fx-cursor: hand;");
        deleteBtn.setOnAction(e -> {
            manager.removeInstance(inst);
            refreshList();
        });

        HBox row = new HBox(8, info);
        row.setAlignment(Pos.CENTER_LEFT);
        HBox.setHgrow(info, Priority.ALWAYS);

        StackPane rightSlot = new StackPane(new HBox(4, settingsBtn, launchBtn, deleteBtn));
        rightSlot.setAlignment(Pos.CENTER_RIGHT);

        HBox.setHgrow(rightSlot, Priority.ALWAYS);

        row.getChildren().add(rightSlot);

        rowBg.getChildren().add(row);

        HBox wrapper = new HBox(rowBg);
        wrapper.setPadding(new Insets(0, 8, 0, 0));
        return wrapper;
    }

    private String toHex(Color color) {
        return String.format("#%02X%02X%02X",
                (int) (color.getRed() * 255),
                (int) (color.getGreen() * 255),
                (int) (color.getBlue() * 255));
    }
}
