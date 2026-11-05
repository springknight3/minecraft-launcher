package net.stratalauncher.ui.animations;

import javafx.animation.ScaleTransition;
import javafx.scene.Node;
import javafx.scene.Parent;
import javafx.scene.control.Button;
import javafx.scene.input.MouseEvent;
import net.stratalauncher.ui.MotionConstants;

public final class ButtonMotion {
    private ButtonMotion() {
    }

    public static void attach(Parent root) {
        root.addEventFilter(MouseEvent.MOUSE_ENTERED_TARGET, event ->
                animateButton(event.getTarget(), 1.025));
        root.addEventFilter(MouseEvent.MOUSE_EXITED_TARGET, event ->
                animateButton(event.getTarget(), 1));
        root.addEventFilter(MouseEvent.MOUSE_PRESSED, event ->
                animateButton(event.getTarget(), 0.97));
        root.addEventFilter(MouseEvent.MOUSE_RELEASED, event ->
                animateButton(event.getTarget(), 1.025));
    }

    private static void animateButton(Object target, double scale) {
        if (MotionConstants.reduceMotion) {
            return;
        }
        if (!(target instanceof Node node)) {
            return;
        }
        Node current = node;
        while (current != null && !(current instanceof Button)) {
            current = current.getParent();
        }
        if (current instanceof Button button) {
            ScaleTransition transition = new ScaleTransition(MotionConstants.FAST, button);
            transition.setToX(scale);
            transition.setToY(scale);
            transition.setInterpolator(MotionConstants.STANDARD);
            transition.play();
        }
    }
}
