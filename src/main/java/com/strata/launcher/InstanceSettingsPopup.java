package com.strata.launcher;

import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.Scene;
import javafx.scene.control.Button;
import javafx.scene.control.CheckBox;
import javafx.scene.control.Label;
import javafx.scene.control.TextField;
import javafx.scene.layout.HBox;
import javafx.scene.layout.VBox;
import javafx.scene.paint.Color;
import javafx.scene.text.Font;
import javafx.scene.text.Text;
import javafx.stage.Stage;

public class InstanceSettingsPopup extends Stage {

    private final Instance instance;
    private final InstanceManager manager;

    public InstanceSettingsPopup(Instance instance, InstanceManager manager) {
        this.instance = instance;
        this.manager = manager;

        Theme theme = Theme.get();

        VBox root = new VBox(10);
        root.setPadding(new Insets(20));
        root.setStyle("-fx-background-color: " + toHex(theme.BACKGROUND) + ";");

        Text title = new Text("Settings: " + instance.getName());
        title.setFill(theme.FOREGROUND);
        title.setFont(Font.font(16));

        Label usernameLabel = new Label("Username");
        usernameLabel.setTextFill(theme.FOREGROUND);
        TextField usernameField = new TextField(instance.getUsername());
        styleField(usernameField, theme);

        Label memoryLabel = new Label("Max Memory (MB)");
        memoryLabel.setTextFill(theme.FOREGROUND);
        TextField memoryField = new TextField(String.valueOf(instance.getMaxMemory()));
        styleField(memoryField, theme);

        Label javaLabel = new Label("Java Path");
        javaLabel.setTextFill(theme.FOREGROUND);
        TextField javaField = new TextField(instance.getJavaPath());
        styleField(javaField, theme);

        CheckBox separateJvmBox = new CheckBox("Run in separate JVM");
        separateJvmBox.setSelected(instance.isSeparateJvm());
        separateJvmBox.setTextFill(theme.FOREGROUND);

        Button cancelBtn = createButton("Cancel", theme.DECORATION, theme.FOREGROUND);
        cancelBtn.setOnAction(e -> close());

        Button saveBtn = createButton("Save", theme.ACTION, theme.ACTION_FOREGROUND);
        saveBtn.setOnAction(e -> {
            instance.setUsername(usernameField.getText().trim());
            try { instance.setMaxMemory(Integer.parseInt(memoryField.getText().trim())); } catch (NumberFormatException ignored) {}
            instance.setJavaPath(javaField.getText().trim());
            instance.setSeparateJvm(separateJvmBox.isSelected());
            manager.save();
            close();
        });

        HBox buttons = new HBox(8, cancelBtn, saveBtn);
        buttons.setAlignment(Pos.CENTER_RIGHT);

        root.getChildren().addAll(title, usernameLabel, usernameField, memoryLabel, memoryField, javaLabel, javaField, separateJvmBox, buttons);

        Scene scene = new Scene(root, 380, 380);
        setScene(scene);
        setTitle("Instance Settings");
        setResizable(false);
    }

    private void styleField(TextField field, Theme theme) {
        field.setStyle("-fx-background-color: " + toHex(theme.DECORATION) + "; -fx-text-fill: " + toHex(theme.FOREGROUND) + "; -fx-background-radius: 6; -fx-padding: 8;");
        field.setMaxWidth(Double.MAX_VALUE);
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
