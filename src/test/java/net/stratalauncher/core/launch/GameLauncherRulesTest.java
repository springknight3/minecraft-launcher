package net.stratalauncher.core.launch;

import com.google.gson.JsonParser;
import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;

class GameLauncherRulesTest {
    @Test
    void excludesQuickPlayArgumentsWhenQuickPlayIsNotEnabled() {
        var argument = JsonParser.parseString("""
                {
                  "rules": [
                    {
                      "action": "allow",
                      "features": {
                        "has_quick_plays_support": true
                      }
                    }
                  ],
                  "value": ["--quickPlayPath", "${quickPlayPath}"]
                }
                """).getAsJsonObject();

        assertFalse(GameLauncher.allowedByRules(argument, "linux"));
    }

    @Test
    void allowsArgumentsWhoseFeatureRequirementsAreDisabled() {
        var argument = JsonParser.parseString("""
                {
                  "rules": [
                    {
                      "action": "allow",
                      "features": {
                        "is_demo_user": false
                      }
                    }
                  ],
                  "value": ["--username", "${auth_player_name}"]
                }
                """).getAsJsonObject();

        assertTrue(GameLauncher.allowedByRules(argument, "linux"));
    }
}
