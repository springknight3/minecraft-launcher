package net.stratalauncher.ui;

import javafx.animation.FadeTransition;
import javafx.animation.ScaleTransition;
import javafx.animation.TranslateTransition;
import javafx.animation.ParallelTransition;
import javafx.animation.PauseTransition;
import javafx.geometry.Insets;
import javafx.geometry.Pos;
import javafx.scene.Node;
import javafx.scene.CacheHint;
import javafx.scene.control.Button;
import javafx.scene.control.Label;
import javafx.scene.layout.HBox;
import javafx.scene.layout.StackPane;
import javafx.scene.layout.VBox;
import javafx.util.Duration;

import java.util.Objects;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

public final class FlyoutManager {
    private final StackPane overlay = new StackPane();
    private final VBox toastStack = new VBox(8);
    private final StackPane flyoutLayer = new StackPane();
    private final StackPane backdrop = new StackPane();
    private final VBox flyoutPanel = new VBox(16);
    private final Label flyoutTitle = new Label();
    private final Label flyoutSubtitle = new Label();
    private final VBox flyoutHeader = new VBox(8, flyoutTitle, flyoutSubtitle);
    private final HBox actionButtons = new HBox(10);
    private Node activeContent;
    private ParallelTransition entrance;
    private boolean flyoutOpen;
    private boolean darkMode;

    public FlyoutManager() {
        overlay.getStyleClass().add("overlay-layer");
        overlay.setAlignment(Pos.CENTER);
        overlay.setVisible(false);
        overlay.setPickOnBounds(true);
        overlay.setMouseTransparent(true);
        toastStack.setAlignment(Pos.BOTTOM_CENTER);
        toastStack.setPadding(new Insets(0, 20, 24, 20));
        toastStack.setMouseTransparent(true);
        StackPane.setAlignment(toastStack, Pos.BOTTOM_CENTER);
        backdrop.getStyleClass().add("flyout-backdrop");
        flyoutTitle.getStyleClass().add("flyout-title");
        flyoutSubtitle.getStyleClass().add("flyout-subtitle");
        flyoutSubtitle.setWrapText(true);
        actionButtons.setAlignment(Pos.CENTER_RIGHT);
        flyoutPanel.getStyleClass().add("flyout-panel");
        flyoutPanel.setMaxWidth(1100);
        flyoutPanel.setMaxHeight(760);
        flyoutPanel.setPadding(new Insets(24));
        flyoutPanel.getChildren().addAll(flyoutHeader, actionButtons);
        flyoutLayer.getChildren().addAll(backdrop, flyoutPanel);
        flyoutLayer.setAlignment(Pos.CENTER);
        flyoutLayer.setVisible(false);
        flyoutLayer.setManaged(false);
        StackPane.setMargin(flyoutPanel, new Insets(24));
        overlay.getChildren().add(toastStack);
    }

    public StackPane overlay() {
        return overlay;
    }

    public void setDarkMode(boolean darkMode) {
        this.darkMode = darkMode;
    }

