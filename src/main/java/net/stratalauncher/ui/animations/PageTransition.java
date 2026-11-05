package net.stratalauncher.ui.animations;

import javafx.animation.FadeTransition;
import javafx.animation.ParallelTransition;
import javafx.animation.TranslateTransition;
import javafx.scene.control.Control;
import javafx.scene.Node;
import javafx.scene.Parent;
import javafx.scene.control.Tab;
import javafx.scene.control.TabPane;
import javafx.scene.CacheHint;
import javafx.scene.layout.BorderPane;
import javafx.util.Duration;
import net.stratalauncher.ui.MotionConstants;
import net.stratalauncher.ui.FrameTimeOverlay;

public final class PageTransition {
    private PageTransition() {
    }

    public static void show(BorderPane host, Node page) {
        host.setCenter(page);
        animate(page, true);
    }

    public static void attachTabTransitions(Node root) {
        if (!(root instanceof Parent parent)) {
            return;
        }
        if (root instanceof TabPane tabPane) {
            tabPane.getSelectionModel().selectedItemProperty().addListener((observable, oldTab, selected) -> {
                if (selected != null && selected.getContent() != null) {
                    animate(selected.getContent(), false);
                }
            });
            for (Tab tab : tabPane.getTabs()) {
                attachTabTransitions(tab.getContent());
            }
        }
        for (Node child : parent.getChildrenUnmodifiable()) {
            if (child instanceof TabPane || !(child instanceof Control)) {
                attachTabTransitions(child);
            }
        }
    }

    private static void animate(Node page, boolean horizontal) {
        if (MotionConstants.reduceMotion) {
            page.setOpacity(1);
            page.setTranslateX(0);
            page.setTranslateY(0);
            return;
        }
        FrameTimeOverlay.measure(horizontal ? "Screen switch" : "Tab switch",
                Duration.millis(MotionConstants.NORMAL.toMillis() + 336));
        page.setOpacity(0);
        page.setTranslateX(horizontal ? 22 : 10);
        FadeTransition fade = new FadeTransition(MotionConstants.NORMAL, page);
        fade.setFromValue(0);
        fade.setToValue(1);
        fade.setInterpolator(MotionConstants.STANDARD);
        TranslateTransition translate = new TranslateTransition(MotionConstants.NORMAL, page);
        translate.setFromX(horizontal ? 22 : 10);
        translate.setToX(0);
        translate.setInterpolator(MotionConstants.EMPHASIS);
        ParallelTransition transition = new ParallelTransition(fade, translate);
        boolean previousCache = page.isCache();
        CacheHint previousCacheHint = page.getCacheHint();
        page.setCache(true);
        page.setCacheHint(CacheHint.SPEED);
        transition.setOnFinished(event -> {
            page.setCache(previousCache);
            page.setCacheHint(previousCacheHint);
        });
        if (page instanceof Parent parent) {
            int index = 0;
            for (Node child : animatedContent(parent).stream().limit(6).toList()) {
                Duration delay = Duration.millis(Math.min(index++, 12) * 28L);
                FadeTransition childFade = new FadeTransition(MotionConstants.NORMAL, child);
                childFade.setFromValue(0);
                childFade.setToValue(1);
                childFade.setDelay(delay);
                childFade.setInterpolator(MotionConstants.STANDARD);
                transition.getChildren().add(childFade);
            }
        }
        transition.playFromStart();
    }

    private static java.util.List<Node> animatedContent(Parent parent) {
        java.util.List<Node> result = new java.util.ArrayList<>();
        for (Node child : parent.getChildrenUnmodifiable()) {
            if (!child.isManaged() || !child.isVisible()) {
                continue;
            }
            if (child instanceof Control || !(child instanceof Parent nested)) {
                result.add(child);
            } else {
                result.addAll(animatedContent(nested));
            }
        }
        return result;
    }
}
