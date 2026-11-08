#include "server/DevServerManager.h"
#include "server/ServerProcess.h"
#include "settings/InstanceSettings.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <stdexcept>

using namespace strata::server;

class M11ServerTests : public QObject
{
    Q_OBJECT

private slots:
    void createsIsolatedServerAndPersistsSettings();
    void preservesOtherServerProperties();
    void validatesNamesFlagsAndProperties();
    void deletesServerData();
    void persistsEulaAcceptanceImmediately();
    void managesServerOwnedJavaRuntimesSafely();
    void parsesStablePaperBuildsSafely();
    void enforcesLaunchPrerequisites();
    void serverProcessCapturesConsoleAndStops();
    void serverProcessReportsCrashesAndRejectsBadCommands();
};

void M11ServerTests::createsIsolatedServerAndPersistsSettings()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    strata::download::DownloadManager downloads;
    strata::mojang::MojangManifestClient manifest;
    DevServerManager manager(temporaryDirectory.path(), downloads, manifest);

    const ServerConfig created = manager.create(
        QStringLiteral("Local test"), QStringLiteral("1.21.1"), ServerDistribution::Fabric);
    const QString serverDirectory = manager.serverDirectoryPath(created);

    QVERIFY(QFileInfo(serverDirectory).isDir());
    QVERIFY(!created.installed);
    QVERIFY(!created.eulaAccepted);
    QFile eula(QDir(serverDirectory).filePath(QStringLiteral("eula.txt")));
    QVERIFY(eula.open(QIODevice::ReadOnly));
    QVERIFY(eula.readAll().contains("eula=false"));

    const ServerConfig updated =
        manager.updateSettings(created, QStringLiteral(" Fabric dev "), true,
                               {QStringLiteral("-Xmx4G"), QStringLiteral("-XX:+UseG1GC")});
    const QVector<ServerConfig> reloaded = manager.listServers();

    QCOMPARE(updated.name, QStringLiteral("Fabric dev"));
    QCOMPARE(reloaded.size(), 1);
    QCOMPARE(reloaded.constFirst().name, QStringLiteral("Fabric dev"));
    QVERIFY(reloaded.constFirst().eulaAccepted);
    QCOMPARE(reloaded.constFirst().javaFlags,
             QStringList({QStringLiteral("-Xmx4G"), QStringLiteral("-XX:+UseG1GC")}));
    eula.close();
    QVERIFY(eula.open(QIODevice::ReadOnly));
    QCOMPARE(eula.readAll(), QByteArray("eula=true\n"));
}

void M11ServerTests::preservesOtherServerProperties()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    strata::download::DownloadManager downloads;
    strata::mojang::MojangManifestClient manifest;
    DevServerManager manager(temporaryDirectory.path(), downloads, manifest);
    const ServerConfig created = manager.create(
        QStringLiteral("Properties"), QStringLiteral("1.21.1"), ServerDistribution::Vanilla);

    const QString edited =
        QStringLiteral("# local settings\nview-distance=14\nonline-mode=false\nmotd=New server\n");
    const ServerConfig saved = manager.saveServerProperties(created, edited);

    QCOMPARE(manager.readServerProperties(saved), edited);
    QCOMPARE(saved.viewDistance, 14);
    QVERIFY(!saved.onlineMode);
    QCOMPARE(manager.listServers().constFirst().viewDistance, 14);
    QVERIFY(!manager.listServers().constFirst().onlineMode);
}

