package net.stratalauncher.core.auth;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonParseException;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.UUID;

public final class AccountManager {
    private static final Gson GSON = new GsonBuilder().setPrettyPrinting().create();

    private final Path accountsFile;
    private final CredentialStore credentialStore;
    private final List<Account> accounts = new ArrayList<>();
    private String activeAccountId;

    public AccountManager(Path dataDirectory, CredentialStore credentialStore) throws IOException {
        accountsFile = dataDirectory.toAbsolutePath().normalize().resolve("accounts.json");
        this.credentialStore = Objects.requireNonNull(credentialStore);
        load();
    }

    public synchronized List<Account> accounts() {
        return List.copyOf(accounts);
    }

    public synchronized Account activeAccount() {
        return accounts.stream().filter(account -> account.id().equals(activeAccountId)).findFirst().orElse(null);
    }

    public synchronized Account addOffline(String name) throws IOException {
        String normalized = requireOfflineName(name);
        Account account = new Account(UUID.randomUUID().toString(), normalized,
                offlineUuid(normalized), Account.Type.OFFLINE, "");
        accounts.add(account);
        String oldActiveId = activeAccountId;
        if (activeAccountId == null) {
            activeAccountId = account.id();
        }
        try {
            save();
        } catch (IOException exception) {
            accounts.remove(account);
            activeAccountId = oldActiveId;
            throw exception;
        }
        return account;
    }

    public synchronized Account addMicrosoft(MicrosoftAuthService.AuthenticatedProfile profile) throws IOException {
        Objects.requireNonNull(profile);
        validateMinecraftUuid(profile.uuid());
        Account existing = accounts.stream()
                .filter(account -> account.type() == Account.Type.MICROSOFT && account.uuid().equals(profile.uuid()))
                .findFirst().orElse(null);
        String accountId = existing == null ? UUID.randomUUID().toString() : existing.id();
        Account account = new Account(accountId, profile.name(), profile.uuid(), Account.Type.MICROSOFT, profile.xuid());
        TokenData tokenData = new TokenData(profile.accessToken(), profile.refreshToken(), profile.expiresAtEpochSecond());
        credentialStore.put(accountId, GSON.toJson(tokenData));

        if (existing == null) {
            accounts.add(account);
        } else {
            accounts.set(accounts.indexOf(existing), account);
        }
        activeAccountId = accountId;
        try {
            save();
        } catch (IOException exception) {
            if (existing == null) {
                accounts.remove(account);
                activeAccountId = accounts.isEmpty() ? null : accounts.getFirst().id();
            } else {
                accounts.set(accounts.indexOf(account), existing);
                activeAccountId = existing.id();
            }
            if (existing == null) {
                try {
                    credentialStore.remove(accountId);
                } catch (IOException cleanupException) {
                    exception.addSuppressed(cleanupException);
                }
            }
            throw exception;
        }
        return account;
    }

    public synchronized void select(String accountId) throws IOException {
        if (accounts.stream().noneMatch(account -> account.id().equals(accountId))) {
            throw new IllegalArgumentException("Account does not exist.");
        }
        String oldActiveId = activeAccountId;
        activeAccountId = accountId;
        try {
            save();
        } catch (IOException exception) {
            activeAccountId = oldActiveId;
            throw exception;
        }
    }

    public synchronized void remove(String accountId) throws IOException {
        Account account = accounts.stream().filter(candidate -> candidate.id().equals(accountId))
                .findFirst().orElseThrow(() -> new IllegalArgumentException("Account does not exist."));
        int index = accounts.indexOf(account);
        String oldActiveId = activeAccountId;
        accounts.remove(account);
        if (account.id().equals(activeAccountId)) {
            activeAccountId = accounts.isEmpty() ? null : accounts.getFirst().id();
        }
        try {
            save();
        } catch (IOException exception) {
            accounts.add(index, account);
            activeAccountId = oldActiveId;
            throw exception;
        }
        if (account.type() == Account.Type.MICROSOFT) {
            credentialStore.remove(account.id());
        }
    }

