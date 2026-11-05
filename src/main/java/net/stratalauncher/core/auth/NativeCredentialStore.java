package net.stratalauncher.core.auth;

import com.github.javakeyring.BackendNotSupportedException;
import com.github.javakeyring.Keyring;
import com.github.javakeyring.PasswordAccessException;

import java.io.IOException;

public final class NativeCredentialStore implements CredentialStore {
    private static final String SERVICE = "StrataLauncher/Microsoft";

    @Override
    public void put(String accountId, String secret) throws IOException {
        withKeyring(keyring -> {
            keyring.setPassword(SERVICE, accountId, secret);
            return null;
        });
    }

    @Override
    public String get(String accountId) throws IOException {
        return withKeyring(keyring -> keyring.getPassword(SERVICE, accountId));
    }

    @Override
    public void remove(String accountId) throws IOException {
        withKeyring(keyring -> {
            keyring.deletePassword(SERVICE, accountId);
            return null;
        });
    }

    private static <T> T withKeyring(KeyringOperation<T> operation) throws IOException {
        try (Keyring keyring = Keyring.create()) {
            return operation.apply(keyring);
        } catch (BackendNotSupportedException exception) {
            throw new IOException("The native operating-system credential store is not available.", exception);
        } catch (PasswordAccessException exception) {
            throw new IOException("Could not access the native operating-system credential store.", exception);
        } catch (Exception exception) {
            throw new IOException("Could not close the native operating-system credential store.", exception);
        }
    }

    @FunctionalInterface
    private interface KeyringOperation<T> {
        T apply(Keyring keyring) throws PasswordAccessException;
    }
}
