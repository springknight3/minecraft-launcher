package net.stratalauncher.ui.instances;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;

class InstanceListViewTest {
    @Test
    void onlyRecognizesExplicitMinecraftClientReadyMarkers() {
        assertTrue(InstanceListView.isGameReadyMarker("[Render thread/INFO]: Sound engine started"));
        assertTrue(InstanceListView.isGameReadyMarker("[Render thread/INFO]: Reload complete"));
        assertFalse(InstanceListView.isGameReadyMarker("Minecraft started (process 1234)"));
        assertFalse(InstanceListView.isGameReadyMarker("Preparing to launch Minecraft"));
    }
}
