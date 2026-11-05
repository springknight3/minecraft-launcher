package net.stratalauncher.ui.install;

import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.control.Label;
import javafx.scene.control.ProgressBar;
import javafx.scene.layout.VBox;
import net.stratalauncher.core.download.DownloadManager;

public class InstallingView extends VBox {
    private final ProgressBar progressBar = new ProgressBar(-1);
    private final Label currentItem = new Label("Preparing downloads...");

    public InstallingView(String instanceName) {
        setAlignment(Pos.CENTER);
        setSpacing(18);
        setPadding(new Insets(36));
        getStyleClass().add("installing-view");

        Label heading = new Label("Installing " + instanceName + "...");
        heading.getStyleClass().add("installing-heading");
        progressBar.setMaxWidth(460);
        progressBar.setPrefWidth(460);
        progressBar.getStyleClass().add("installing-progress");
        currentItem.getStyleClass().add("installing-item");
        currentItem.setWrapText(true);
        currentItem.setMaxWidth(560);
        currentItem.setAlignment(Pos.CENTER);

        getChildren().addAll(heading, progressBar, currentItem);
    }

    public void update(DownloadManager.Progress progress) {
        if (progress.total() == 0) {
            progressBar.setProgress(-1);
            currentItem.setText(progress.currentItem());
            return;
        }
        progressBar.setProgress(progress.fraction());
        currentItem.setText(progress.completed() >= progress.total()
                ? "Downloaded: " + progress.currentItem()
                : "Downloading: " + progress.currentItem());
    }
}
