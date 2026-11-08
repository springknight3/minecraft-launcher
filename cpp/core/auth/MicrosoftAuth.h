#pragma once

#include "Account.h"

#include <QJsonObject>
#include <QObject>
#include <QUrl>

#include <atomic>

namespace strata::auth
{
    struct DeviceCode
    {
        QString deviceCode;
        QString userCode;
        QUrl verificationUri;
        QString message;
        int expiresIn = 900;
        int interval = 5;
    };

    class MicrosoftAuth : public QObject
    {
        Q_OBJECT
    public:
        explicit MicrosoftAuth(QObject* parent = nullptr);

        AuthenticatedProfile signIn(const QString& clientId);
        AuthenticatedProfile refresh(const QString& clientId, const QString& refreshToken);
        void cancel();
        static DeviceCode parseDeviceCodeResponse(const QJsonObject& response);
        static AuthenticatedProfile parseProfileResponse(const QJsonObject& response,
                                                         const QString& xuid,
                                                         const QString& accessToken,
                                                         const QString& refreshToken);

    signals:
        void userCodeReady(const QString& userCode, const QUrl& verificationUri,
                           const QString& message);

    private:
        std::atomic_bool m_cancelled = false;
        AuthenticatedProfile authenticateMinecraft(const QString& microsoftAccessToken,
                                                   const QString& refreshToken);
    };
} // namespace strata::auth
