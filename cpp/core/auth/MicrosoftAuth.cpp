#include "MicrosoftAuth.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>

#include <stdexcept>

namespace strata::auth
{
    namespace
    {
        const QUrl deviceCodeEndpoint(
            QStringLiteral("https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode"));
        const QUrl tokenEndpoint(
            QStringLiteral("https://login.microsoftonline.com/consumers/oauth2/v2.0/token"));
        const QUrl
            xboxUserEndpoint(QStringLiteral("https://user.auth.xboxlive.com/user/authenticate"));
        const QUrl
            xboxXstsEndpoint(QStringLiteral("https://xsts.auth.xboxlive.com/xsts/authorize"));
        const QUrl minecraftLoginEndpoint(
            QStringLiteral("https://api.minecraftservices.com/authentication/login_with_xbox"));
        const QUrl minecraftProfileEndpoint(
            QStringLiteral("https://api.minecraftservices.com/minecraft/profile"));
        constexpr qsizetype maximumResponseBytes = 1024 * 1024;
        constexpr int requestTimeoutMilliseconds = 30000;

        QJsonObject requestJson(QNetworkAccessManager& manager, const QUrl& url,
                                const QByteArray& payload, const QByteArray& contentType,
                                const QByteArray& bearerToken = {}, bool oauthErrors = false)
        {
            QNetworkRequest request(url);
            request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
            request.setRawHeader("Accept", "application/json");
            if (!bearerToken.isEmpty())
            {
                request.setRawHeader("Authorization", "Bearer " + bearerToken);
            }
            request.setTransferTimeout(requestTimeoutMilliseconds);
            QNetworkReply* reply = manager.post(request, payload);
            QByteArray body;
            bool responseTooLarge = false;
            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            QObject::connect(reply, &QNetworkReply::readyRead, reply,
                             [reply, &body, &responseTooLarge]
                             {
                                 body.append(reply->read(maximumResponseBytes + 1 - body.size()));
                                 if (body.size() > maximumResponseBytes)
                                 {
                                     body.truncate(maximumResponseBytes + 1);
                                     responseTooLarge = true;
                                     reply->abort();
                                 }
                             });
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
            timeout.start(requestTimeoutMilliseconds);
            loop.exec();
            timeout.stop();
            if (!responseTooLarge)
            {
                body.append(reply->readAll());
            }
            const auto networkError = reply->error();
            const int statusCode =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QString errorString = reply->errorString();
            reply->deleteLater();
            if (responseTooLarge || body.size() > maximumResponseBytes)
            {
                throw std::runtime_error("Authentication response exceeded the size limit");
            }
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
            {
                throw std::runtime_error("Authentication service returned an invalid response");
            }
            const QJsonObject object = document.object();
            if ((networkError != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300) &&
                !oauthErrors)
            {
                const QString xboxError = object.value(QStringLiteral("XErr")).toString();
                const QString serviceError =
                    !xboxError.isEmpty()
                        ? QStringLiteral("Xbox authentication error %1").arg(xboxError)
                        : object.value(QStringLiteral("error_description"))
                              .toString(object.value(QStringLiteral("error"))
                                            .toString(QStringLiteral(
                                                "please check your account and try again")));
                throw std::runtime_error(
                    QStringLiteral("Authentication service rejected the request (HTTP %1): %2")
                        .arg(statusCode)
                        .arg(serviceError.isEmpty() ? errorString : serviceError)
                        .toStdString());
            }
            return object;
        }

        QJsonObject postForm(QNetworkAccessManager& manager, const QUrl& endpoint,
                             const QUrlQuery& parameters, bool oauthErrors = false)
        {
            return requestJson(manager, endpoint, parameters.toString(QUrl::FullyEncoded).toUtf8(),
                               "application/x-www-form-urlencoded", {}, oauthErrors);
        }

        QString requiredString(const QJsonObject& object, const QString& key)
        {
            const QString value = object.value(key).toString();
            if (value.isEmpty())
            {
                throw std::runtime_error(
                    QStringLiteral("Authentication response is missing %1").arg(key).toStdString());
            }
            return value;
        }

        QString safeOAuthError(const QJsonObject& object)
        {
            QString error = object.value(QStringLiteral("error_description")).toString();
            if (error.isEmpty())
            {
                error = object.value(QStringLiteral("error")).toString();
            }
            return error.isEmpty() ? QStringLiteral("please check your account and try again")
                                   : error;
        }

        QString userHash(const QJsonObject& object)
        {
            const QJsonArray users = object.value(QStringLiteral("DisplayClaims"))
                                         .toObject()
                                         .value(QStringLiteral("xui"))
                                         .toArray();
            if (users.isEmpty() ||
                users.first().toObject().value(QStringLiteral("uhs")).toString().isEmpty())
            {
                throw std::runtime_error("Xbox authentication returned no user identifier");
            }
            return users.first().toObject().value(QStringLiteral("uhs")).toString();
        }

