package com.strata.launcher;

import javafx.application.Platform;
import javafx.concurrent.Task;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.Scene;
import javafx.scene.layout.StackPane;
import javafx.scene.layout.VBox;
import javafx.scene.paint.Color;
import javafx.scene.shape.Rectangle;
import javafx.scene.text.Font;
import javafx.scene.text.FontWeight;
import javafx.scene.text.Text;
import javafx.stage.Stage;

public class DownloadWindow extends Stage {

    public DownloadWindow(Instance instance, InstanceManager manager, Runnable onDone) {
        Theme theme = Theme.get();

        VBox root = new VBox(16);
        root.setAlignment(Pos.CENTER);
        root.setPadding(new Insets(40));
        root.setStyle("-fx-background-color: " + toHex(theme.BACKGROUND) + ";");

        Text header = new Text("DOWNLOADING");
        header.setFill(theme.FOREGROUND);
        header.setFont(Font.font("System", FontWeight.BOLD, 24));

        Text statusText = new Text("Preparing...");
        statusText.setFill(theme.FOREGROUND);
        statusText.setOpacity(0.7);
        statusText.setFont(Font.font(13));

        StackPane barBg = new StackPane();
        barBg.setMaxWidth(360);
        barBg.setPrefHeight(8);
        barBg.setStyle("-fx-background-color: " + toHex(theme.DECORATION) + "; -fx-background-radius: 4;");

        Rectangle barFill = new Rectangle(0, 8);
        barFill.setFill(theme.ACTION);
        barFill.setArcWidth(4);
        barFill.setArcHeight(4);

        barBg.getChildren().add(barFill);
        barBg.widthProperty().addListener((obs, o, n) -> barFill.setWidth(0));

        Text percentText = new Text("0%");
        percentText.setFill(theme.FOREGROUND);
        percentText.setFont(Font.font(12));

        root.getChildren().addAll(header, statusText, barBg, percentText);

        Rectangle clip = new Rectangle();
        clip.widthProperty().bind(root.widthProperty());
        clip.heightProperty().bind(root.heightProperty());
        StackPane clipPane = new StackPane(root);
        clipPane.setClip(clip);

        Scene scene = new Scene(clipPane, 440, 260);
        setScene(scene);
        setTitle("Downloading " + instance.getName());
        setResizable(false);

        Task<Void> downloadTask = new Task<>() {
            @Override
            protected Void call() throws Exception {
                GameExecutor executor = new GameExecutor();
                executor.downloadInstance(instance, (status, percent) -> Platform.runLater(() -> {
                    statusText.setText(status);
                    percentText.setText(percent + "%");
                    double width = clipPane.getWidth() - 80;
                    barFill.setWidth(width * percent / 100.0);
                }));
                return null;
            }

            @Override
            protected void succeeded() {
                Platform.runLater(() -> {
                    instance.setDownloaded(true);
                    manager.save();
                    close();
                    if (onDone != null) onDone.run();
                });
            }

            @Override
            protected void failed() {
                Platform.runLater(() -> {
                    statusText.setText("Download failed: " + getException().getMessage());
                    percentText.setText("");
                });
            }
        };

        Thread dlThread = new Thread(downloadTask);
        dlThread.setDaemon(true);
        dlThread.start();
    }

    private String toHex(Color color) {
        return String.format("#%02X%02X%02X",
                (int) (color.getRed() * 255),
                (int) (color.getGreen() * 255),
                (int) (color.getBlue() * 255));
    }
}
