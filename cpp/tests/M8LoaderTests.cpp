#include "M8LoaderTests.h"

#include "../core/auth/OfflineAccountStore.h"
#include "../core/download/DownloadManager.h"
#include "../core/instance/Instance.h"
#include "../core/launch/GameLauncher.h"
#include "../core/loader/LoaderInstaller.h"
#include "../core/runtime/JavaRuntimeManager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <stdexcept>

namespace
{
    bool writeJson(const QString& path, const QJsonObject& value)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) &&
               file.write(QJsonDocument(value).toJson(QJsonDocument::Compact)) >= 0;
    }
} // namespace

void M8LoaderTests::loaderTypesAreValidated()
{
    QCOMPARE(strata::loader::LoaderSelection::parseType(QStringLiteral("fabric")),
             strata::loader::LoaderType::Fabric);
    QCOMPARE(strata::loader::LoaderSelection::typeName(strata::loader::LoaderType::NeoForge),
             QStringLiteral("NEOFORGE"));
    QVERIFY_EXCEPTION_THROWN(strata::loader::LoaderSelection::parseType(QStringLiteral("unknown")),
                             std::runtime_error);
}

void M8LoaderTests::neoForgeVersionsMapToCompatibleMavenPrefixes()
{
    QCOMPARE(strata::loader::LoaderInstaller::neoForgeCompatibilityPrefix(QStringLiteral("1.21.1")),
             QStringLiteral("21.1."));
    QCOMPARE(strata::loader::LoaderInstaller::neoForgeCompatibilityPrefix(QStringLiteral("1.20.4")),
             QStringLiteral("20.4."));
    QVERIFY(strata::loader::LoaderInstaller::neoForgeCompatibilityPrefix(QStringLiteral("1.19.4"))
                .isEmpty());
    QVERIFY(strata::loader::LoaderInstaller::neoForgeCompatibilityPrefix(QStringLiteral("invalid"))
                .isEmpty());
}

void M8LoaderTests::mavenMetadataFiltersAndSortsCompatibleVersions()
{
    const QByteArray xml = R"xml(
        <metadata><versioning><versions>
          <version>1.20.1-47.2.0</version>
          <version>1.20.1-47.3.0</version>
          <version>1.20.1-47.10.0</version>
          <version>1.19.4-45.0.0</version>
          <version>malformed/1.20.1-99</version>
        </versions></versioning></metadata>
    )xml";
    QCOMPARE(strata::loader::LoaderInstaller::parseMavenVersions(xml, QStringLiteral("1.20.1-")),
             QStringList({QStringLiteral("1.20.1-47.10.0"), QStringLiteral("1.20.1-47.3.0"),
                          QStringLiteral("1.20.1-47.2.0")}));
}

void M8LoaderTests::mavenMetadataRejectsDtds()
{
    const QByteArray xml = R"xml(<!DOCTYPE metadata [<!ENTITY x SYSTEM "file:///etc/passwd">]>
        <metadata><version>&x;</version></metadata>)xml";
    QVERIFY_EXCEPTION_THROWN(strata::loader::LoaderInstaller::parseMavenVersions(xml, QString()),
                             std::runtime_error);
}

void M8LoaderTests::loaderProfilesValidateMinecraftInheritance()
{
    const QJsonObject profile{{QStringLiteral("id"), QStringLiteral("fabric-loader-0.16.0-1.21.1")},
                              {QStringLiteral("inheritsFrom"), QStringLiteral("1.21.1")},
                              {QStringLiteral("mainClass"),
                               QStringLiteral("net.fabricmc.loader.impl.launch.knot.KnotClient")},
                              {QStringLiteral("libraries"), QJsonArray{}}};
    const strata::loader::LoaderProfile parsed = strata::loader::LoaderInstaller::parseProfile(
        strata::loader::LoaderType::Fabric, QStringLiteral("1.21.1"), QStringLiteral("0.16.0"),
        profile);
    QCOMPARE(parsed.profileVersionId, QStringLiteral("fabric-loader-0.16.0-1.21.1"));

    QJsonObject wrongVersion = profile;
    wrongVersion.insert(QStringLiteral("inheritsFrom"), QStringLiteral("1.20.4"));
    QVERIFY_EXCEPTION_THROWN(strata::loader::LoaderInstaller::parseProfile(
                                 strata::loader::LoaderType::Fabric, QStringLiteral("1.21.1"),
                                 QStringLiteral("0.16.0"), wrongVersion),
                             std::runtime_error);
}

