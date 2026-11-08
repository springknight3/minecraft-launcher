#include "AccountManager.h"

#include "MicrosoftAuth.h"
#include "OfflineAccountStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

#include <QDateTime>

#include <algorithm>
#include <stdexcept>

namespace strata::auth
{
    namespace
    {
        QString typeName(Account::Type type)
        {
            return type == Account::Type::Microsoft ? QStringLiteral("MICROSOFT")
                                                    : QStringLiteral("OFFLINE");
        }

        Account::Type parseType(const QString& type)
        {
            if (type == QStringLiteral("MICROSOFT"))
            {
                return Account::Type::Microsoft;
            }
            if (type == QStringLiteral("OFFLINE"))
            {
                return Account::Type::Offline;
            }
            throw std::runtime_error("Account data contains an unknown account type");
        }
    } // namespace

    AccountManager::AccountManager(QString dataDirectory, CredentialStore& credentials)
        : m_accountsFile(QDir(QFileInfo(dataDirectory).absoluteFilePath())
                             .filePath(QStringLiteral("accounts.json"))),
          m_credentials(credentials)
    {
        load();
    }

    QList<Account> AccountManager::accounts() const
    {
        QMutexLocker lock(&m_mutex);
        return m_accounts;
    }

    std::optional<Account> AccountManager::activeAccount() const
    {
        QMutexLocker lock(&m_mutex);
        for (const Account& account : m_accounts)
        {
            if (account.id == m_activeAccountId)
            {
                return account;
            }
        }
        return std::nullopt;
    }

    QString AccountManager::clientId() const
    {
        QMutexLocker lock(&m_mutex);
        return m_clientId;
    }

    void AccountManager::setClientId(const QString& clientId)
    {
        const QString normalized = clientId.trimmed();
        if (!normalized.isEmpty() &&
            !QRegularExpression(
                 QStringLiteral("^[0-9a-fA-F]{8}-(?:[0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}$"))
                 .match(normalized)
                 .hasMatch())
        {
            throw std::invalid_argument("Microsoft client ID must be a valid UUID");
        }
        QMutexLocker lock(&m_mutex);
        const QString previous = m_clientId;
        m_clientId = normalized;
        try
        {
            save();
        }
        catch (...)
        {
            m_clientId = previous;
            throw;
        }
    }

    Account AccountManager::addOffline(const QString& name)
    {
        const QString normalized = OfflineAccountStore::validateUsername(name);
        QMutexLocker lock(&m_mutex);
        const Account account{QUuid::createUuid().toString(QUuid::WithoutBraces),
                              normalized,
                              OfflineAccountStore::offlineUuid(normalized),
                              Account::Type::Offline,
                              {}};
        const QString previousActiveId = m_activeAccountId;
        m_accounts.append(account);
        if (m_activeAccountId.isEmpty())
        {
            m_activeAccountId = account.id;
        }
        try
        {
            save();
        }
        catch (...)
        {
            m_accounts.removeLast();
            m_activeAccountId = previousActiveId;
            throw;
        }
        return account;
    }

    Account AccountManager::addMicrosoft(const AuthenticatedProfile& profile)
    {
        validateAccount({QUuid::createUuid().toString(QUuid::WithoutBraces), profile.name,
                         profile.uuid, Account::Type::Microsoft, profile.xuid});
        QMutexLocker lock(&m_mutex);
        auto existing =
            std::find_if(m_accounts.begin(), m_accounts.end(),
                         [&profile](const Account& account)
                         {
                             return account.type == Account::Type::Microsoft &&
                                    account.uuid.compare(profile.uuid, Qt::CaseInsensitive) == 0;
                         });
        const bool wasExisting = existing != m_accounts.end();
        const Account previous = wasExisting ? *existing : Account{};
        const QString id =
            wasExisting ? existing->id : QUuid::createUuid().toString(QUuid::WithoutBraces);
        const Account account{id, profile.name, profile.uuid.toLower(), Account::Type::Microsoft,
                              profile.xuid};
        const QJsonObject tokens{{QStringLiteral("accessToken"), profile.accessToken},
                                 {QStringLiteral("refreshToken"), profile.refreshToken},
                                 {QStringLiteral("expiresAt"), profile.expiresAtEpochSecond}};
        const QString previousCredentials = m_credentials.get(id);
        m_credentials.put(id,
                          QString::fromUtf8(QJsonDocument(tokens).toJson(QJsonDocument::Compact)));
        const QString oldActiveId = m_activeAccountId;
        if (wasExisting)
        {
            *existing = account;
        }
        else
        {
            m_accounts.append(account);
        }
        m_activeAccountId = id;
        try
        {
            save();
        }
        catch (...)
        {
            if (wasExisting)
            {
                *existing = previous;
            }
            else
            {
                m_accounts.removeLast();
            }
            m_activeAccountId = oldActiveId;
            try
            {
                if (previousCredentials.isEmpty())
                {
                    m_credentials.remove(id);
                }
                else
                {
                    m_credentials.put(id, previousCredentials);
                }
            }
            catch (...)
            {
            }
            throw;
        }
        return account;
    }

