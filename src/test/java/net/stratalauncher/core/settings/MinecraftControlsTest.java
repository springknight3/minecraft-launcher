package net.stratalauncher.core.settings;

import org.junit.jupiter.api.Test;

import java.util.Map;

import static org.junit.jupiter.api.Assertions.assertEquals;

class MinecraftControlsTest {
    @Test
    void showsNamedDefaultsAndRetainsStoredModdedBindings() {
        Map<String, String> bindings = MinecraftControls.bindings(Map.of(
                "key_key.forward", "key.keyboard.up",
                "key_key.mod.example", "key.keyboard.m"));

        assertEquals("Move Forward", MinecraftControls.actionName("key_key.forward"));
        assertEquals("Up", MinecraftControls.displayKey(bindings.get("key_key.forward")));
        assertEquals("Mod Example", MinecraftControls.actionName("key_key.mod.example"));
        assertEquals("key.keyboard.m", bindings.get("key_key.mod.example"));
    }

    @Test
    void identifiesDuplicateAssignmentsButIgnoresUnassignedBindings() {
        Map<String, String> bindings = Map.of(
                "key_key.forward", "key.keyboard.w",
                "key_key.mod.example", "key.keyboard.w",
                "key_key.jump", "key.keyboard.unknown");

        assertEquals(Map.of("key.keyboard.w", java.util.List.of("key_key.forward", "key_key.mod.example")),
                MinecraftControls.conflicts(bindings));
    }

    @Test
    void doesNotFlagMinecraftDebugChordBindingsAsConflicts() {
        assertEquals(Map.of(), MinecraftControls.conflicts(MinecraftControls.defaultBindings()));
    }
}
