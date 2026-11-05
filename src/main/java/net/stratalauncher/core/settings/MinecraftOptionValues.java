package net.stratalauncher.core.settings;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class MinecraftOptionValues {
    private static final Pattern VERSION_MAJOR = Pattern.compile("^(\\d+)\\.");

    private MinecraftOptionValues() {
    }

    public static String toDisplayValue(String minecraftVersion, String key, String storedValue) {
        if (!usesNormalizedFov(minecraftVersion) || !"fov".equals(key)) {
            return storedValue;
        }
        try {
            double normalized = Double.parseDouble(storedValue);
            return Integer.toString((int) Math.round(70 + normalized * 40));
        } catch (NumberFormatException exception) {
            return storedValue;
        }
    }

    public static String toStoredValue(String minecraftVersion, String key, String displayValue) {
        if (!usesNormalizedFov(minecraftVersion) || !"fov".equals(key)) {
            return displayValue;
        }
        final int degrees;
        try {
            degrees = Integer.parseInt(displayValue.trim());
        } catch (NumberFormatException exception) {
            throw new IllegalArgumentException("Minecraft 26.3 FOV must be an integer from 30 to 110.", exception);
        }
        if (degrees < 30 || degrees > 110) {
            throw new IllegalArgumentException("Minecraft 26.3 FOV must be between 30 and 110 degrees.");
        }
        return Double.toString((degrees - 70) / 40.0);
    }

    public static boolean usesNormalizedFov(String minecraftVersion) {
        if (minecraftVersion == null) {
            return false;
        }
        Matcher matcher = VERSION_MAJOR.matcher(minecraftVersion);
        return matcher.find() && Integer.parseInt(matcher.group(1)) >= 26;
    }
}