void M11ServerTests::validatesNamesFlagsAndProperties()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    strata::download::DownloadManager downloads;
    strata::mojang::MojangManifestClient manifest;
    DevServerManager manager(temporaryDirectory.path(), downloads, manifest);

    QVERIFY_EXCEPTION_THROWN(manager.create(QStringLiteral("../escape"), QStringLiteral("1.21.1"),
                                            ServerDistribution::Vanilla),
                             std::invalid_argument);
    const ServerConfig created = manager.create(QStringLiteral("Valid"), QStringLiteral("1.21.1"),
                                                ServerDistribution::Paper);
    QVERIFY_EXCEPTION_THROWN(manager.updateSettings(created, QStringLiteral("Valid"), false,
                                                    {QStringLiteral("not-a-jvm-flag")}),
                             std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN(manager.saveServerProperties(
                                 created, QStringLiteral("view-distance=99\nonline-mode=true\n")),
                             std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN(manager.saveServerProperties(
                                 created, QStringLiteral("view-distance=10\nview-distance=12\n")),
                             std::invalid_argument);

    QFile oversizedProperties(
        QDir(manager.serverDirectoryPath(created)).filePath(QStringLiteral("server.properties")));
    QVERIFY(oversizedProperties.open(QIODevice::WriteOnly));
    QCOMPARE(oversizedProperties.write(QByteArray(1'000'001, 'x')), qint64(1'000'001));
    oversizedProperties.close();
    QVERIFY_EXCEPTION_THROWN(manager.readServerProperties(created), std::runtime_error);
}

void M11ServerTests::deletesServerData()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    strata::download::DownloadManager downloads;
    strata::mojang::MojangManifestClient manifest;
    DevServerManager manager(temporaryDirectory.path(), downloads, manifest);
    const ServerConfig created = manager.create(
        QStringLiteral("Delete me"), QStringLiteral("1.21.1"), ServerDistribution::Vanilla);
    const QString root =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("servers/%1").arg(created.id));

    manager.deleteServer(created);

    QVERIFY(!QFileInfo::exists(root));
    QVERIFY(manager.listServers().isEmpty());
    QVERIFY_EXCEPTION_THROWN(manager.deleteServer(created), std::runtime_error);
}

void M11ServerTests::persistsEulaAcceptanceImmediately()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    strata::download::DownloadManager downloads;
    strata::mojang::MojangManifestClient manifest;
    DevServerManager manager(temporaryDirectory.path(), downloads, manifest);
    const ServerConfig created = manager.create(QStringLiteral("EULA"), QStringLiteral("1.21.1"),
                                                ServerDistribution::Vanilla);

    const ServerConfig accepted = manager.setEulaAccepted(created, true);

    QVERIFY(accepted.eulaAccepted);
    QVERIFY(manager.listServers().constFirst().eulaAccepted);
    QFile eula(QDir(manager.serverDirectoryPath(created)).filePath(QStringLiteral("eula.txt")));
    QVERIFY(eula.open(QIODevice::ReadOnly));
    QCOMPARE(eula.readAll(), QByteArray("eula=true\n"));
}

void M11ServerTests::managesServerOwnedJavaRuntimesSafely()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString dataDirectory = QDir(temporaryDirectory.path()).filePath(QStringLiteral("data"));
    strata::download::DownloadManager downloads;
    strata::mojang::MojangManifestClient manifest;
    DevServerManager servers(dataDirectory, downloads, manifest);
    const ServerConfig server = servers.create(
        QStringLiteral("Runtime owner"), QStringLiteral("1.21.1"), ServerDistribution::Vanilla);
    const QString ownerDirectory = servers.serverDirectoryPath(server);
    const QString component = QStringLiteral("temurin-java-21");

    QCOMPARE(strata::runtime::JavaRuntimeManager::runtimeDirectory(ownerDirectory, dataDirectory,
                                                                   component, false),
             QDir(dataDirectory).filePath(QStringLiteral("java-runtimes/%1").arg(component)));
    const QString redundantRuntime =
        QDir(ownerDirectory).filePath(QStringLiteral("java-runtimes/%1").arg(component));
    QCOMPARE(strata::runtime::JavaRuntimeManager::runtimeDirectory(ownerDirectory, dataDirectory,
                                                                   component, true),
             redundantRuntime);
    QVERIFY_EXCEPTION_THROWN(
        strata::runtime::JavaRuntimeManager::runtimeDirectory(
            QDir(temporaryDirectory.path()).filePath(QStringLiteral("outside")), dataDirectory,
            component, true),
        std::runtime_error);

    QVERIFY(QDir().mkpath(redundantRuntime));
    const QString executableRelative =
