#include "M5LaunchTests.h"

#include "auth/OfflineAccountStore.h"
#include "instance/Instance.h"
#include "launch/GameLauncher.h"
#include "launch/GameProcess.h"

#include <archive.h>
#include <archive_entry.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <stdexcept>

using namespace strata;
using strata::launch::GameLauncher;

void M5LaunchTests::offlineAccountPersistsJavaCompatibleUuid()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    auth::OfflineAccountStore store(temporaryDirectory.path());

    const auth::OfflineAccount account = store.setUsername(QStringLiteral("Notch"));
    QCOMPARE(account.uuid, QStringLiteral("b50ad385-829d-3141-a216-7e7d7539ba7f"));
    QCOMPARE(store.username(), QStringLiteral("Notch"));
    QVERIFY(QFileInfo::exists(
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("offline-profile.json"))));
    QVERIFY_EXCEPTION_THROWN(store.setUsername(QStringLiteral("../invalid")),
                             std::invalid_argument);
}

void M5LaunchTests::rulesAndArgumentsRespectConditionsAndBoundaries()
{
    const QJsonObject windowsAllow{
        {QStringLiteral("rules"),
         QJsonArray{
             QJsonObject{{QStringLiteral("action"), QStringLiteral("allow")},
                         {QStringLiteral("os"),
                          QJsonObject{{QStringLiteral("name"), QStringLiteral("windows")}}}}}}};
    QVERIFY(GameLauncher::isAllowedByRules(windowsAllow, QStringLiteral("windows"),
                                           QStringLiteral("x86_64")));
    QVERIFY(!GameLauncher::isAllowedByRules(windowsAllow, QStringLiteral("linux"),
                                            QStringLiteral("x86_64")));

    const QJsonArray arguments{
        QStringLiteral("--username"), QStringLiteral("${auth_player_name}"),
        QJsonObject{{QStringLiteral("rules"),
                     QJsonArray{QJsonObject{{QStringLiteral("action"), QStringLiteral("allow")}}}},
                    {QStringLiteral("value"),
                     QJsonArray{QStringLiteral("--width"), QStringLiteral("${width}")}}}};
    const QMap<QString, QString> substitutions{
        {QStringLiteral("auth_player_name"), QStringLiteral("Player Name")},
        {QStringLiteral("width"), QStringLiteral("1280")}};
    QCOMPARE(GameLauncher::resolveArguments(arguments, substitutions, QStringLiteral("linux"),
                                            QStringLiteral("x86_64")),
             (QStringList{QStringLiteral("--username"), QStringLiteral("Player Name"),
                          QStringLiteral("--width"), QStringLiteral("1280")}));
    QCOMPARE(GameLauncher::composeJvmArguments(
                 {QStringLiteral("-XX:+UseSerialGC"), QStringLiteral("-Xmx2G")},
                 {QStringLiteral("-XX:+UseG1GC")}),
             (QStringList{QStringLiteral("-Xmx2G"), QStringLiteral("-XX:+UseG1GC")}));
}

