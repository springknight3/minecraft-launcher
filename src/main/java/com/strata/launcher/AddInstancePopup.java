package com.strata.launcher;

import javafx.concurrent.Task;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.control.ComboBox;
import javafx.scene.control.Label;
import javafx.scene.control.TextField;
import javafx.scene.layout.HBox;
import javafx.scene.layout.VBox;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;
import javafx.scene.text.Text;
import javafx.stage.Stage;

import java.util.List;

public class AddInstancePopup extends Stage {

    private final InstanceManager manager;
    private final Runnable onCreated;

    public AddInstancePopup(InstanceManager manager, Runnable onCreated) {
        this.manager = manager;
        this.onCreated = onCreated;

        Theme theme = Theme.get();

        VBox root = new VBox(12);
        root.setPadding(new Insets(20));
        root.setStyle("-fx-background-color: " + toHex(theme.BACKGROUND) + ";");

        Text title = new Text("Create Instance");
        title.setFill(theme.FOREGROUND);
        title.setFont(Font.font(18));

        Label nameLabel = new Label("Name");
        nameLabel.setTextFill(theme.FOREGROUND);
        nameLabel.setFont(Font.font(12));

        TextField nameField = new TextField();
        nameField.setPromptText("My Instance");
        nameField.setStyle("-fx-background-color: " + toHex(theme.DECORATION) + "; -fx-text-fill: " + toHex(theme.FOREGROUND) + "; -fx-prompt-text-fill: " + toHex(theme.FOREGROUND) + "80; -fx-background-radius: 6; -fx-padding: 8;");
        nameField.setMaxWidth(Double.MAX_VALUE);

        Label versionLabel = new Label("Version");
        versionLabel.setTextFill(theme.FOREGROUND);
        versionLabel.setFont(Font.font(12));

        ComboBox<String> versionBox = new ComboBox<>();
        versionBox.setStyle("-fx-background-color: " + toHex(theme.DECORATION) + "; -fx-background-radius: 6;");
        versionBox.setMaxWidth(Double.MAX_VALUE);
        versionBox.setPromptText("Loading versions...");
        versionBox.setEditable(false);

        Label statusLabel = new Label("");
        statusLabel.setTextFill(theme.FOREGROUND);
        statusLabel.setFont(Font.font(11));

        Task<List<String>> versionTask = new Task<>() {
            @Override
            protected List<String> call() {
                return InstanceManager.fetchVersions();
            }

            @Override
            protected void succeeded() {
                versionBox.getItems().addAll(getValue());
                if (!versionBox.getItems().isEmpty()) {
                    versionBox.getSelectionModel().selectFirst();
                }
            }

            @Override
            protected void failed() {
                statusLabel.setText("Failed to load versions");
            }
        };
        Thread versionThread = new Thread(versionTask);
        versionThread.setDaemon(true);
        versionThread.start();

        Button cancelBtn = createButton("Cancel", theme.DECORATION, theme.FOREGROUND);
        cancelBtn.setOnAction(e -> close());

        Button createBtn = createButton("Create", theme.ACTION, theme.ACTION_FOREGROUND);
        createBtn.setOnAction(e -> {
            String name = nameField.getText().trim();
            String version = versionBox.getValue();
            if (name.isEmpty() || version == null) return;
            Instance instance = new Instance(name, version);
            manager.addInstance(instance);
            close();
            DownloadWindow dlWindow = new DownloadWindow(instance, manager, () -> {
                if (onCreated != null) onCreated.run();
            });
            dlWindow.show();
        });

        HBox buttons = new HBox(8, cancelBtn, createBtn);
        buttons.setAlignment(Pos.CENTER_RIGHT);

        root.getChildren().addAll(title, nameLabel, nameField, versionLabel, versionBox, statusLabel, buttons);

        Scene scene = new Scene(root, 360, 320);
        setScene(scene);
        setTitle("Create Instance");
        setResizable(false);
    }

    private Button createButton(String text, Color bg, Color fg) {
        Button btn = new Button(text);
        btn.setTextFill(fg);
        btn.setFont(Font.font(12));
        btn.setStyle("-fx-background-color: " + toHex(bg) + "; -fx-background-radius: 6; -fx-padding: 8 16;");
        return btn;
    }

    private String toHex(Color color) {
        return String.format("#%02X%02X%02X",
                (int) (color.getRed() * 255),
                (int) (color.getGreen() * 255),
                (int) (color.getBlue() * 255));
    }
}
