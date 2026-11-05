#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "Paths.h"
#include "instance/InstanceManager.h"
#include "logging/LauncherLogging.h"
#include "settings/InstanceSettings.h"

#include <algorithm>
#include <stdexcept>

using namespace strata;

class M3CoreTests : public QObject
{
    Q_OBJECT

private slots:
    void resolvesPlatformDataDirectories();
    void createsDuplicatesAndDeletesInstances();
    void ordersFavoritesAndReadsLegacyMetadata();
    void rejectsBlankInstanceValues();
    void persistsInstanceSettingsAndValidatesFlags();
    void redactsCredentialsAndWritesLogs();
};

void M3CoreTests::resolvesPlatformDataDirectories()
{
    QCOMPARE(Paths::dataDirectory(OperatingSystem::Windows, QStringLiteral("C:/Users/test"),
                                  QStringLiteral("D:/UserData"), QStringLiteral("")),
             QStringLiteral("D:/UserData/StrataLauncher"));
    QCOMPARE(Paths::dataDirectory(OperatingSystem::Windows, QStringLiteral("C:/Users/test")),
             QStringLiteral("C:/Users/test/AppData/Roaming/StrataLauncher"));
    QCOMPARE(Paths::dataDirectory(OperatingSystem::MacOS, QStringLiteral("/Users/test")),
             QStringLiteral("/Users/test/Library/Application Support/StrataLauncher"));
    QCOMPARE(Paths::dataDirectory(OperatingSystem::Linux, QStringLiteral("/home/test")),
             QStringLiteral("/home/test/.local/share/StrataLauncher"));
    QCOMPARE(Paths::dataDirectory(OperatingSystem::Linux, QStringLiteral("/home/test"), QString(),
                                  QStringLiteral("/mnt/data")),
             QStringLiteral("/mnt/data/StrataLauncher"));
}

void M3CoreTests::createsDuplicatesAndDeletesInstances()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    InstanceManager manager(temporaryDirectory.path() + QStringLiteral("/launcher-data"));

    const Instance original = manager.create(QStringLiteral("Original"), QStringLiteral("1.21.1"));
    QVERIFY(QFileInfo(original.gameDirectory()).isDir());
    QCOMPARE(manager.listInstances(), QList<Instance>{original});

    const QString worldPath =
        QDir(original.gameDirectory()).filePath(QStringLiteral("saves/world/level.dat"));
    QVERIFY(QDir().mkpath(QFileInfo(worldPath).absolutePath()));
    QFile world(worldPath);
    QVERIFY(world.open(QIODevice::WriteOnly));
    QCOMPARE(world.write("world data"), qint64(10));
    world.close();

    const Instance duplicate = manager.duplicate(original, QStringLiteral("Copy"));
    QVERIFY(duplicate.id != original.id);
    QVERIFY(duplicate.gameDirectory() != original.gameDirectory());
    QFile copiedWorld(
        QDir(duplicate.gameDirectory()).filePath(QStringLiteral("saves/world/level.dat")));
    QVERIFY(copiedWorld.open(QIODevice::ReadOnly));
    QCOMPARE(copiedWorld.readAll(), QByteArray("world data"));

    manager.deleteInstance(duplicate);
    QVERIFY(QFileInfo(original.gameDirectory()).isDir());
    QVERIFY(!QFileInfo::exists(duplicate.directory));
    QCOMPARE(manager.listInstances().size(), 1);
}

void M3CoreTests::ordersFavoritesAndReadsLegacyMetadata()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    InstanceManager manager(temporaryDirectory.path());
    manager.create(QStringLiteral("Alpha"), QStringLiteral("1.21.1"));
    const Instance beta = manager.create(QStringLiteral("Beta"), QStringLiteral("1.21.1"));
    const Instance favorite = manager.setFavorite(beta, true);
    QCOMPARE(manager.listInstances().constFirst(), favorite);
    QCOMPARE(manager.rename(favorite, QStringLiteral("  Renamed  ")).name,
             QStringLiteral("Renamed"));

    const Instance legacy = manager.create(QStringLiteral("Legacy"), QStringLiteral("1.20.1"));
    QFile metadata(QDir(legacy.directory).filePath(QStringLiteral("instance.json")));
    QVERIFY(metadata.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QJsonObject oldMetadata{
        {QStringLiteral("id"), legacy.id},
        {QStringLiteral("name"), legacy.name},
        {QStringLiteral("versionId"), legacy.versionId},
        {QStringLiteral("createdAt"), legacy.createdAt.toUTC().toString(Qt::ISODateWithMs)}};
    QCOMPARE(metadata.write(QJsonDocument(oldMetadata).toJson()),
             qint64(QJsonDocument(oldMetadata).toJson().size()));
    metadata.close();

    const auto instances = manager.listInstances();
    const auto legacyInstance =
        std::find_if(instances.cbegin(), instances.cend(),
                     [&legacy](const Instance& entry) { return entry.id == legacy.id; });
    QVERIFY(legacyInstance != instances.cend());
    QVERIFY(!legacyInstance->favorite);
}