void M5LaunchTests::launchPlanUsesOfflineSubstitutionsAndArgumentLists()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString instanceDirectory =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("instance-config"));
    const QString gameDirectory = QDir(instanceDirectory).filePath(QStringLiteral("game"));
    const QString versionId = QStringLiteral("test-version");
    const QString versionDirectory =
        QDir(gameDirectory).filePath(QStringLiteral("versions/%1").arg(versionId));
    QVERIFY(QDir().mkpath(versionDirectory));
    const QString clientJar = QDir(versionDirectory).filePath(versionId + QStringLiteral(".jar"));
    QFile client(clientJar);
    QVERIFY(client.open(QIODevice::WriteOnly));
    QVERIFY(client.write("fixture") > 0);
    client.close();

    const QJsonObject metadata{
        {QStringLiteral("mainClass"), QStringLiteral("example.Main")},
        {QStringLiteral("assetIndex"),
         QJsonObject{{QStringLiteral("id"), QStringLiteral("test-assets")}}},
        {QStringLiteral("arguments"),
         QJsonObject{
             {QStringLiteral("jvm"),
              QJsonArray{QStringLiteral("-Djava.library.path=${natives_directory}"),
                         QStringLiteral("-cp"), QStringLiteral("${classpath}")}},
             {QStringLiteral("game"),
              QJsonArray{QStringLiteral("--username"), QStringLiteral("${auth_player_name}"),
                         QStringLiteral("--uuid"), QStringLiteral("${auth_uuid}")}}}}};
    QFile versionMetadata(QDir(versionDirectory).filePath(versionId + QStringLiteral(".json")));
    QVERIFY(versionMetadata.open(QIODevice::WriteOnly));
    QVERIFY(versionMetadata.write(QJsonDocument(metadata).toJson()) > 0);
    versionMetadata.close();

    const QString assetIndexPath =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("assets/indexes/test-assets.json"));
    QVERIFY(QDir().mkpath(QFileInfo(assetIndexPath).absolutePath()));
    QFile assetIndex(assetIndexPath);
    QVERIFY(assetIndex.open(QIODevice::WriteOnly));
    QVERIFY(assetIndex.write("{}") == 2);
    assetIndex.close();

    QFile marker(QDir(gameDirectory).filePath(QStringLiteral(".stratalauncher-installed")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    QVERIFY(marker.write(versionId.toUtf8()) == versionId.size());
    marker.close();

    const Instance instance{
        QStringLiteral("id"), QStringLiteral("Test"), versionId, {}, instanceDirectory, false};
    const auth::OfflineAccount account{
        QStringLiteral("Player"), auth::OfflineAccountStore::offlineUuid(QStringLiteral("Player"))};
    const launch::LaunchPlan plan = launch::GameLauncher::buildPlan(
        instance, account, temporaryDirectory.path(), QStringLiteral("/usr/bin/java"));

    QCOMPARE(plan.program, QStringLiteral("/usr/bin/java"));
    QCOMPARE(plan.workingDirectory, gameDirectory);
    QVERIFY(plan.arguments.contains(QStringLiteral("example.Main")));
    QVERIFY(plan.arguments.contains(QStringLiteral("--username")));
    QVERIFY(plan.arguments.contains(QStringLiteral("Player")));
    QVERIFY(plan.arguments.contains(account.uuid));
    QVERIFY(plan.arguments.contains(QStringLiteral("-cp")));
    QVERIFY(plan.arguments.contains(clientJar));
    QVERIFY(!plan.arguments.contains(QStringLiteral("Player Name")));
}

void M5LaunchTests::launchPlanUsesMicrosoftTokensAndIdentity()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString instanceDirectory =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("instance-config"));
    const QString gameDirectory = QDir(instanceDirectory).filePath(QStringLiteral("game"));
    const QString versionId = QStringLiteral("microsoft-test");
    const QString versionDirectory =
        QDir(gameDirectory).filePath(QStringLiteral("versions/%1").arg(versionId));
    QVERIFY(QDir().mkpath(versionDirectory));
    QFile client(QDir(versionDirectory).filePath(versionId + QStringLiteral(".jar")));
    QVERIFY(client.open(QIODevice::WriteOnly));
    QVERIFY(client.write("fixture") > 0);
    client.close();

    const QJsonObject metadata{
        {QStringLiteral("mainClass"), QStringLiteral("example.Main")},
        {QStringLiteral("assetIndex"),
         QJsonObject{{QStringLiteral("id"), QStringLiteral("test-assets")}}},
        {QStringLiteral("arguments"),
         QJsonObject{{QStringLiteral("jvm"),
                      QJsonArray{QStringLiteral("-cp"), QStringLiteral("${classpath}")}},
                     {QStringLiteral("game"), QJsonArray{QStringLiteral("${auth_access_token}"),
                                                         QStringLiteral("${auth_xuid}"),
                                                         QStringLiteral("${user_type}")}}}}};
    QFile metadataFile(QDir(versionDirectory).filePath(versionId + QStringLiteral(".json")));
    QVERIFY(metadataFile.open(QIODevice::WriteOnly));
    QVERIFY(metadataFile.write(QJsonDocument(metadata).toJson()) > 0);
    metadataFile.close();
    const QString assetIndexPath =
        temporaryDirectory.filePath(QStringLiteral("assets/indexes/test-assets.json"));
    QVERIFY(QDir().mkpath(QFileInfo(assetIndexPath).absolutePath()));
    QFile assetIndex(assetIndexPath);
    QVERIFY(assetIndex.open(QIODevice::WriteOnly));
    QCOMPARE(assetIndex.write("{}"), qint64(2));
    assetIndex.close();
    QFile marker(QDir(gameDirectory).filePath(QStringLiteral(".stratalauncher-installed")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    QCOMPARE(marker.write(versionId.toUtf8()), qint64(versionId.size()));
    marker.close();

    const Instance instance{
        QStringLiteral("id"), QStringLiteral("Test"), versionId, {}, instanceDirectory, false};
    const launch::LaunchAccount account{QStringLiteral("Player"),
                                        QStringLiteral("0123456789abcdef0123456789abcdef"),
                                        QStringLiteral("minecraft-secret"),
                                        QStringLiteral("xuid-123"), auth::Account::Type::Microsoft};
    const auto plan = launch::GameLauncher::buildPlan(instance, account, temporaryDirectory.path(),
                                                      QStringLiteral("/usr/bin/java"));
    QVERIFY(plan.arguments.contains(QStringLiteral("minecraft-secret")));
    QVERIFY(plan.arguments.contains(QStringLiteral("xuid-123")));
    QVERIFY(plan.arguments.contains(QStringLiteral("msa")));
}

void M5LaunchTests::nativeExtractionRejectsTraversal()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString archivePath =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("unsafe.zip"));
    std::unique_ptr<struct archive, decltype(&archive_write_free)> writer(archive_write_new(),
                                                                          &archive_write_free);
    QVERIFY(writer != nullptr);
    QVERIFY(archive_write_set_format_zip(writer.get()) == ARCHIVE_OK);
    const QByteArray encodedPath = QFile::encodeName(archivePath);
    QVERIFY(archive_write_open_filename(writer.get(), encodedPath.constData()) == ARCHIVE_OK);
    std::unique_ptr<struct archive_entry, decltype(&archive_entry_free)> entry(archive_entry_new(),
                                                                               &archive_entry_free);
    QVERIFY(entry != nullptr);
    archive_entry_set_pathname(entry.get(), "../escaped.txt");
    archive_entry_set_filetype(entry.get(), AE_IFREG);
    archive_entry_set_perm(entry.get(), 0644);
    archive_entry_set_size(entry.get(), 4);
    QVERIFY(archive_write_header(writer.get(), entry.get()) == ARCHIVE_OK);
    QCOMPARE(archive_write_data(writer.get(), "oops", 4), la_ssize_t(4));
    QCOMPARE(archive_write_close(writer.get()), ARCHIVE_OK);

    QVERIFY_EXCEPTION_THROWN(
        launch::GameLauncher::extractNativeArchives(
            {archivePath}, QDir(temporaryDirectory.path()).filePath(QStringLiteral("natives"))),
        std::runtime_error);
    QVERIFY(!QFileInfo::exists(
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("escaped.txt"))));

    const QString safeArchivePath =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("safe.zip"));
    writer.reset(archive_write_new());
    QVERIFY(writer != nullptr);
    QVERIFY(archive_write_set_format_zip(writer.get()) == ARCHIVE_OK);
    const QByteArray safeEncodedPath = QFile::encodeName(safeArchivePath);
    QVERIFY(archive_write_open_filename(writer.get(), safeEncodedPath.constData()) == ARCHIVE_OK);
    entry.reset(archive_entry_new());
    QVERIFY(entry != nullptr);
    archive_entry_set_pathname(entry.get(), "META-INF/ignored.txt");
    archive_entry_set_filetype(entry.get(), AE_IFREG);
    archive_entry_set_perm(entry.get(), 0644);
    archive_entry_set_size(entry.get(), 7);
    QVERIFY(archive_write_header(writer.get(), entry.get()) == ARCHIVE_OK);
    QCOMPARE(archive_write_data(writer.get(), "ignored", 7), la_ssize_t(7));
    entry.reset(archive_entry_new());
    QVERIFY(entry != nullptr);
    archive_entry_set_pathname(entry.get(), "native/data.txt");
    archive_entry_set_filetype(entry.get(), AE_IFREG);
    archive_entry_set_perm(entry.get(), 0644);
    archive_entry_set_size(entry.get(), 6);
    QVERIFY(archive_write_header(writer.get(), entry.get()) == ARCHIVE_OK);
    QCOMPARE(archive_write_data(writer.get(), "native", 6), la_ssize_t(6));
    QCOMPARE(archive_write_close(writer.get()), ARCHIVE_OK);

    const QString destination =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("safe-natives"));
    launch::GameLauncher::extractNativeArchives({safeArchivePath}, destination);
    QFile extracted(QDir(destination).filePath(QStringLiteral("native/data.txt")));
    QVERIFY(extracted.open(QIODevice::ReadOnly));
    QCOMPARE(extracted.readAll(), QByteArrayLiteral("native"));
    QVERIFY(!QFileInfo::exists(QDir(destination).filePath(QStringLiteral("META-INF/ignored.txt"))));
}

