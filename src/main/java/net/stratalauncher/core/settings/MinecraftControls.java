package net.stratalauncher.core.settings;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeSet;

public final class MinecraftControls {
    private static final Map<String, String> DEFAULT_BINDINGS = Map.ofEntries(
            Map.entry("key_key.attack", "key.mouse.left"),
            Map.entry("key_key.use", "key.mouse.right"),
            Map.entry("key_key.forward", "key.keyboard.w"),
            Map.entry("key_key.left", "key.keyboard.a"),
            Map.entry("key_key.back", "key.keyboard.s"),
            Map.entry("key_key.right", "key.keyboard.d"),
            Map.entry("key_key.jump", "key.keyboard.space"),
            Map.entry("key_key.sneak", "key.keyboard.left.shift"),
            Map.entry("key_key.sprint", "key.keyboard.left.control"),
            Map.entry("key_key.inventory", "key.keyboard.e"),
            Map.entry("key_key.drop", "key.keyboard.q"),
            Map.entry("key_key.swapOffhand", "key.keyboard.f"),
            Map.entry("key_key.pickItem", "key.mouse.middle"),
            Map.entry("key_key.chat", "key.keyboard.t"),
            Map.entry("key_key.playerlist", "key.keyboard.tab"),
            Map.entry("key_key.command", "key.keyboard.slash"),
            Map.entry("key_key.screenshot", "key.keyboard.f2"),
            Map.entry("key_key.togglePerspective", "key.keyboard.f5"),
            Map.entry("key_key.smoothCamera", "key.keyboard.unknown"),
            Map.entry("key_key.fullscreen", "key.keyboard.f11"),
            Map.entry("key_key.hotbar.1", "key.keyboard.1"),
            Map.entry("key_key.hotbar.2", "key.keyboard.2"),
            Map.entry("key_key.hotbar.3", "key.keyboard.3"),
            Map.entry("key_key.hotbar.4", "key.keyboard.4"),
            Map.entry("key_key.hotbar.5", "key.keyboard.5"),
            Map.entry("key_key.hotbar.6", "key.keyboard.6"),
            Map.entry("key_key.hotbar.7", "key.keyboard.7"),
            Map.entry("key_key.hotbar.8", "key.keyboard.8"),
            Map.entry("key_key.hotbar.9", "key.keyboard.9"),
            Map.entry("key_key.saveToolbarActivator", "key.keyboard.c"),
            Map.entry("key_key.loadToolbarActivator", "key.keyboard.x"),
            Map.entry("key_key.advancements", "key.keyboard.l"),
            Map.entry("key_key.friends", "key.keyboard.o"),
            Map.entry("key_key.socialInteractions", "key.keyboard.p"),
            Map.entry("key_key.toggleGui", "key.keyboard.f1"),
            Map.entry("key_key.toggleSpectatorShaderEffects", "key.keyboard.f4"),
            Map.entry("key_key.spectatorHotbar", "key.mouse.middle"),
            Map.entry("key_key.quickActions", "key.keyboard.g"),
            Map.entry("key_key.debug.overlay", "key.keyboard.f3"),
            Map.entry("key_key.debug.modifier", "key.keyboard.f3"),
            Map.entry("key_key.debug.reloadChunk", "key.keyboard.a"),
            Map.entry("key_key.debug.showHitboxes", "key.keyboard.b"),
            Map.entry("key_key.debug.clearChat", "key.keyboard.d"),
            Map.entry("key_key.debug.crash", "key.keyboard.c"),
            Map.entry("key_key.debug.showChunkBorders", "key.keyboard.g"),
            Map.entry("key_key.debug.showAdvancedTooltips", "key.keyboard.h"),
            Map.entry("key_key.debug.copyRecreateCommand", "key.keyboard.i"),
            Map.entry("key_key.debug.spectate", "key.keyboard.n"),
            Map.entry("key_key.debug.switchGameMode", "key.keyboard.f4"),
            Map.entry("key_key.debug.debugOptions", "key.keyboard.f6"),
            Map.entry("key_key.debug.focusPause", "key.keyboard.p"),
            Map.entry("key_key.debug.dumpDynamicTextures", "key.keyboard.s"),
            Map.entry("key_key.debug.reloadResourcePacks", "key.keyboard.t"),
            Map.entry("key_key.debug.profiling", "key.keyboard.l"),
            Map.entry("key_key.debug.copyLocation", "key.keyboard.c"),
            Map.entry("key_key.debug.dumpVersion", "key.keyboard.v"),
            Map.entry("key_key.debug.profilingChart", "key.keyboard.1"),
            Map.entry("key_key.debug.fpsCharts", "key.keyboard.2"),
            Map.entry("key_key.debug.networkCharts", "key.keyboard.3"),
            Map.entry("key_key.debug.lightmapTexture", "key.keyboard.4"),
            Map.entry("key_key.debug.improvedTransparency", "key.keyboard.x"));