    public void show(String title, String subtitle, Node content, Action... actions) {
        FrameTimeOverlay.measure("Flyout open", MotionConstants.NORMAL);
        close();
        flyoutTitle.setText(title);
        flyoutSubtitle.setText(subtitle == null ? "" : subtitle);
        flyoutHeader.setVisible(subtitle != null && !subtitle.isBlank());
        flyoutHeader.setManaged(flyoutHeader.isVisible());
        actionButtons.getChildren().clear();
        for (Action action : actions) {
            Button button = new Button(action.label());
            String icon = iconForAction(action.label());
            if (icon != null) {
                IconAssets.apply(button, icon, darkMode);
            }
            if (action.styleClass() != null && !action.styleClass().isBlank()) {
                button.getStyleClass().add(action.styleClass());
            }
            button.setOnAction(event -> {
                boolean cancelAction = "Cancel".equalsIgnoreCase(action.label());
                try {
                    if (cancelAction) {
                        action.handler().getAsBoolean();
                        close();
                    } else if (action.handler().getAsBoolean()) {
                        close();
                    }
                } finally {
                    if (cancelAction && flyoutOpen) {
                        close();
                    }
                }
            });
            actionButtons.getChildren().add(button);
        }

        if (activeContent != content) {
            if (activeContent != null) {
                flyoutPanel.getChildren().remove(activeContent);
            }
            activeContent = content;
            flyoutPanel.getChildren().add(1, content);
        }
        flyoutOpen = true;
        flyoutLayer.setVisible(true);
        flyoutLayer.setManaged(true);
        overlay.getChildren().add(0, flyoutLayer);
        overlay.setVisible(true);
        overlay.setMouseTransparent(false);
        if (!MotionConstants.reduceMotion) {
            flyoutPanel.setOpacity(0);
            flyoutPanel.setTranslateY(18);
            entrance = new ParallelTransition(
                    new FadeTransition(MotionConstants.NORMAL, flyoutPanel),
                    new TranslateTransition(MotionConstants.NORMAL, flyoutPanel));
            ((FadeTransition) entrance.getChildren().get(0)).setFromValue(0);
            ((FadeTransition) entrance.getChildren().get(0)).setToValue(1);
            ((FadeTransition) entrance.getChildren().get(0)).setInterpolator(MotionConstants.STANDARD);
            ((TranslateTransition) entrance.getChildren().get(1)).setFromY(18);
            ((TranslateTransition) entrance.getChildren().get(1)).setToY(0);
            ((TranslateTransition) entrance.getChildren().get(1)).setInterpolator(MotionConstants.EMPHASIS);
            boolean previousCache = flyoutPanel.isCache();
            CacheHint previousCacheHint = flyoutPanel.getCacheHint();
            flyoutPanel.setCache(true);
            flyoutPanel.setCacheHint(CacheHint.SPEED);
            entrance.setOnFinished(event -> {
                flyoutPanel.setCache(previousCache);
                flyoutPanel.setCacheHint(previousCacheHint);
                entrance = null;
            });
            entrance.play();
        } else {
            flyoutPanel.setOpacity(1);
            flyoutPanel.setTranslateY(0);
        }
    }

    private static String iconForAction(String label) {
        String normalized = label.toLowerCase(java.util.Locale.ROOT);
        if (normalized.contains("delete") || normalized.contains("remove")) {
            return "delete";
        }
        if (normalized.contains("install") || normalized.contains("download") || normalized.contains("repair")) {
            return "download";
        }
        if (normalized.contains("save")) {
            return "save";
        }
        if (normalized.contains("accept")) {
            return "secure";
        }
        if (normalized.contains("terminate")) {
            return "terminate";
        }
        if (normalized.contains("start") || normalized.contains("launch")) {
            return "start";
        }
        if (normalized.contains("stop")) {
            return "stop";
        }
        if (normalized.contains("add") || normalized.contains("create") || normalized.contains("duplicate")) {
            return "add";
        }
        return null;
    }

    public void close() {
        if (entrance != null) {
            entrance.stop();
            entrance = null;
        }
        flyoutPanel.setCache(false);
        flyoutPanel.setCacheHint(CacheHint.DEFAULT);
        flyoutPanel.setOpacity(1);
        flyoutPanel.setTranslateY(0);
        if (flyoutOpen) {
            if (activeContent != null) {
                flyoutPanel.getChildren().remove(activeContent);
                activeContent = null;
            }
            overlay.getChildren().remove(flyoutLayer);
            flyoutOpen = false;
        }
        overlay.setVisible(!toastStack.getChildren().isEmpty());
        overlay.setMouseTransparent(true);
    }

    void clearBenchmarkToasts() {
        toastStack.getChildren().clear();
        if (!flyoutOpen) {
            overlay.setVisible(false);
            overlay.setMouseTransparent(true);
        }
    }

    public void showConfirmation(String title, String message, Consumer<Boolean> result) {
        Label body = body(message);
        show(title, "", body,
                new Action("Cancel", "action-button", () -> {
                    result.accept(false);
                    return true;
                }),
                new Action("Confirm", "primary-button", () -> {
                    result.accept(true);
                    return true;
                }));
    }

    public void showTypedDeleteConfirmation(String title, String message, Consumer<Boolean> result) {
        javafx.scene.control.TextField confirmation = new javafx.scene.control.TextField();
        confirmation.setPromptText("Type DELETE to confirm");
        Label validation = new Label();
        validation.getStyleClass().add("setting-description");
        VBox content = new VBox(10, body(message), new Label("Type DELETE to permanently remove this item."),
                confirmation, validation);
        show(title, "", content,
                new Action("Cancel", "action-button", () -> {
                    result.accept(false);
                    return true;
                }),
                new Action("Delete", "danger-button", () -> {
                    if (!"DELETE".equals(confirmation.getText())) {
                        validation.setText("Enter DELETE exactly to continue.");
                        return false;
                    }
                    result.accept(true);
                    return true;
                }));
        confirmation.requestFocus();
    }

