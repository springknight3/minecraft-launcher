#pragma once

#include "Account.h"
#include "CredentialStore.h"

#include <QMutex>
#include <QString>
#include <QVariantList>

#include <optional>

namespace strata::auth
{
    class MicrosoftAuth;

    class AccountManager
    {
    public:
        AccountManager(QString dataDirectory, CredentialStore& credentials);

        QList<Account> accounts() const;
        std::optional<Account> activeAccount() const;
        QString clientId() const;
        void setClientId(const QString& clientId);
        Account addOffline(const QString& name);
        Account addMicrosoft(const AuthenticatedProfile& profile);
        void select(const QString& accountId);
        void remove(const QString& accountId);
        LaunchProfile activeLaunchProfile(MicrosoftAuth& auth);

    private:
        QString m_accountsFile;
        CredentialStore& m_credentials;
        mutable QMutex m_mutex;
        QList<Account> m_accounts;
        QString m_activeAccountId;
        QString m_clientId;

        void load();
        void save() const;
        static void validateAccount(const Account& account);
    };
} // namespace strata::auth