void M5LaunchTests::processTracksReadyExitCrashAndStop()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString fixture = QString::fromUtf8(STRATA_PROCESS_FIXTURE);
    const auto runFixture = [&temporaryDirectory, &fixture](const QString& mode)
    {
        launch::LaunchPlan plan{
            fixture,
            {mode},
            temporaryDirectory.path(),
            QDir(temporaryDirectory.path()).filePath(mode + QStringLiteral(".log"))};
        return new launch::GameProcess(mode, plan);
    };

    std::unique_ptr<launch::GameProcess> normal(runFixture(QStringLiteral("normal")));
    QSignalSpy normalStatus(normal.get(), &launch::GameProcess::statusChanged);
    QSignalSpy normalLog(normal.get(), &launch::GameProcess::logOutputReceived);
    QSignalSpy normalFinished(normal.get(), &launch::GameProcess::finished);
    normal->start();
    QTRY_COMPARE_WITH_TIMEOUT(normal->status(), QStringLiteral("ready"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(normalFinished.count(), 1, 3000);
    QCOMPARE(normal->status(), QStringLiteral("stopped"));
    QVERIFY(normalStatus.count() >= 2);
    QVERIFY(normalLog.count() > 0);
    QVERIFY(normalLog.first().at(1).toByteArray().contains("Sound engine started"));

    std::unique_ptr<launch::GameProcess> crash(runFixture(QStringLiteral("crash")));
    QSignalSpy crashFinished(crash.get(), &launch::GameProcess::finished);
    crash->start();
    QTRY_COMPARE_WITH_TIMEOUT(crashFinished.count(), 1, 3000);
    QCOMPARE(crash->status(), QStringLiteral("crashed"));
    QVERIFY(crashFinished.takeFirst().at(2).toBool());

    std::unique_ptr<launch::GameProcess> stopped(runFixture(QStringLiteral("wait")));
    QSignalSpy stopFinished(stopped.get(), &launch::GameProcess::finished);
    stopped->start();
    QTRY_VERIFY_WITH_TIMEOUT(stopped->isRunning(), 3000);
    stopped->stop();
    QTRY_COMPARE_WITH_TIMEOUT(stopFinished.count(), 1, 3000);
    QCOMPARE(stopped->status(), QStringLiteral("stopped"));
}

QTEST_GUILESS_MAIN(M5LaunchTests)
