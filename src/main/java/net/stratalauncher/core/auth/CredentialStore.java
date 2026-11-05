package net.stratalauncher.core.auth;

import java.io.IOException;

public interface CredentialStore {
    void put(String accountId, String secret) throws IOException;

    String get(String accountId) throws IOException;

    void remove(String accountId) throws IOException;
}