void M8LoaderTests::loaderMetadataMergesLibrariesAndArguments()
{
    const QJsonObject base{
        {QStringLiteral("mainClass"), QStringLiteral("vanilla.Main")},
        {QStringLiteral("libraries"),
         QJsonArray{QJsonObject{{QStringLiteral("name"), QStringLiteral("shared:lib:1")}}}},
        {QStringLiteral("arguments"),
         QJsonObject{{QStringLiteral("jvm"), QJsonArray{QStringLiteral("-Xmx2G")}},
                     {QStringLiteral("game"), QJsonArray{QStringLiteral("--username")}}}}};
    const QJsonObject loaderMetadata{
        {QStringLiteral("id"), QStringLiteral("fabric-1.21.1")},
        {QStringLiteral("inheritsFrom"), QStringLiteral("1.21.1")},
        {QStringLiteral("mainClass"), QStringLiteral("fabric.Main")},
        {QStringLiteral("libraries"),
         QJsonArray{QJsonObject{{QStringLiteral("name"), QStringLiteral("shared:lib:1")}},
                    QJsonObject{{QStringLiteral("name"), QStringLiteral("fabric:loader:1")}}}},
        {QStringLiteral("arguments"),
         QJsonObject{{QStringLiteral("jvm"), QJsonArray{QStringLiteral("-Dloader=1")}},
                     {QStringLiteral("game"), QJsonArray{QStringLiteral("--fabric")}}}}};
    const strata::loader::LoaderProfile profile{strata::loader::LoaderType::Fabric,
                                                QStringLiteral("1.21.1"), QStringLiteral("0.16.0"),
                                                QStringLiteral("fabric-1.21.1"), loaderMetadata};
    const QJsonObject merged = strata::loader::LoaderInstaller::mergeMetadata(base, profile);
    QCOMPARE(merged.value(QStringLiteral("mainClass")).toString(), QStringLiteral("fabric.Main"));
    const QJsonArray libraries = merged.value(QStringLiteral("libraries")).toArray();
    QCOMPARE(libraries.size(), 2);
    const QJsonObject arguments = merged.value(QStringLiteral("arguments")).toObject();
    QCOMPARE(arguments.value(QStringLiteral("jvm")).toArray().size(), 2);
    QCOMPARE(arguments.value(QStringLiteral("game")).toArray().size(), 2);
    QCOMPARE(arguments.value(QStringLiteral("jvm")).toArray().at(1).toString(),
             QStringLiteral("-Dloader=1"));
}

