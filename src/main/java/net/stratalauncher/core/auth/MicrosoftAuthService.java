package net.stratalauncher.core.auth;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import com.google.gson.JsonParser;

import java.io.IOException;
import java.net.URI;
import java.net.URLEncoder;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.time.Instant;
import java.util.Objects;
import java.util.Locale;
import java.util.UUID;
import java.util.function.Consumer;

public final class MicrosoftAuthService {
    private static final URI DEVICE_CODE_ENDPOINT =
            URI.create("https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode");
    private static final URI TOKEN_ENDPOINT =
            URI.create("https://login.microsoftonline.com/consumers/oauth2/v2.0/token");
    private static final URI XBOX_USER_ENDPOINT = URI.create("https://user.auth.xboxlive.com/user/authenticate");
    private static final URI XBOX_XSTS_ENDPOINT = URI.create("https://xsts.auth.xboxlive.com/xsts/authorize");
    private static final URI MINECRAFT_LOGIN_ENDPOINT =
            URI.create("https://api.minecraftservices.com/authentication/login_with_xbox");
    private static final URI MINECRAFT_PROFILE_ENDPOINT =
            URI.create("https://api.minecraftservices.com/minecraft/profile");
    private static final Duration REQUEST_TIMEOUT = Duration.ofSeconds(30);
    private final HttpClient httpClient;