    private MinecraftControls() {
    }

    public static Map<String, String> defaultBindings() {
        return DEFAULT_BINDINGS;
    }

    public static Map<String, String> bindings(Map<String, String> options) {
        TreeSet<String> keys = new TreeSet<>(DEFAULT_BINDINGS.keySet());
        options.keySet().stream().filter(MinecraftControls::isBindingKey).forEach(keys::add);
        Map<String, String> bindings = new LinkedHashMap<>();
        for (String key : keys) {
            bindings.put(key, options.getOrDefault(key, DEFAULT_BINDINGS.getOrDefault(key, "key.keyboard.unknown")));
        }
        return Map.copyOf(bindings);
    }

    public static Map<String, List<String>> conflicts(Map<String, String> bindings) {
        Map<String, List<String>> assigned = new LinkedHashMap<>();
        for (Map.Entry<String, String> binding : bindings.entrySet()) {
            if (binding.getValue() == null || binding.getValue().equals("key.keyboard.unknown")
                    || binding.getKey().startsWith("key_key.debug.")
                    || binding.getKey().equals("key_key.spectatorHotbar")) {
                continue;
            }
            assigned.computeIfAbsent(binding.getValue(), ignored -> new ArrayList<>()).add(binding.getKey());
        }
        Map<String, List<String>> conflicts = new LinkedHashMap<>();
        assigned.forEach((value, keys) -> {
            if (keys.size() > 1) {
                conflicts.put(value, keys.stream().sorted().toList());
            }
        });
        return Map.copyOf(conflicts);
    }

    public static String actionName(String optionKey) {
        if (!isBindingKey(optionKey)) {
            return optionKey;
        }
        String name = optionKey.substring("key_key.".length());
        return switch (name) {
            case "attack" -> "Attack";
            case "use" -> "Use Item / Place Block";
            case "forward" -> "Move Forward";
            case "left" -> "Strafe Left";
            case "back" -> "Move Backward";
            case "right" -> "Strafe Right";
            case "jump" -> "Jump";
            case "sneak" -> "Sneak";
            case "sprint" -> "Sprint";
            case "inventory" -> "Open Inventory";
            case "drop" -> "Drop Item";
            case "swapOffhand" -> "Swap Offhand";
            case "pickItem" -> "Pick Block";
            case "chat" -> "Open Chat";
            case "playerlist" -> "Player List";
            case "command" -> "Open Command";
            case "friends" -> "Friends";
            case "socialInteractions" -> "Social Interactions";
            case "toggleGui" -> "Toggle HUD";
            case "toggleSpectatorShaderEffects" -> "Toggle Spectator Effects";
            case "screenshot" -> "Take Screenshot";
            case "togglePerspective" -> "Toggle Perspective";
            case "smoothCamera" -> "Toggle Cinematic Camera";
            case "fullscreen" -> "Toggle Fullscreen";
            case "spectatorOutlines" -> "Spectator Outlines";
            case "spectatorHotbar" -> "Spectator Hotbar";
            case "quickActions" -> "Quick Actions";
            case "debug.overlay" -> "Debug Overlay";
            case "debug.modifier" -> "Debug Modifier";
            case "saveToolbarActivator" -> "Save Toolbar Activator";
            case "loadToolbarActivator" -> "Load Toolbar Activator";
            case "advancements" -> "Advancements";
            default -> name.startsWith("hotbar.")
                    ? "Select Hotbar Slot " + name.substring("hotbar.".length())
                    : title(name.replace('.', ' '));
        };
    }

    public static boolean isBindingKey(String key) {
        return key != null && key.startsWith("key_key.");
    }

    public static String displayKey(String value) {
        if (value == null || value.equals("key.keyboard.unknown")) {
            return "Unassigned";
        }
        if (value.startsWith("key.mouse.")) {
            return "Mouse " + title(value.substring("key.mouse.".length()));
        }
        if (value.startsWith("key.keyboard.")) {
            return title(value.substring("key.keyboard.".length()));
        }
        return value;
    }

    private static String title(String value) {
        StringBuilder title = new StringBuilder();
        boolean capitalize = true;
        char previous = ' ';
        for (char character : value.replace('.', ' ').replace('_', ' ').toCharArray()) {
            if (Character.isWhitespace(character)) {
                title.append(' ');
                capitalize = true;
            } else {
                if (Character.isUpperCase(character) && Character.isLowerCase(previous)) {
                    title.append(' ');
                    capitalize = true;
                }
                title.append(capitalize ? Character.toUpperCase(character) : character);
                capitalize = false;
            }
            previous = character;
        }
        return title.toString();
    }
}
