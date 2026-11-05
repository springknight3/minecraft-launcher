package net.stratalauncher.ui;

import javafx.animation.Interpolator;
import javafx.util.Duration;

public final class MotionConstants {
    public static final Duration FAST = Duration.millis(220);
    public static final Duration NORMAL = Duration.millis(420);
    public static final Duration SLOW = Duration.millis(680);
    public static final Interpolator STANDARD = Interpolator.SPLINE(0.25, 0.1, 0.25, 1);
    public static final Interpolator EMPHASIS = Interpolator.SPLINE(0.22, 1, 0.36, 1);
    public static final Interpolator EXPRESS = Interpolator.SPLINE(0.2, 0.8, 0.2, 1);
    public static volatile boolean reduceMotion = Boolean.getBoolean("stratalauncher.reduceMotion");

    private MotionConstants() {
    }
}