void M3CoreTests::rejectsBlankInstanceValues()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    InstanceManager manager(temporaryDirectory.path());
    QVERIFY_EXCEPTION_THROWN(manager.create(QStringLiteral("  "), QStringLiteral("1.21.1")),
                             std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN(manager.create(QStringLiteral("Name"), QStringLiteral(" ")),
                             std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN(manager.rename({}, QStringLiteral("New")), std::invalid_argument);
}

void M3CoreTests::persistsInstanceSettingsAndValidatesFlags()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    InstanceManager manager(temporaryDirectory.path());
    const Instance instance = manager.create(QStringLiteral("Test"), QStringLiteral("1.21.1"));
    InstanceSettings settings(instance.directory);
    const QStringList globalFlags{QStringLiteral("-Xmx2G"), QStringLiteral("-XX:+UseG1GC")};
    QVERIFY(!settings.hasJavaLaunchFlagsOverride());
    QCOMPARE(settings.javaRuntimeMode(), InstanceSettings::JavaRuntimeMode::System);
    QCOMPARE(settings.effectiveJavaLaunchFlags(globalFlags), globalFlags);

    settings.setJavaLaunchFlagsOverride({QStringLiteral(" -Xmx4G "), QStringLiteral("")});
    settings.setJavaRuntimeMode(InstanceSettings::JavaRuntimeMode::Managed);
    settings.setJavaRuntimeComponent(QStringLiteral("temurin-java-21"));
    settings.setJavaRuntimeRedundant(true);
    InstanceSettings reloaded(instance.directory);
    QVERIFY(reloaded.hasJavaLaunchFlagsOverride());
    QCOMPARE(reloaded.javaLaunchFlagsOverride(), QStringList{QStringLiteral("-Xmx4G")});
    QCOMPARE(reloaded.javaRuntimeMode(), InstanceSettings::JavaRuntimeMode::Managed);
    QCOMPARE(reloaded.javaRuntimeComponent(), QStringLiteral("temurin-java-21"));
    QVERIFY(reloaded.javaRuntimeRedundant());
    QVERIFY_EXCEPTION_THROWN(InstanceSettings::validateFlags({QStringLiteral("@args.txt")}),
                             std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN(InstanceSettings::validateFlags({QStringLiteral("-cp")}),
                             std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN(InstanceSettings::validateFlags(
                                 {QStringLiteral("-XX:+UseG1GC"), QStringLiteral("-XX:+UseZGC")}),
                             std::invalid_argument);

    reloaded.clearJavaLaunchFlagsOverride();
    QVERIFY(!InstanceSettings(instance.directory).hasJavaLaunchFlagsOverride());

    QFile legacySettings(QDir(instance.directory).filePath(QStringLiteral("settings.json")));
    QVERIFY(legacySettings.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray legacyJson = R"({"javaLaunchFlagsOverride":["-Xmx3G"]})";
    QCOMPARE(legacySettings.write(legacyJson), qint64(legacyJson.size()));
    legacySettings.close();
    const InstanceSettings legacy(instance.directory);
    QCOMPARE(legacy.javaRuntimeMode(), InstanceSettings::JavaRuntimeMode::System);
    QCOMPARE(legacy.javaLaunchFlagsOverride(), QStringList{QStringLiteral("-Xmx3G")});
}

void M3CoreTests::redactsCredentialsAndWritesLogs()
{
    QCOMPARE(LauncherLogging::redact(QStringLiteral("access_token=secret --password pass")),
             QStringLiteral("access_token=[REDACTED] --password [REDACTED]"));
    QCOMPARE(LauncherLogging::redact(QStringLiteral("--accessToken hidden")),
             QStringLiteral("--accessToken [REDACTED]"));
    QCOMPARE(LauncherLogging::redact(QStringLiteral("--accessToken=hidden")),
             QStringLiteral("--accessToken=[REDACTED]"));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QtMessageHandler previousHandler = qInstallMessageHandler(nullptr);
    QString error;
    QVERIFY2(LauncherLogging::configure(temporaryDirectory.path(), &error), qPrintable(error));
    qInfo().noquote() << "access_token=private-value";
    qInstallMessageHandler(previousHandler);

    QFile logFile(
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("logs/stratalauncher.log")));
    QVERIFY(logFile.open(QIODevice::ReadOnly));
    const QByteArray log = logFile.readAll();
    QVERIFY(log.contains("[REDACTED]"));
    QVERIFY(!log.contains("private-value"));
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    M3CoreTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "M3CoreTests.moc"