    void AccountManager::select(const QString& accountId)
    {
        QMutexLocker lock(&m_mutex);
        if (std::none_of(m_accounts.cbegin(), m_accounts.cend(),
                         [&accountId](const Account& account) { return account.id == accountId; }))
        {
            throw std::invalid_argument("Account does not exist");
        }
        const QString previous = m_activeAccountId;
        m_activeAccountId = accountId;
        try
        {
            save();
        }
        catch (...)
        {
            m_activeAccountId = previous;
            throw;
        }
    }

    void AccountManager::remove(const QString& accountId)
    {
        QMutexLocker lock(&m_mutex);
        const auto iterator =
            std::find_if(m_accounts.begin(), m_accounts.end(),
                         [&accountId](const Account& account) { return account.id == accountId; });
        if (iterator == m_accounts.end())
        {
            throw std::invalid_argument("Account does not exist");
        }
        const Account removed = *iterator;
        const qsizetype index = std::distance(m_accounts.begin(), iterator);
        const QString previousActiveId = m_activeAccountId;
        m_accounts.erase(iterator);
        if (m_activeAccountId == accountId)
        {
            m_activeAccountId = m_accounts.isEmpty() ? QString() : m_accounts.first().id;
        }
        try
        {
            save();
        }
        catch (...)
        {
            m_accounts.insert(index, removed);
            m_activeAccountId = previousActiveId;
            throw;
        }
        if (removed.type == Account::Type::Microsoft)
        {
            m_credentials.remove(removed.id);
        }
    }

    LaunchProfile AccountManager::activeLaunchProfile(MicrosoftAuth& auth)
    {
        Account account;
        QString configuredClientId;
        {
            QMutexLocker lock(&m_mutex);
            const auto iterator = std::find_if(m_accounts.cbegin(), m_accounts.cend(),
                                               [this](const Account& candidate)
                                               { return candidate.id == m_activeAccountId; });
            if (iterator == m_accounts.cend())
            {
                throw std::runtime_error("Add or select an account before launching Minecraft");
            }
            account = *iterator;
            configuredClientId = m_clientId;
        }
        if (account.type == Account::Type::Offline)
        {
            return {account, QStringLiteral("0")};
        }
        const QJsonDocument stored =
            QJsonDocument::fromJson(m_credentials.get(account.id).toUtf8());
        if (!stored.isObject())
        {
            throw std::runtime_error(
                "Cached Microsoft credentials are invalid. Sign in to this account again.");
        }
        const QJsonObject tokens = stored.object();
        const QString refreshToken = tokens.value(QStringLiteral("refreshToken")).toString();
        if (refreshToken.isEmpty())
        {
            throw std::runtime_error(
                "Cached Microsoft credentials are missing. Sign in to this account again.");
        }
        const qint64 expiresAt = tokens.value(QStringLiteral("expiresAt")).toVariant().toLongLong();
        const QString accessToken = tokens.value(QStringLiteral("accessToken")).toString();
        if (!accessToken.isEmpty() && expiresAt > QDateTime::currentSecsSinceEpoch() + 120)
        {
            return {account, accessToken};
        }
        const AuthenticatedProfile refreshed = auth.refresh(configuredClientId, refreshToken);
        if (account.uuid.compare(refreshed.uuid, Qt::CaseInsensitive) != 0)
        {
            throw std::runtime_error(
                "Microsoft authentication returned a different account. Sign in again.");
        }
        const Account updated{account.id, refreshed.name, account.uuid, Account::Type::Microsoft,
                              refreshed.xuid};
        const QJsonObject updatedTokens{
            {QStringLiteral("accessToken"), refreshed.accessToken},
            {QStringLiteral("refreshToken"), refreshed.refreshToken},
            {QStringLiteral("expiresAt"), refreshed.expiresAtEpochSecond}};
        m_credentials.put(
            account.id,
            QString::fromUtf8(QJsonDocument(updatedTokens).toJson(QJsonDocument::Compact)));
        QMutexLocker lock(&m_mutex);
        const auto iterator =
            std::find_if(m_accounts.begin(), m_accounts.end(), [&account](const Account& candidate)
                         { return candidate.id == account.id; });
        if (iterator == m_accounts.end())
        {
            throw std::runtime_error("The Microsoft account was removed while refreshing");
        }
        const Account previous = *iterator;
        *iterator = updated;
        try
        {
            save();
        }
        catch (...)
        {
            *iterator = previous;
            throw;
        }
        return {updated, refreshed.accessToken};
    }

