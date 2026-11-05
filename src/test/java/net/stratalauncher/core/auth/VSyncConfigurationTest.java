package net.stratalauncher.core.auth;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.assertEquals;

class VSyncConfigurationTest {
    @Test
    void resolvesOsDefaultConfigAndExplicitOverride() {
        assertEquals(new VSyncConfiguration.Resolution(false, VSyncConfiguration.Source.DEFAULT),
                VSyncConfiguration.resolve(true, null, null));
        assertEquals(new VSyncConfiguration.Resolution(true, VSyncConfiguration.Source.DEFAULT),
                VSyncConfiguration.resolve(false, null, null));
        assertEquals(new VSyncConfiguration.Resolution(true, VSyncConfiguration.Source.CONFIG),
                VSyncConfiguration.resolve(true, true, null));
        assertEquals(new VSyncConfiguration.Resolution(false, VSyncConfiguration.Source.CONFIG),
                VSyncConfiguration.resolve(false, false, null));
        assertEquals(new VSyncConfiguration.Resolution(false,
                        VSyncConfiguration.Source.COMMAND_LINE_OVERRIDE),
                VSyncConfiguration.resolve(false, true, "false"));
    }
}
