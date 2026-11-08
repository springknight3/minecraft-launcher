#include "M7AuthTests.h"

#include "auth/AccountManager.h"
#include "auth/MicrosoftAuth.h"
#include "auth/OfflineAccountStore.h"

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <QHash>
#include <stdexcept>

namespace
{
    class MemoryCredentialStore final : public strata::auth::CredentialStore
    {
    public:
        void put(const QString& accountId, const QString& secret) override
        {
            values.insert(accountId, secret);
        }

        QString get(const QString& accountId) const override
        {
            return values.value(accountId);
        }

        void remove(const QString& accountId) override
        {
            values.remove(accountId);
        }

        QHash<QString, QString> values;
    };
} // namespace

void M7AuthTests::persistsOfflineAccountsAndActiveSelection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    MemoryCredentialStore credentials;
    const QString firstId = [&]
    {
        strata::auth::AccountManager manager(directory.path(), credentials);
        const auto first = manager.addOffline(QStringLiteral("First"));
        const auto second = manager.addOffline(QStringLiteral("Second"));
        manager.select(second.id);
        return first.id;
    }();

    strata::auth::AccountManager reopened(directory.path(), credentials);
    QCOMPARE(reopened.accounts().size(), 2);
    QVERIFY(reopened.activeAccount().has_value());
    QCOMPARE(reopened.activeAccount()->name, QStringLiteral("Second"));
    QCOMPARE(reopened.accounts().first().id, firstId);
    QCOMPARE(reopened.activeAccount()->uuid,
             strata::auth::OfflineAccountStore::offlineUuid(QStringLiteral("Second")));
}

void M7AuthTests::storesMicrosoftCredentialsOutsideAccountMetadata()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    MemoryCredentialStore credentials;
    strata::auth::AccountManager manager(directory.path(), credentials);
    const auto account = manager.addMicrosoft(
        {QStringLiteral("Player"), QStringLiteral("0123456789abcdef0123456789abcdef"),
         QStringLiteral("xuid"), QStringLiteral("minecraft-access-secret"),
         QStringLiteral("refresh-secret"), QDateTime::currentSecsSinceEpoch() + 3600});

    QFile metadata(directory.filePath(QStringLiteral("accounts.json")));
    QVERIFY(metadata.open(QIODevice::ReadOnly));
    const QByteArray contents = metadata.readAll();
    QVERIFY(!contents.contains("minecraft-access-secret"));
    QVERIFY(!contents.contains("refresh-secret"));
    QVERIFY(credentials.values.value(account.id).contains(QStringLiteral("refresh-secret")));
    QCOMPARE(manager.activeAccount()->id, account.id);
    strata::auth::MicrosoftAuth auth;
    const auto launchProfile = manager.activeLaunchProfile(auth);
    QCOMPARE(launchProfile.account.id, account.id);
    QCOMPARE(launchProfile.accessToken, QStringLiteral("minecraft-access-secret"));
    manager.remove(account.id);
    QVERIFY(!credentials.values.contains(account.id));
}

void M7AuthTests::rejectsInvalidAccountsAndClientIds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    MemoryCredentialStore credentials;
    strata::auth::AccountManager manager(directory.path(), credentials);
    QVERIFY_EXCEPTION_THROWN(manager.addOffline(QStringLiteral("../invalid")),
                             std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN(manager.setClientId(QStringLiteral("not-a-client-id")),
                             std::invalid_argument);
    QVERIFY(manager.accounts().isEmpty());
}

void M7AuthTests::parsesMicrosoftDeviceCodeAndProfileResponses()
{
    const auto deviceCode = strata::auth::MicrosoftAuth::parseDeviceCodeResponse(
        {{QStringLiteral("device_code"), QStringLiteral("secret")},
         {QStringLiteral("user_code"), QStringLiteral("ABCD-EFGH")},
         {QStringLiteral("verification_uri"), QStringLiteral("https://microsoft.com/devicelogin")},
         {QStringLiteral("expires_in"), 900},
         {QStringLiteral("interval"), 5}});
    QCOMPARE(deviceCode.userCode, QStringLiteral("ABCD-EFGH"));
    QCOMPARE(deviceCode.verificationUri.toString(),
             QStringLiteral("https://microsoft.com/devicelogin"));
    QVERIFY_EXCEPTION_THROWN(
        strata::auth::MicrosoftAuth::parseDeviceCodeResponse(
            {{QStringLiteral("device_code"), QStringLiteral("secret")},
             {QStringLiteral("user_code"), QStringLiteral("ABCD-EFGH")},
             {QStringLiteral("verification_uri"), QStringLiteral("http://example.com")}}),
        std::runtime_error);

    const auto profile = strata::auth::MicrosoftAuth::parseProfileResponse(
        {{QStringLiteral("id"), QStringLiteral("01234567-89ab-cdef-0123-456789abcdef")},
         {QStringLiteral("name"), QStringLiteral("Player")}},
        QStringLiteral("xuid"), QStringLiteral("access"), QStringLiteral("refresh"));
    QCOMPARE(profile.uuid, QStringLiteral("0123456789abcdef0123456789abcdef"));
    QVERIFY_EXCEPTION_THROWN(strata::auth::MicrosoftAuth::parseProfileResponse(
                                 {{QStringLiteral("id"), QStringLiteral("not-a-uuid")},
                                  {QStringLiteral("name"), QStringLiteral("Player")}},
                                 {}, {}, {}),
                             std::runtime_error);
}

QTEST_GUILESS_MAIN(M7AuthTests)
