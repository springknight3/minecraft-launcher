#pragma once

#include <QString>

namespace strata::auth
{
    struct Account
    {
        enum class Type
        {
            Offline,
            Microsoft
        };

        QString id;
        QString name;
        QString uuid;
        Type type = Type::Offline;
        QString xuid;
    };

    struct AuthenticatedProfile
    {
        QString name;
        QString uuid;
        QString xuid;
        QString accessToken;
        QString refreshToken;
        qint64 expiresAtEpochSecond = 0;
    };

    struct LaunchProfile
    {
        Account account;
        QString accessToken;
    };
} // namespace strata::auth