    void AccountManager::load()
    {
        QFile file(m_accountsFile);
        if (!file.exists())
        {
            return;
        }
        if (!file.open(QIODevice::ReadOnly))
        {
            throw std::runtime_error(QStringLiteral("Unable to read account data: %1")
                                         .arg(file.errorString())
                                         .toStdString());
        }
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
        {
            throw std::runtime_error("Account data is invalid");
        }
        const QJsonObject root = document.object();
        m_clientId = root.value(QStringLiteral("clientId")).toString();
        m_activeAccountId = root.value(QStringLiteral("activeAccountId")).toString();
        const QJsonArray accounts = root.value(QStringLiteral("accounts")).toArray();
        for (const QJsonValue& value : accounts)
        {
            const QJsonObject object = value.toObject();
            Account account{object.value(QStringLiteral("id")).toString(),
                            object.value(QStringLiteral("name")).toString(),
                            object.value(QStringLiteral("uuid")).toString(),
                            parseType(object.value(QStringLiteral("type")).toString()),
                            object.value(QStringLiteral("xuid")).toString()};
            validateAccount(account);
            account.uuid = account.type == Account::Type::Offline
                               ? OfflineAccountStore::offlineUuid(account.name)
                               : account.uuid.remove(QLatin1Char('-')).toLower();
            if (std::any_of(m_accounts.cbegin(), m_accounts.cend(),
                            [&account](const Account& existing)
                            { return existing.id == account.id; }))
            {
                throw std::runtime_error("Account data contains a duplicate account identifier");
            }
            m_accounts.append(account);
        }
        if (std::none_of(m_accounts.cbegin(), m_accounts.cend(), [this](const Account& account)
                         { return account.id == m_activeAccountId; }))
        {
            m_activeAccountId = m_accounts.isEmpty() ? QString() : m_accounts.first().id;
        }
    }

    void AccountManager::save() const
    {
        if (!QDir().mkpath(QFileInfo(m_accountsFile).absolutePath()))
        {
            throw std::runtime_error("Unable to create account data directory");
        }
        QJsonArray accounts;
        for (const Account& account : m_accounts)
        {
            accounts.append(QJsonObject{{QStringLiteral("id"), account.id},
                                        {QStringLiteral("name"), account.name},
                                        {QStringLiteral("uuid"), account.uuid},
                                        {QStringLiteral("type"), typeName(account.type)},
                                        {QStringLiteral("xuid"), account.xuid}});
        }
        const QJsonObject root{{QStringLiteral("accounts"), accounts},
                               {QStringLiteral("activeAccountId"), m_activeAccountId},
                               {QStringLiteral("clientId"), m_clientId}};
        QSaveFile file(m_accountsFile);
        if (!file.open(QIODevice::WriteOnly))
        {
            throw std::runtime_error(QStringLiteral("Unable to save account data: %1")
                                         .arg(file.errorString())
                                         .toStdString());
        }
        const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
        if (file.write(json) != json.size() || !file.commit())
        {
            throw std::runtime_error(QStringLiteral("Unable to commit account data: %1")
                                         .arg(file.errorString())
                                         .toStdString());
        }
    }

    void AccountManager::validateAccount(const Account& account)
    {
        if (account.id.isEmpty() || account.name.isEmpty() || account.uuid.isEmpty())
        {
            throw std::runtime_error("Account data contains an incomplete record");
        }
        if (account.type == Account::Type::Offline)
        {
            QString expectedUuid = OfflineAccountStore::offlineUuid(account.name);
            QString actualUuid = account.uuid;
            expectedUuid.remove(QLatin1Char('-'));
            actualUuid.remove(QLatin1Char('-'));
            if (expectedUuid.compare(actualUuid, Qt::CaseInsensitive) != 0)
            {
                throw std::runtime_error("Offline account identifier does not match its name");
            }
            return;
        }
        if (!QRegularExpression(QStringLiteral("(?i)^[0-9a-f]{32}$"))
                 .match(account.uuid)
                 .hasMatch())
        {
            throw std::runtime_error("Microsoft account has an invalid profile identifier");
        }
    }
} // namespace strata::auth
