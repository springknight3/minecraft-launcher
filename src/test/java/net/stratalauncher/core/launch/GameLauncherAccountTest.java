package net.stratalauncher.core.launch;

import org.junit.jupiter.api.Test;

import java.nio.file.Path;

import static org.junit.jupiter.api.Assertions.assertEquals;

class GameLauncherAccountTest {
    @Test
    void usesMicrosoftIdentityAndTokenInLauncherSubstitutions() {
        var values = GameLauncher.createSubstitutions(
                "Player", "0123456789abcdef0123456789abcdef", "minecraft-access-token",
                "msa", "xuid-123", "1.21.1", Path.of("game"),
                Path.of("assets"), "asset-index", Path.of("natives"), null,
                Path.of("libraries"), java.util.List.of(Path.of("client.jar")));

        assertEquals("0123456789abcdef0123456789abcdef", values.get("auth_uuid"));
        assertEquals("minecraft-access-token", values.get("auth_access_token"));
        assertEquals("msa", values.get("user_type"));
        assertEquals("xuid-123", values.get("auth_xuid"));
    }
}
