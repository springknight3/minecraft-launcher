package net.stratalauncher.core.settings;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

class MinecraftOptionValuesTest {
    @Test
    void convertsMinecraft263FovBetweenDegreesAndStoredScale() {
        assertEquals("70", MinecraftOptionValues.toDisplayValue("26.3", "fov", "0.0"));
        assertEquals("110", MinecraftOptionValues.toDisplayValue("26.3", "fov", "1.0"));
        assertEquals("1.0", MinecraftOptionValues.toStoredValue("26.3", "fov", "110"));
        assertEquals("-1.0", MinecraftOptionValues.toStoredValue("26.3", "fov", "30"));
    }

    @Test
    void leavesUnchangedOptionFormatsAloneAndValidatesFovBounds() {
        assertEquals("110", MinecraftOptionValues.toStoredValue("1.21.1", "fov", "110"));
        assertEquals("1.0", MinecraftOptionValues.toStoredValue("26.3", "gamma", "1.0"));
        assertThrows(IllegalArgumentException.class,
                () -> MinecraftOptionValues.toStoredValue("26.3", "fov", "111"));
    }
}
