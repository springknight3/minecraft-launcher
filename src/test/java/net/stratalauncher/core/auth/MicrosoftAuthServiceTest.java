package net.stratalauncher.core.auth;

import com.google.gson.JsonParser;
import org.junit.jupiter.api.Test;

import java.io.IOException;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

class MicrosoftAuthServiceTest {
    @Test
    void parsesDeviceCodeResponseForUserPromptAndPolling() throws IOException {
        var response = JsonParser.parseString("""
                {
                  "device_code": "secret-device-code",
                  "user_code": "ABCD-EFGH",
                  "verification_uri": "https://microsoft.com/devicelogin",
                  "message": "Sign in",
                  "expires_in": 900,
                  "interval": 5
                }
                """).getAsJsonObject();

        MicrosoftAuthService.DeviceCode deviceCode = MicrosoftAuthService.parseDeviceCode(response);

        assertEquals("ABCD-EFGH", deviceCode.userCode());
        assertEquals("https://microsoft.com/devicelogin", deviceCode.verificationUri());
        assertEquals(5, deviceCode.interval());
    }

    @Test
    void rejectsMalformedMinecraftProfileIdentifiers() {
        var response = JsonParser.parseString("""
                {"id":"not-a-uuid","name":"Player"}
                """).getAsJsonObject();

        assertThrows(IOException.class,
                () -> MicrosoftAuthService.parseProfile(response, "access", "refresh"));
    }
}