        DeviceCode parseDeviceCode(const QJsonObject& response)
        {
            DeviceCode result;
            result.deviceCode = requiredString(response, QStringLiteral("device_code"));
            result.userCode = requiredString(response, QStringLiteral("user_code"));
            result.verificationUri =
                QUrl(requiredString(response, QStringLiteral("verification_uri")));
            if (!result.verificationUri.isValid() ||
                result.verificationUri.scheme() != QStringLiteral("https") ||
                (result.verificationUri.host() != QStringLiteral("microsoft.com") &&
                 result.verificationUri.host() != QStringLiteral("www.microsoft.com") &&
                 result.verificationUri.host() != QStringLiteral("microsoftonline.com")))
            {
                throw std::runtime_error("Microsoft returned an invalid verification URL");
            }
            result.message = response.value(QStringLiteral("message")).toString();
            const int expiration = response.value(QStringLiteral("expires_in")).toInt(900);
            const int interval = response.value(QStringLiteral("interval")).toInt(5);
            result.expiresIn = expiration > 0 ? expiration : 900;
            result.interval = interval > 0 ? interval : 5;
            return result;
        }

        void validateClientId(const QString& clientId)
        {
            if (!QRegularExpression(
                     QStringLiteral("(?i)^[0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12}$"))
                     .match(clientId)
                     .hasMatch())
            {
                throw std::invalid_argument(
                    "Configure a valid Microsoft client ID in the account panel first");
            }
        }
    } // namespace

    MicrosoftAuth::MicrosoftAuth(QObject* parent) : QObject(parent) {}

    AuthenticatedProfile MicrosoftAuth::signIn(const QString& clientId)
    {
        validateClientId(clientId);
        m_cancelled.store(false);
        QNetworkAccessManager manager;
        QUrlQuery deviceParameters;
        deviceParameters.addQueryItem(QStringLiteral("client_id"), clientId);
        deviceParameters.addQueryItem(QStringLiteral("scope"),
                                      QStringLiteral("XboxLive.signin offline_access"));
        const DeviceCode deviceCode =
            parseDeviceCodeResponse(postForm(manager, deviceCodeEndpoint, deviceParameters));
        emit userCodeReady(deviceCode.userCode, deviceCode.verificationUri, deviceCode.message);

        int interval = deviceCode.interval;
        for (int elapsed = 0; elapsed < deviceCode.expiresIn;)
        {
            if (m_cancelled.load())
            {
                throw std::runtime_error("Microsoft sign-in was cancelled");
            }
            const int waitSeconds = qMin(interval, deviceCode.expiresIn - elapsed);
            int remainingMilliseconds = waitSeconds * 1000;
            while (remainingMilliseconds > 0 && !m_cancelled.load())
            {
                const int sleepMilliseconds = qMin(remainingMilliseconds, 100);
                QThread::msleep(static_cast<unsigned long>(sleepMilliseconds));
                remainingMilliseconds -= sleepMilliseconds;
            }
            elapsed += waitSeconds;
            if (m_cancelled.load())
            {
                throw std::runtime_error("Microsoft sign-in was cancelled");
            }
            QUrlQuery parameters;
            parameters.addQueryItem(QStringLiteral("grant_type"),
                                    QStringLiteral("urn:ietf:params:oauth:grant-type:device_code"));
            parameters.addQueryItem(QStringLiteral("client_id"), clientId);
            parameters.addQueryItem(QStringLiteral("device_code"), deviceCode.deviceCode);
            const QJsonObject response = postForm(manager, tokenEndpoint, parameters, true);
            if (response.contains(QStringLiteral("access_token")))
            {
                return authenticateMinecraft(
                    requiredString(response, QStringLiteral("access_token")),
                    response.value(QStringLiteral("refresh_token")).toString());
            }
            const QString error = response.value(QStringLiteral("error")).toString();
            if (error == QStringLiteral("authorization_pending"))
            {
                continue;
            }
            if (error == QStringLiteral("slow_down"))
            {
                interval += 5;
                continue;
            }
            if (error == QStringLiteral("authorization_declined"))
            {
                throw std::runtime_error("Microsoft sign-in was declined");
            }
            if (error == QStringLiteral("expired_token"))
            {
                throw std::runtime_error("Microsoft device code expired. Start sign-in again");
            }
            throw std::runtime_error(QStringLiteral("Microsoft sign-in failed: %1")
                                         .arg(safeOAuthError(response))
                                         .toStdString());
        }
        throw std::runtime_error("Microsoft device code expired. Start sign-in again");
    }

    AuthenticatedProfile MicrosoftAuth::refresh(const QString& clientId,
                                                const QString& refreshToken)
    {
        validateClientId(clientId);
        QNetworkAccessManager manager;
        QUrlQuery parameters;
        parameters.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
        parameters.addQueryItem(QStringLiteral("client_id"), clientId);
        parameters.addQueryItem(QStringLiteral("refresh_token"), refreshToken);
        parameters.addQueryItem(QStringLiteral("scope"),
                                QStringLiteral("XboxLive.signin offline_access"));
        const QJsonObject response = postForm(manager, tokenEndpoint, parameters);
        const QString nextRefreshToken =
            response.value(QStringLiteral("refresh_token")).toString(refreshToken);
        return authenticateMinecraft(requiredString(response, QStringLiteral("access_token")),
                                     nextRefreshToken);
    }