    public MicrosoftAuthService() {
        this(HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(15)).build());
    }

    MicrosoftAuthService(HttpClient httpClient) {
        this.httpClient = Objects.requireNonNull(httpClient);
    }

    public AuthenticatedProfile signIn(String clientId, Consumer<DeviceCode> deviceCodePrompt)
            throws IOException, InterruptedException {
        requireClientId(clientId);
        JsonObject deviceResponse = postForm(DEVICE_CODE_ENDPOINT,
                "client_id", clientId,
                "scope", "XboxLive.signin offline_access");
        DeviceCode deviceCode = parseDeviceCode(deviceResponse);
        Objects.requireNonNull(deviceCodePrompt).accept(deviceCode);

        Instant expires = Instant.now().plusSeconds(deviceCode.expiresIn());
        int interval = deviceCode.interval();
        while (Instant.now().isBefore(expires)) {
            Thread.sleep(Duration.ofSeconds(interval));
            JsonObject response = postFormAllowOAuthErrors(TOKEN_ENDPOINT,
                    "grant_type", "urn:ietf:params:oauth:grant-type:device_code",
                    "client_id", clientId,
                    "device_code", deviceCode.deviceCode());
            if (response.has("access_token")) {
                String microsoftAccessToken = requiredString(response, "access_token");
                String refreshToken = optionalString(response, "refresh_token");
                return authenticateMinecraft(microsoftAccessToken, refreshToken);
            }
            String error = optionalString(response, "error");
            if ("authorization_pending".equals(error)) {
                continue;
            }
            if ("slow_down".equals(error)) {
                interval += 5;
                continue;
            }
            if ("authorization_declined".equals(error)) {
                throw new IOException("Microsoft sign-in was declined.");
            }
            if ("expired_token".equals(error)) {
                throw new IOException("Microsoft device code expired. Start sign-in again.");
            }
            throw new IOException("Microsoft sign-in failed: " + safeError(response));
        }
        throw new IOException("Microsoft device code expired. Start sign-in again.");
    }

    public AuthenticatedProfile refresh(String clientId, String refreshToken)
            throws IOException, InterruptedException {
        requireClientId(clientId);
        JsonObject response = postForm(TOKEN_ENDPOINT,
                "grant_type", "refresh_token",
                "client_id", clientId,
                "refresh_token", refreshToken,
                "scope", "XboxLive.signin offline_access");
        String microsoftAccessToken = requiredString(response, "access_token");
        String nextRefreshToken = optionalString(response, "refresh_token");
        return authenticateMinecraft(microsoftAccessToken,
                nextRefreshToken.isBlank() ? refreshToken : nextRefreshToken);
    }

    static DeviceCode parseDeviceCode(JsonObject response) throws IOException {
        return new DeviceCode(requiredString(response, "device_code"),
                requiredString(response, "user_code"),
                requiredString(response, "verification_uri"),
                optionalString(response, "message"),
                positiveInt(response, "expires_in", 900),
                positiveInt(response, "interval", 5));
    }

    static AuthenticatedProfile parseProfile(JsonObject response, String accessToken, String refreshToken)
            throws IOException {
        String name = requiredString(response, "name");
        String uuid = requiredString(response, "id").replace("-", "").toLowerCase(Locale.ROOT);
        try {
            if (uuid.length() != 32) {
                throw new IllegalArgumentException();
            }
            UUID.fromString(uuid.substring(0, 8) + "-" + uuid.substring(8, 12) + "-"
                    + uuid.substring(12, 16) + "-" + uuid.substring(16, 20) + "-" + uuid.substring(20));
        } catch (IllegalArgumentException exception) {
            throw new IOException("Microsoft returned an invalid Minecraft profile identifier.");
        }
        return new AuthenticatedProfile(name, uuid, "", accessToken, refreshToken, 0);
    }

    private AuthenticatedProfile authenticateMinecraft(String microsoftAccessToken, String refreshToken)
            throws IOException, InterruptedException {
        JsonObject userProperties = new JsonObject();
        userProperties.addProperty("AuthMethod", "RPS");
        userProperties.addProperty("SiteName", "user.auth.xboxlive.com");
        userProperties.addProperty("RpsTicket", "d=" + microsoftAccessToken);
        JsonObject userPayload = new JsonObject();
        userPayload.add("Properties", userProperties);
        userPayload.addProperty("RelyingParty", "http://auth.xboxlive.com");
        userPayload.addProperty("TokenType", "JWT");
        JsonObject xboxUser = postJson(XBOX_USER_ENDPOINT, userPayload);
        String xboxToken = requiredString(xboxUser, "Token");
        String userHash = firstUserHash(xboxUser);

        JsonObject xstsProperties = new JsonObject();
        xstsProperties.addProperty("SandboxId", "RETAIL");
        JsonArray userTokens = new JsonArray();
        userTokens.add(xboxToken);
        xstsProperties.add("UserTokens", userTokens);
        JsonObject xstsPayload = new JsonObject();
        xstsPayload.add("Properties", xstsProperties);
        xstsPayload.addProperty("RelyingParty", "rp://api.minecraftservices.com/");
        xstsPayload.addProperty("TokenType", "JWT");
        JsonObject xstsResponse = postJson(XBOX_XSTS_ENDPOINT, xstsPayload);
        String xstsToken = requiredString(xstsResponse, "Token");
        String xstsUserHash = firstUserHash(xstsResponse);
        if (!userHash.equals(xstsUserHash)) {
            throw new IOException("Xbox authentication returned inconsistent account details.");
        }

        JsonObject minecraftLogin = new JsonObject();
        minecraftLogin.addProperty("identityToken", "XBL3.0 x=" + userHash + ";" + xstsToken);
        JsonObject minecraftResponse = postJson(MINECRAFT_LOGIN_ENDPOINT, minecraftLogin);
        String minecraftAccessToken = requiredString(minecraftResponse, "access_token");

        HttpRequest profileRequest = HttpRequest.newBuilder(MINECRAFT_PROFILE_ENDPOINT)
                .timeout(REQUEST_TIMEOUT)
                .header("Authorization", "Bearer " + minecraftAccessToken)
                .header("Accept", "application/json")
                .GET()
                .build();
        JsonObject profile = sendJson(profileRequest);
        AuthenticatedProfile parsedProfile = parseProfile(profile, minecraftAccessToken, refreshToken);
        return new AuthenticatedProfile(parsedProfile.name(), parsedProfile.uuid(), userHash,
                parsedProfile.accessToken(), parsedProfile.refreshToken(),
                Instant.now().plusSeconds(optionalLong(minecraftResponse, "expires_in", 0)).getEpochSecond());
    }

    private JsonObject postForm(URI uri, String... pairs) throws IOException, InterruptedException {
        return postForm(uri, false, pairs);
    }

    private JsonObject postFormAllowOAuthErrors(URI uri, String... pairs) throws IOException, InterruptedException {
        return postForm(uri, true, pairs);
    }

    private JsonObject postForm(URI uri, boolean allowOAuthErrors, String... pairs)
            throws IOException, InterruptedException {
        StringBuilder form = new StringBuilder();
        for (int i = 0; i < pairs.length; i += 2) {
            if (!form.isEmpty()) {
                form.append('&');
            }
            form.append(encode(pairs[i])).append('=').append(encode(pairs[i + 1]));
        }
        HttpRequest request = HttpRequest.newBuilder(uri)
                .timeout(REQUEST_TIMEOUT)
                .header("Content-Type", "application/x-www-form-urlencoded")
                .header("Accept", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(form.toString()))
                .build();
        return sendJson(request, allowOAuthErrors);
    }

    private JsonObject postJson(URI uri, JsonObject payload) throws IOException, InterruptedException {
        HttpRequest request = HttpRequest.newBuilder(uri)
                .timeout(REQUEST_TIMEOUT)
                .header("Content-Type", "application/json")
                .header("Accept", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(payload.toString()))
                .build();
        return sendJson(request);
    }

    private JsonObject sendJson(HttpRequest request) throws IOException, InterruptedException {
        return sendJson(request, false);
    }

    private JsonObject sendJson(HttpRequest request, boolean allowOAuthErrors) throws IOException, InterruptedException {
        HttpResponse<String> response = httpClient.send(request,
                HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));
        JsonObject body;
        try {
            JsonElement parsed = JsonParser.parseString(response.body());
            body = parsed.isJsonObject() ? parsed.getAsJsonObject() : new JsonObject();
        } catch (JsonParseException exception) {
            throw new IOException("Authentication service returned an invalid response.", exception);
        }
        if ((response.statusCode() < 200 || response.statusCode() >= 300) && !allowOAuthErrors) {
            String error = safeError(body);
            if (body.has("XErr")) {
                error = "Xbox authentication error " + optionalString(body, "XErr");
            }
            throw new IOException("Authentication service rejected the request (HTTP "
                    + response.statusCode() + "): " + error);
        }
        return body;
    }

    private static String firstUserHash(JsonObject response) throws IOException {
        JsonObject claims = requiredObject(response, "DisplayClaims");
        JsonArray users = requiredArray(claims, "xui");
        if (users.isEmpty() || !users.get(0).isJsonObject()) {
            throw new IOException("Xbox authentication returned no user identifier.");
        }
        return requiredString(users.get(0).getAsJsonObject(), "uhs");
    }

    private static JsonObject requiredObject(JsonObject object, String key) throws IOException {
        if (!object.has(key) || !object.get(key).isJsonObject()) {
            throw new IOException("Authentication response is missing " + key + ".");
        }
        return object.getAsJsonObject(key);
    }

    private static JsonArray requiredArray(JsonObject object, String key) throws IOException {
        if (!object.has(key) || !object.get(key).isJsonArray()) {
            throw new IOException("Authentication response is missing " + key + ".");
        }
        return object.getAsJsonArray(key);
    }

    private static String requiredString(JsonObject object, String key) throws IOException {
        String value = optionalString(object, key);
        if (value.isBlank()) {
            throw new IOException("Authentication response is missing " + key + ".");
        }
        return value;
    }

    private static String optionalString(JsonObject object, String key) {
        return object.has(key) && !object.get(key).isJsonNull() ? object.get(key).getAsString() : "";
    }

    private static int positiveInt(JsonObject object, String key, int fallback) {
        long value = optionalLong(object, key, fallback);
        return value > 0 && value <= Integer.MAX_VALUE ? (int) value : fallback;
    }

    private static long optionalLong(JsonObject object, String key, long fallback) {
        try {
            return object.has(key) ? object.get(key).getAsLong() : fallback;
        } catch (NumberFormatException exception) {
            return fallback;
        }
    }

    private static String safeError(JsonObject response) {
        String error = optionalString(response, "error");
        if (error.isBlank()) {
            error = optionalString(response, "error_description");
        }
        return error.isBlank() ? "please check your account and try again" : error;
    }

    private static String encode(String value) {
        return URLEncoder.encode(value, StandardCharsets.UTF_8);
    }

    private static void requireClientId(String clientId) {
        if (clientId == null || !clientId.matches("(?i)[0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12}")) {
            throw new IllegalArgumentException("Configure a valid Azure client ID in launcher settings first.");
        }
    }

    public record DeviceCode(String deviceCode, String userCode, String verificationUri, String message,
                             int expiresIn, int interval) {
    }

    public record AuthenticatedProfile(String name, String uuid, String xuid, String accessToken,
                                       String refreshToken, long expiresAtEpochSecond) {
    }
}