    public LaunchProfile activeLaunchProfile(MicrosoftAuthService authService, String clientId)
            throws IOException, InterruptedException {
        Account account;
        synchronized (this) {
            account = activeAccount();
        }
        if (account == null) {
            throw new IOException("Add or select an account before launching Minecraft.");
        }
        if (account.type() == Account.Type.OFFLINE) {
            return new LaunchProfile(account, "0");
        }
        String stored = credentialStore.get(account.id());
        if (stored == null || stored.isBlank()) {
            throw new IOException("Cached Microsoft credentials are missing. Sign in to this account again.");
        }
        TokenData tokenData;
        try {
            tokenData = GSON.fromJson(stored, TokenData.class);
        } catch (JsonParseException exception) {
            throw new IOException("Cached Microsoft credentials are invalid. Sign in to this account again.", exception);
        }
        if (tokenData == null || tokenData.refreshToken == null || tokenData.refreshToken.isBlank()) {
            throw new IOException("Cached Microsoft credentials cannot be refreshed. Sign in to this account again.");
        }
        if (tokenData.accessToken != null && !tokenData.accessToken.isBlank()
                && tokenData.expiresAtEpochSecond > java.time.Instant.now().plusSeconds(120).getEpochSecond()) {
            return new LaunchProfile(account, tokenData.accessToken);
        }
        MicrosoftAuthService.AuthenticatedProfile refreshed = authService.refresh(clientId, tokenData.refreshToken);
        if (!account.uuid().equalsIgnoreCase(refreshed.uuid())) {
            throw new IOException("Microsoft authentication returned a different account. Sign in again.");
        }
        Account updated = new Account(account.id(), refreshed.name(), account.uuid(),
                Account.Type.MICROSOFT, refreshed.xuid());
        synchronized (this) {
            int currentIndex = accounts.indexOf(account);
            if (currentIndex < 0) {
                throw new IOException("The Microsoft account was removed while refreshing its credentials.");
            }
            credentialStore.put(account.id(), GSON.toJson(new TokenData(refreshed.accessToken(),
                    refreshed.refreshToken(), refreshed.expiresAtEpochSecond())));
            accounts.set(currentIndex, updated);
            try {
                save();
            } catch (IOException exception) {
                accounts.set(currentIndex, account);
                throw exception;
            }
        }
        return new LaunchProfile(updated, refreshed.accessToken());
    }

    private void load() throws IOException {
        if (!Files.isRegularFile(accountsFile)) {
            return;
        }
        try {
            AccountsData data = GSON.fromJson(Files.readString(accountsFile), AccountsData.class);
            if (data == null) {
                throw new IOException("Account data is empty: " + accountsFile);
            }
            if (data.accounts != null) {
                for (Account account : data.accounts) {
                    validateAccount(account);
                    if (accounts.stream().anyMatch(existing -> existing.id().equals(account.id()))) {
                        throw new IOException("Account data contains a duplicate account identifier.");
                    }
                    accounts.add(account);
                }
            }
            activeAccountId = accounts.stream().anyMatch(account -> account.id().equals(data.activeAccountId))
                    ? data.activeAccountId : accounts.isEmpty() ? null : accounts.getFirst().id();
        } catch (JsonParseException exception) {
            throw new IOException("Could not parse account data: " + accountsFile, exception);
        }
    }

    private void save() throws IOException {
        Files.createDirectories(accountsFile.getParent());
        Path temporary = Files.createTempFile(accountsFile.getParent(), "accounts-", ".tmp");
        try {
            Files.writeString(temporary, GSON.toJson(new AccountsData(List.copyOf(accounts), activeAccountId)));
            try {
                Files.move(temporary, accountsFile, StandardCopyOption.ATOMIC_MOVE,
                        StandardCopyOption.REPLACE_EXISTING);
            } catch (java.nio.file.AtomicMoveNotSupportedException exception) {
                Files.move(temporary, accountsFile, StandardCopyOption.REPLACE_EXISTING);
            }
        } finally {
            Files.deleteIfExists(temporary);
        }
    }

    private static void validateAccount(Account account) throws IOException {
        if (account == null || account.id() == null || account.name() == null || account.type() == null) {
            throw new IOException("Account data contains an incomplete account record.");
        }
        try {
            UUID.fromString(account.id());
            validateMinecraftUuid(account.uuid());
        } catch (IllegalArgumentException exception) {
            throw new IOException("Account data contains an invalid identifier.", exception);
        }
    }

    private static void validateMinecraftUuid(String uuid) {
        if (uuid == null || !uuid.replace("-", "").matches("(?i)[0-9a-f]{32}")) {
            throw new IllegalArgumentException("Minecraft account has an invalid profile identifier.");
        }
    }

    private static String requireOfflineName(String name) {
        if (name == null || !name.trim().matches("[A-Za-z0-9_]{1,16}")) {
            throw new IllegalArgumentException("Offline account name must be 1-16 letters, numbers, or underscores.");
        }
        return name.trim();
    }

    private static String offlineUuid(String playerName) {
        return UUID.nameUUIDFromBytes(("OfflinePlayer:" + playerName).getBytes(java.nio.charset.StandardCharsets.UTF_8))
                .toString().replace("-", "");
    }

    public record LaunchProfile(Account account, String accessToken) {
    }

    private record TokenData(String accessToken, String refreshToken, long expiresAtEpochSecond) {
    }

    private record AccountsData(List<Account> accounts, String activeAccountId) {
    }
}