    void MicrosoftAuth::cancel()
    {
        m_cancelled.store(true);
    }

    DeviceCode MicrosoftAuth::parseDeviceCodeResponse(const QJsonObject& response)
    {
        return parseDeviceCode(response);
    }

    AuthenticatedProfile MicrosoftAuth::parseProfileResponse(const QJsonObject& response,
                                                             const QString& xuid,
                                                             const QString& accessToken,
                                                             const QString& refreshToken)
    {
        const QString uuid =
            requiredString(response, QStringLiteral("id")).remove(QLatin1Char('-')).toLower();
        if (!QRegularExpression(QStringLiteral("(?i)^[0-9a-f]{32}$")).match(uuid).hasMatch())
        {
            throw std::runtime_error("Microsoft returned an invalid Minecraft profile identifier");
        }
        return {requiredString(response, QStringLiteral("name")),
                uuid,
                xuid,
                accessToken,
                refreshToken,
                0};
    }

    AuthenticatedProfile MicrosoftAuth::authenticateMinecraft(const QString& microsoftAccessToken,
                                                              const QString& refreshToken)
    {
        QNetworkAccessManager manager;
        const QJsonObject userProperties{
            {QStringLiteral("AuthMethod"), QStringLiteral("RPS")},
            {QStringLiteral("SiteName"), QStringLiteral("user.auth.xboxlive.com")},
            {QStringLiteral("RpsTicket"), QStringLiteral("d=") + microsoftAccessToken}};
        const QJsonObject xboxUser = requestJson(
            manager, xboxUserEndpoint,
            QJsonDocument(QJsonObject{{QStringLiteral("Properties"), userProperties},
                                      {QStringLiteral("RelyingParty"),
                                       QStringLiteral("http://auth.xboxlive.com")},
                                      {QStringLiteral("TokenType"), QStringLiteral("JWT")}})
                .toJson(QJsonDocument::Compact),
            "application/json");
        const QString xboxToken = requiredString(xboxUser, QStringLiteral("Token"));
        const QString xboxHash = userHash(xboxUser);
        const QJsonObject xstsProperties{{QStringLiteral("SandboxId"), QStringLiteral("RETAIL")},
                                         {QStringLiteral("UserTokens"), QJsonArray{xboxToken}}};
        const QJsonObject xstsResponse = requestJson(
            manager, xboxXstsEndpoint,
            QJsonDocument(QJsonObject{{QStringLiteral("Properties"), xstsProperties},
                                      {QStringLiteral("RelyingParty"),
                                       QStringLiteral("rp://api.minecraftservices.com/")},
                                      {QStringLiteral("TokenType"), QStringLiteral("JWT")}})
                .toJson(QJsonDocument::Compact),
            "application/json");
        const QString xstsToken = requiredString(xstsResponse, QStringLiteral("Token"));
        const QString xstsHash = userHash(xstsResponse);
        if (xboxHash != xstsHash)
        {
            throw std::runtime_error("Xbox authentication returned inconsistent account details");
        }
        const QJsonObject minecraftResponse = requestJson(
            manager, minecraftLoginEndpoint,
            QJsonDocument(QJsonObject{{QStringLiteral("identityToken"),
                                       QStringLiteral("XBL3.0 x=%1;%2").arg(xboxHash, xstsToken)}})
                .toJson(QJsonDocument::Compact),
            "application/json");
        const QString accessToken =
            requiredString(minecraftResponse, QStringLiteral("access_token"));
        QNetworkRequest request(minecraftProfileEndpoint);
        request.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());
        request.setRawHeader("Accept", "application/json");
        request.setTransferTimeout(requestTimeoutMilliseconds);
        QNetworkReply* reply = manager.get(request);
        QByteArray profileBody;
        bool responseTooLarge = false;
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::readyRead, reply,
                         [reply, &profileBody, &responseTooLarge]
                         {
                             profileBody.append(
                                 reply->read(maximumResponseBytes + 1 - profileBody.size()));
                             if (profileBody.size() > maximumResponseBytes)
                             {
                                 profileBody.truncate(maximumResponseBytes + 1);
                                 responseTooLarge = true;
                                 reply->abort();
                             }
                         });
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        if (!responseTooLarge)
        {
            profileBody.append(reply->readAll());
        }
        const auto networkError = reply->error();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if (responseTooLarge || profileBody.size() > maximumResponseBytes ||
            networkError != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300)
        {
            throw std::runtime_error("Unable to retrieve the Minecraft profile");
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(profileBody, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            throw std::runtime_error("Minecraft profile response is invalid");
        }
        const QJsonObject profile = document.object();
        const qint64 expiresIn =
            minecraftResponse.value(QStringLiteral("expires_in")).toVariant().toLongLong();
        AuthenticatedProfile parsedProfile =
            parseProfileResponse(profile, xboxHash, accessToken, refreshToken);
        parsedProfile.expiresAtEpochSecond =
            QDateTime::currentSecsSinceEpoch() + qMax<qint64>(expiresIn, 0);
        return parsedProfile;
    }
} // namespace strata::auth