#ifdef Q_OS_WIN
        QStringLiteral("bin/java.exe");
#else
        QStringLiteral("bin/java");
#endif
    const QString executable = QDir(redundantRuntime).filePath(executableRelative);
    QVERIFY(QDir().mkpath(QFileInfo(executable).absolutePath()));
    QFile java(executable);
    QVERIFY(java.open(QIODevice::WriteOnly));
    QCOMPARE(java.write("fake server java"), qint64(16));
    java.close();
#ifndef Q_OS_WIN
    QVERIFY(java.setPermissions(java.permissions() | QFileDevice::ExeOwner));
#endif
    const QString checksum = QString::fromLatin1(
        QCryptographicHash::hash(QByteArrayLiteral("fake server java"), QCryptographicHash::Sha1)
            .toHex());
    QFile marker(QDir(redundantRuntime).filePath(QStringLiteral(".stratalauncher-java-path")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    const QByteArray markerContents =
        executableRelative.toUtf8() + '\n' + checksum.toLatin1() + '\n';
    QCOMPARE(marker.write(markerContents), markerContents.size());
    marker.close();
    QFile version(
        QDir(redundantRuntime).filePath(QStringLiteral(".stratalauncher-runtime-version")));
    QVERIFY(version.open(QIODevice::WriteOnly));
    QCOMPARE(version.write("21.0.7"), qint64(6));
    version.close();

    strata::runtime::JavaRuntimeManager runtimes(downloads, dataDirectory);
    strata::InstanceSettings settings(ownerDirectory);
    settings.setJavaRuntimeMode(strata::InstanceSettings::JavaRuntimeMode::Managed);
    settings.setJavaRuntimeComponent(component);
    settings.setJavaRuntimeRedundant(true);
    QCOMPARE(runtimes.javaExecutable(ownerDirectory, server.minecraftVersion, component, true),
             executable);

    const QList<strata::runtime::RuntimeOwner> owners{
        {QStringLiteral("Server: Runtime owner"), ownerDirectory, component, true, true}};
    const auto installed = runtimes.listInstalledRuntimes(owners);
    QCOMPARE(installed.size(), 1);
    QCOMPARE(installed.constFirst().users, QStringList{QStringLiteral("Server: Runtime owner")});
    QVERIFY_EXCEPTION_THROWN(runtimes.deleteRuntime(installed.constFirst(), owners),
                             std::runtime_error);
    auto unusedOwners = owners;
    unusedOwners[0].managed = false;
    runtimes.deleteRuntime(installed.constFirst(), unusedOwners);
    QVERIFY(!QFileInfo::exists(redundantRuntime));
}

void M11ServerTests::parsesStablePaperBuildsSafely()
{
    const QJsonArray builds = QJsonDocument::fromJson(R"json([
        {"id":12,"channel":"EXPERIMENTAL","downloads":{"server:default":{
            "name":"paper-test.jar","url":"https://fill-data.papermc.io/test",
            "checksums":{"sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}}},
        {"id":8,"channel":"STABLE","downloads":{"server:default":{
            "name":"paper-test-8.jar","url":"https://fill-data.papermc.io/build-8",
            "checksums":{"sha256":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"}}}},
        {"id":10,"channel":"STABLE","downloads":{"server:default":{
            "name":"paper-test-10.jar","url":"https://fill-data.papermc.io/build-10",
            "checksums":{"sha256":"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"}}}}
    ])json")
                                  .array();

    const PaperBuild selected = DevServerManager::latestStablePaperBuild(builds);

    QCOMPARE(selected.id, 10);
    QCOMPARE(selected.filename, QStringLiteral("paper-test-10.jar"));
    QCOMPARE(selected.url.toString(), QStringLiteral("https://fill-data.papermc.io/build-10"));
    QCOMPARE(selected.sha256, QString(64, QLatin1Char('c')));
    const QJsonArray unsafeBuild = QJsonDocument::fromJson(R"json([
        {"id":1,"channel":"STABLE","downloads":{"server:default":{
            "name":"paper.jar","url":"https://attacker.example/paper.jar",
            "checksums":{"sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}}}
    ])json")
                                       .array();
    QVERIFY_EXCEPTION_THROWN(DevServerManager::latestStablePaperBuild(unsafeBuild),
                             std::runtime_error);
}

void M11ServerTests::enforcesLaunchPrerequisites()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    strata::download::DownloadManager downloads;
    strata::mojang::MojangManifestClient manifest;
    DevServerManager manager(temporaryDirectory.path(), downloads, manifest);
    const ServerConfig created = manager.create(
        QStringLiteral("Not installed"), QStringLiteral("1.21.1"), ServerDistribution::Vanilla);

    try
    {
        manager.launchPlan(created);
        QFAIL("A server without an installation must not produce a launch plan.");
    }
    catch (const std::runtime_error& error)
    {
        QCOMPARE(QString::fromUtf8(error.what()),
                 QStringLiteral("Install this server before starting it."));
    }
}

void M11ServerTests::serverProcessCapturesConsoleAndStops()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    ServerProcess process(QStringLiteral("test-server"), {QStringLiteral(STRATA_PROCESS_FIXTURE),
                                                          {QStringLiteral("server")},
                                                          temporaryDirectory.path()});
    QSignalSpy outputs(&process, &ServerProcess::outputReceived);
    QSignalSpy finished(&process, &ServerProcess::finished);
    QSignalSpy snapshots(&process, &ServerProcess::snapshotChanged);

    process.start();

    QTRY_COMPARE_WITH_TIMEOUT(process.status(), QStringLiteral("running"), 5'000);
    QVERIFY(process.consoleHistory().contains(
        QStringLiteral("[Server thread/INFO]: Done (0.5s)! For help, type \"help\"")));
    process.sendCommand(QStringLiteral("say hello"));
    QTRY_VERIFY_WITH_TIMEOUT(
        process.consoleHistory().contains(QStringLiteral("Received: say hello")), 5'000);
    QVERIFY(!outputs.isEmpty());
    const auto hasMemorySnapshot = [&snapshots]
    {
        for (const QList<QVariant>& snapshot : snapshots)
        {
            if (snapshot.at(4).toLongLong() > 0)
            {
                return true;
            }
        }
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(hasMemorySnapshot(), 5'000);

    process.stop();

    QTRY_COMPARE_WITH_TIMEOUT(process.status(), QStringLiteral("stopped"), 5'000);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.constFirst().at(2).toBool(), false);
}

void M11ServerTests::serverProcessReportsCrashesAndRejectsBadCommands()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    ServerProcess process(QStringLiteral("crashing-server"),
                          {QStringLiteral(STRATA_PROCESS_FIXTURE),
                           {QStringLiteral("crash")},
                           temporaryDirectory.path()});
    QSignalSpy finished(&process, &ServerProcess::finished);

    QVERIFY_EXCEPTION_THROWN(process.sendCommand(QStringLiteral("say hello")), std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(process.sendCommand(QStringLiteral("say\nstop")),
                             std::invalid_argument);
    process.start();

    QTRY_COMPARE_WITH_TIMEOUT(process.status(), QStringLiteral("crashed"), 5'000);
    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.constFirst().at(2).toBool(), true);
    QVERIFY(process.consoleHistory().contains(QStringLiteral("Fatal server condition")));
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    M11ServerTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "M11ServerTests.moc"