void M8LoaderTests::libraryPathsCannotEscapeTheirRoot()
{
    const QString root = QDir(QDir::tempPath()).filePath(QStringLiteral("strata-libraries"));
    QVERIFY(strata::loader::LoaderInstaller::resolveLibraryPath(
                root, QStringLiteral("org/example/lib/1.0/lib-1.0.jar"))
                .startsWith(QDir(root).absolutePath()));
    QVERIFY_EXCEPTION_THROWN(
        strata::loader::LoaderInstaller::resolveLibraryPath(root, QStringLiteral("../escape.jar")),
        std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(
        strata::loader::LoaderInstaller::resolveLibraryPath(root, QStringLiteral("..\\escape.jar")),
        std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(
        strata::loader::LoaderInstaller::resolveLibraryPath(root, QStringLiteral("/etc/passwd")),
        std::runtime_error);
}

void M8LoaderTests::pendingLoaderStateCannotBeLaunched()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    strata::Instance instance;
    instance.versionId = QStringLiteral("1.21.1");
    instance.directory = temporaryDirectory.path();
    QVERIFY(QDir().mkpath(instance.gameDirectory()));

    strata::download::DownloadManager downloads;
    strata::runtime::JavaRuntimeManager runtimes(downloads, temporaryDirectory.path());
    strata::loader::LoaderInstaller installer(downloads, runtimes, temporaryDirectory.path());
    installer.prepare(instance, {strata::loader::LoaderType::Fabric, QStringLiteral("0.16.0")});
    QVERIFY_EXCEPTION_THROWN(installer.installedSelection(instance), std::runtime_error);

    const QString statePath =
        QDir(instance.gameDirectory()).filePath(QStringLiteral(".stratalauncher-loader.json"));
    const QJsonObject state{
        {QStringLiteral("type"), QStringLiteral("FABRIC")},
        {QStringLiteral("minecraftVersion"), QStringLiteral("1.21.1")},
        {QStringLiteral("loaderVersion"), QStringLiteral("0.16.0")},
        {QStringLiteral("profileId"), QStringLiteral("fabric-loader-0.16.0-1.21.1")},
        {QStringLiteral("installed"), true}};
    QVERIFY(writeJson(statePath, state));
    const QJsonObject profile{{QStringLiteral("id"), QStringLiteral("fabric-loader-0.16.0-1.21.1")},
                              {QStringLiteral("inheritsFrom"), QStringLiteral("1.21.1")},
                              {QStringLiteral("mainClass"), QStringLiteral("fabric.Main")},
                              {QStringLiteral("libraries"), QJsonArray{}}};
    QVERIFY(writeJson(
        QDir(instance.gameDirectory()).filePath(QStringLiteral("loader-profile.json")), profile));
    QCOMPARE(installer.installedSelection(instance).version, QStringLiteral("0.16.0"));
    QVERIFY(installer.installedProfile(instance).has_value());
}

void M8LoaderTests::launchPlanRetainsLoaderJvmAndGameArguments()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString instanceDirectory =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("instance"));
    const QString gameDirectory = QDir(instanceDirectory).filePath(QStringLiteral("game"));
    const QString versionId = QStringLiteral("1.21.1");
    const QString versionDirectory =
        QDir(gameDirectory).filePath(QStringLiteral("versions/%1").arg(versionId));
    QVERIFY(QDir().mkpath(versionDirectory));

    QFile clientJar(QDir(versionDirectory).filePath(versionId + QStringLiteral(".jar")));
    QVERIFY(clientJar.open(QIODevice::WriteOnly));
    QCOMPARE(clientJar.write("fixture"), qint64(7));
    clientJar.close();

    const QJsonObject metadata{
        {QStringLiteral("mainClass"), QStringLiteral("vanilla.Main")},
        {QStringLiteral("assetIndex"),
         QJsonObject{{QStringLiteral("id"), QStringLiteral("test-assets")}}},
        {QStringLiteral("arguments"),
         QJsonObject{{QStringLiteral("jvm"),
                      QJsonArray{QStringLiteral("-cp"), QStringLiteral("${classpath}")}},
                     {QStringLiteral("game"), QJsonArray{QStringLiteral("--version"),
                                                         QStringLiteral("${version_name}")}}}}};
    QVERIFY(
        writeJson(QDir(versionDirectory).filePath(versionId + QStringLiteral(".json")), metadata));

    const QString assetIndexPath =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("assets/indexes/test-assets.json"));
    QVERIFY(QDir().mkpath(QFileInfo(assetIndexPath).absolutePath()));
    QVERIFY(writeJson(assetIndexPath, QJsonObject{}));
    QFile marker(QDir(gameDirectory).filePath(QStringLiteral(".stratalauncher-installed")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    QCOMPARE(marker.write(versionId.toUtf8()), qint64(versionId.size()));
    marker.close();

    const QString profileId = QStringLiteral("fabric-loader-0.16.0-1.21.1");
    QVERIFY(writeJson(QDir(gameDirectory).filePath(QStringLiteral(".stratalauncher-loader.json")),
                      QJsonObject{{QStringLiteral("type"), QStringLiteral("FABRIC")},
                                  {QStringLiteral("minecraftVersion"), versionId},
                                  {QStringLiteral("loaderVersion"), QStringLiteral("0.16.0")},
                                  {QStringLiteral("profileId"), profileId},
                                  {QStringLiteral("installed"), true}}));
    const QJsonObject loaderProfile{
        {QStringLiteral("id"), profileId},
        {QStringLiteral("inheritsFrom"), versionId},
        {QStringLiteral("mainClass"), QStringLiteral("fabric.Main")},
        {QStringLiteral("libraries"), QJsonArray{}},
        {QStringLiteral("arguments"),
         QJsonObject{{QStringLiteral("jvm"), QJsonArray{QStringLiteral("-Dloader.flag=true")}},
                     {QStringLiteral("game"), QJsonArray{QStringLiteral("--loader-flag")}}}}};
    QVERIFY(writeJson(QDir(gameDirectory).filePath(QStringLiteral("loader-profile.json")),
                      loaderProfile));

    const strata::Instance instance{
        QStringLiteral("id"), QStringLiteral("Test"), versionId, {}, instanceDirectory, false};
    const strata::auth::OfflineAccount account{
        QStringLiteral("Player"),
        strata::auth::OfflineAccountStore::offlineUuid(QStringLiteral("Player"))};
    const strata::launch::LaunchPlan plan = strata::launch::GameLauncher::buildPlan(
        instance, account, temporaryDirectory.path(), QStringLiteral("/usr/bin/java"));
    QVERIFY(plan.arguments.contains(QStringLiteral("fabric.Main")));
    QVERIFY(plan.arguments.contains(QStringLiteral("-Dloader.flag=true")));
    QVERIFY(plan.arguments.contains(QStringLiteral("--loader-flag")));
    QVERIFY(plan.arguments.contains(profileId));
}

QTEST_APPLESS_MAIN(M8LoaderTests)