    public void showPrompt(String title, String prompt, String initialValue, Consumer<String> result) {
        javafx.scene.control.TextField input = new javafx.scene.control.TextField(initialValue);
        input.setPromptText(prompt);
        input.setMaxWidth(Double.MAX_VALUE);
        VBox body = new VBox(8, body(prompt), input);
        show(title, "", body,
                new Action("Cancel", "action-button", () -> true),
                new Action("Continue", "primary-button", () -> {
                    if (input.getText() == null || input.getText().isBlank()) {
                        showToast("Enter a value before continuing.", ToastKind.WARNING);
                        return false;
                    }
                    result.accept(input.getText());
                    return true;
                }));
        input.requestFocus();
    }

    public void showMessage(String title, String message) {
        show(title, "", body(message), new Action("OK", "primary-button", () -> true));
    }

    public FlyoutHandle showDeviceCode(String title, String message) {
        show(title, "", body(message), new Action("Close", "action-button", () -> true));
        return this::close;
    }

    public void showError(String title, String message) {
        showToast(title + ": " + message, ToastKind.ERROR);
    }

    public void showToast(String message) {
        showToast(message, ToastKind.INFO);
    }

    public void showToast(String message, ToastKind kind) {
        Objects.requireNonNull(message);
        Label toast = new Label(message);
        toast.getStyleClass().addAll("notification-toast", "toast-" + kind.name().toLowerCase());
        toast.setWrapText(true);
        toast.setMaxWidth(560);
        toast.setPadding(new Insets(12, 18, 12, 18));
        toastStack.getChildren().add(toast);
        overlay.setVisible(true);
        overlay.setMouseTransparent(!flyoutOpen);
        if (MotionConstants.reduceMotion) {
            PauseTransition timeout = new PauseTransition(Duration.seconds(4));
            timeout.setOnFinished(event -> removeToast(toast));
            timeout.play();
            return;
        }
        toast.setOpacity(0);
        toast.setTranslateY(12);
        FadeTransition fade = new FadeTransition(MotionConstants.NORMAL, toast);
        fade.setFromValue(0);
        fade.setToValue(1);
        fade.setInterpolator(MotionConstants.STANDARD);
        TranslateTransition translate = new TranslateTransition(MotionConstants.NORMAL, toast);
        translate.setFromY(12);
        translate.setToY(0);
        translate.setInterpolator(MotionConstants.EMPHASIS);
        new ParallelTransition(fade, translate).play();
        if (kind != ToastKind.INFO) {
            ScaleTransition pulse = new ScaleTransition(MotionConstants.NORMAL, toast);
            pulse.setFromX(0.98);
            pulse.setFromY(0.98);
            pulse.setToX(1);
            pulse.setToY(1);
            pulse.setCycleCount(2);
            pulse.setAutoReverse(true);
            pulse.setInterpolator(MotionConstants.STANDARD);
            pulse.play();
        }
        PauseTransition timeout = new PauseTransition(Duration.seconds(4));
        timeout.setOnFinished(event -> removeToast(toast));
        timeout.play();
    }

    private void removeToast(Node toast) {
        FadeTransition fade = new FadeTransition(MotionConstants.FAST, toast);
        fade.setToValue(0);
        fade.setInterpolator(MotionConstants.EXPRESS);
        fade.setOnFinished(event -> {
            toastStack.getChildren().remove(toast);
            if (!flyoutOpen && toastStack.getChildren().isEmpty()) {
                overlay.setVisible(false);
            }
        });
        fade.play();
    }

    private static Label body(String text) {
        Label label = new Label(text);
        label.setWrapText(true);
        label.setMaxWidth(Double.MAX_VALUE);
        return label;
    }

    public record Action(String label, String styleClass, BooleanSupplier handler) {
        public Action {
            Objects.requireNonNull(label);
            Objects.requireNonNull(handler);
        }
    }

    @FunctionalInterface
    public interface FlyoutHandle {
        void close();
    }

    public enum ToastKind {
        INFO,
        WARNING,
        ERROR
    }
}
