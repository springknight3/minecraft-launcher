package net.stratalauncher.core.auth;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.Map;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

class AccountManagerTest {
    @TempDir
    Path temporaryDirectory;

    @Test
    void persistsOfflineAccountsAndActiveSelection() throws IOException, InterruptedException {
        CredentialStore credentials = new MemoryCredentialStore();
        AccountManager manager = new AccountManager(temporaryDirectory, credentials);
        Account first = manager.addOffline("First");
        Account second = manager.addOffline("Second");

        manager.select(second.id());
        AccountManager reopened = new AccountManager(temporaryDirectory, credentials);

        assertEquals(2, reopened.accounts().size());
        assertEquals(second.id(), reopened.activeAccount().id());
        assertEquals(Account.Type.OFFLINE, first.type());
        assertEquals("0", reopened.activeLaunchProfile(new MicrosoftAuthService(), "").accessToken());
    }

    @Test
    void storesMicrosoftTokensOnlyInCredentialStoreAndRemovesThemWithAccount() throws IOException {
        MemoryCredentialStore credentials = new MemoryCredentialStore();
        AccountManager manager = new AccountManager(temporaryDirectory, credentials);
        MicrosoftAuthService.AuthenticatedProfile profile = new MicrosoftAuthService.AuthenticatedProfile(
                "Player", "0123456789abcdef0123456789abcdef", "xuid",
                "minecraft-access-secret", "refresh-secret", 1234);

        Account account = manager.addMicrosoft(profile);
        String metadata = Files.readString(temporaryDirectory.resolve("accounts.json"));

        assertFalse(metadata.contains("minecraft-access-secret"));
        assertFalse(metadata.contains("refresh-secret"));
        assertTrue(credentials.get(account.id()).contains("refresh-secret"));
        manager.remove(account.id());
        assertFalse(credentials.values.containsKey(account.id()));
    }

    @Test
    void returnsCachedMicrosoftAccessTokenUntilItIsNearExpiry() throws IOException, InterruptedException {
        MemoryCredentialStore credentials = new MemoryCredentialStore();
        AccountManager manager = new AccountManager(temporaryDirectory, credentials);
        MicrosoftAuthService.AuthenticatedProfile profile = new MicrosoftAuthService.AuthenticatedProfile(
                "Player", "0123456789abcdef0123456789abcdef", "xuid",
                "minecraft-access-token", "refresh-secret", System.currentTimeMillis() / 1000 + 3600);
        Account account = manager.addMicrosoft(profile);

        AccountManager.LaunchProfile launchProfile =
                manager.activeLaunchProfile(new MicrosoftAuthService(), "");

        assertEquals(account, launchProfile.account());
        assertEquals("minecraft-access-token", launchProfile.accessToken());
    }

    @Test
    void rejectsInvalidOfflineNames() throws IOException {
        AccountManager manager = new AccountManager(temporaryDirectory, new MemoryCredentialStore());

        assertThrows(IllegalArgumentException.class, () -> manager.addOffline("not a name"));
    }

    private static final class MemoryCredentialStore implements CredentialStore {
        private final Map<String, String> values = new HashMap<>();

        @Override
        public void put(String accountId, String secret) {
            values.put(accountId, secret);
        }

        @Override
        public String get(String accountId) {
            return values.get(accountId);
        }

        @Override
        public void remove(String accountId) {
            values.remove(accountId);
        }
    }
}
