#include "M6RuntimeTests.h"

#include "Paths.h"
#include "instance/InstanceManager.h"
#include "runtime/JavaRuntimeManager.h"
#include "settings/InstanceSettings.h"

#include <archive.h>
#include <archive_entry.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <memory>
#include <stdexcept>

using namespace strata;

void M6RuntimeTests::mapsMinecraftVersionsAndJavaMajors()
{
    QCOMPARE(runtime::JavaRuntimeManager::requiredComponent({}, QStringLiteral("1.16.5")),
             QStringLiteral("jre-legacy"));
    QCOMPARE(runtime::JavaRuntimeManager::requiredComponent({}, QStringLiteral("1.17.1")),
             QStringLiteral("java-runtime-alpha"));
    QCOMPARE(runtime::JavaRuntimeManager::requiredComponent({}, QStringLiteral("1.20.4")),
             QStringLiteral("java-runtime-gamma"));
    QCOMPARE(runtime::JavaRuntimeManager::requiredComponent({}, QStringLiteral("1.21.1")),
             QStringLiteral("java-runtime-delta"));
    QCOMPARE(runtime::JavaRuntimeManager::requiredComponent({}, QStringLiteral("26.1")),
             QStringLiteral("java-runtime-epsilon"));
    QCOMPARE(runtime::JavaRuntimeManager::requiredComponent({}, QStringLiteral("19w14a")),
             QStringLiteral("jre-legacy"));
    QCOMPARE(
        runtime::JavaRuntimeManager::requiredComponent(
            {{QStringLiteral("javaVersion"),
              QJsonObject{{QStringLiteral("component"), QStringLiteral("java-runtime-gamma")}}}},
            QStringLiteral("1.21.1")),
        QStringLiteral("java-runtime-gamma"));

    QCOMPARE(
        runtime::JavaRuntimeManager::requiredJavaMajor(
            {{QStringLiteral("javaVersion"), QJsonObject{{QStringLiteral("majorVersion"), 21}}}},
            QStringLiteral("1.21.1")),
        21);
    QCOMPARE(runtime::JavaRuntimeManager::requiredJavaMajor({}, QStringLiteral("1.20.4")), 17);
    QVERIFY_EXCEPTION_THROWN(
        runtime::JavaRuntimeManager::requiredComponent(
            {{QStringLiteral("javaVersion"),
              QJsonObject{{QStringLiteral("component"), QStringLiteral("../escape")}}}},
            QStringLiteral("1.21.1")),
        std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(runtime::JavaRuntimeManager::requiredComponent(
                                 {}, QStringLiteral("not-a-minecraft-version")),
                             std::runtime_error);
}

void M6RuntimeTests::selectsPlatformAndNewestMojangRuntime()
{
    QCOMPARE(runtime::JavaRuntimeManager::platformName(QStringLiteral("Linux"),
                                                       QStringLiteral("x86_64")),
             QStringLiteral("linux"));
    QCOMPARE(runtime::JavaRuntimeManager::platformName(QStringLiteral("Windows 11"),
                                                       QStringLiteral("aarch64")),
             QStringLiteral("windows-arm64"));
    QCOMPARE(runtime::JavaRuntimeManager::platformName(QStringLiteral("Mac OS X"),
                                                       QStringLiteral("arm64")),
             QStringLiteral("mac-os-arm64"));
    QCOMPARE(
        runtime::JavaRuntimeManager::platformName(QStringLiteral("Linux"), QStringLiteral("i686")),
        QStringLiteral("linux-i386"));
    QVERIFY_EXCEPTION_THROWN(runtime::JavaRuntimeManager::platformName(QStringLiteral("FreeBSD"),
                                                                       QStringLiteral("x86_64")),
                             std::runtime_error);

    const QJsonObject index{
        {QStringLiteral("linux"),
         QJsonObject{
             {QStringLiteral("java-runtime-gamma"),
              QJsonArray{
                  QJsonObject{
                      {QStringLiteral("manifest"),
                       QJsonObject{{QStringLiteral("url"),
                                    QStringLiteral("https://piston-meta.mojang.com/old.json")},
                                   {QStringLiteral("sha1"), QString(40, QLatin1Char('a'))}}},
                      {QStringLiteral("version"),
                       QJsonObject{
                           {QStringLiteral("name"), QStringLiteral("17.0.1")},
                           {QStringLiteral("released"), QStringLiteral("2021-01-01T00:00:00Z")}}}},
                  QJsonObject{
                      {QStringLiteral("manifest"),
                       QJsonObject{{QStringLiteral("url"),
                                    QStringLiteral("https://piston-meta.mojang.com/new.json")},
                                   {QStringLiteral("sha1"), QString(40, QLatin1Char('b'))}}},
                      {QStringLiteral("version"),
                       QJsonObject{{QStringLiteral("name"), QStringLiteral("17.0.2")},
                                   {QStringLiteral("released"),
                                    QStringLiteral("2022-01-01T00:00:00Z")}}}}}}}}};
    QJsonObject selected;
    QCOMPARE(runtime::JavaRuntimeManager::selectOfficialRuntime(
                 index, QStringLiteral("linux"), QStringLiteral("java-runtime-gamma"), &selected),
             QStringLiteral("17.0.2"));
    QCOMPARE(selected.value(QStringLiteral("name")).toString(), QStringLiteral("17.0.2"));
    QVERIFY_EXCEPTION_THROWN(
        runtime::JavaRuntimeManager::selectOfficialRuntime(index, QStringLiteral("missing"),
                                                           QStringLiteral("java-runtime-gamma")),
        std::runtime_error);
}

void M6RuntimeTests::parsesRuntimeManifestAndRejectsUnsafeEntries()
{
    const QJsonObject manifest{
        {QStringLiteral("files"),
         QJsonObject{
             {QStringLiteral("bin"),
              QJsonObject{{QStringLiteral("type"), QStringLiteral("directory")}}},
             {QStringLiteral("bin/java"),
              QJsonObject{
                  {QStringLiteral("type"), QStringLiteral("file")},
                  {QStringLiteral("executable"), true},
                  {QStringLiteral("downloads"),
                   QJsonObject{
                       {QStringLiteral("raw"),
                        QJsonObject{{QStringLiteral("url"),
                                     QStringLiteral("https://piston-data.mojang.com/java")},
                                    {QStringLiteral("sha1"), QString(40, QLatin1Char('c'))}}}}}}},
             {QStringLiteral("bin/java-link"),
              QJsonObject{{QStringLiteral("type"), QStringLiteral("link")},
                          {QStringLiteral("target"), QStringLiteral("java")}}}}}};
    const auto files = runtime::JavaRuntimeManager::parseRuntimeFiles(manifest);
    QCOMPARE(files.size(), 3);
    QCOMPARE(files.at(0).type, runtime::RuntimeFile::Type::Directory);
    QCOMPARE(files.at(1).type, runtime::RuntimeFile::Type::File);
    QVERIFY(files.at(1).executable);
    QCOMPARE(files.at(2).type, runtime::RuntimeFile::Type::Link);
    QCOMPARE(files.at(2).target, QStringLiteral("java"));

    QVERIFY_EXCEPTION_THROWN(
        runtime::JavaRuntimeManager::parseRuntimeFiles(
            {{QStringLiteral("files"),
              QJsonObject{{QStringLiteral("../outside"),
                           QJsonObject{{QStringLiteral("type"), QStringLiteral("directory")}}}}}}),
        std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(
        runtime::JavaRuntimeManager::parseRuntimeFiles(
            {{QStringLiteral("files"),
              QJsonObject{
                  {QStringLiteral("escape"),
                   QJsonObject{{QStringLiteral("type"), QStringLiteral("link")},
                               {QStringLiteral("target"), QStringLiteral("../../outside")}}}}}}),
        std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(
        runtime::JavaRuntimeManager::parseRuntimeFiles(
            {{QStringLiteral("files"),
              QJsonObject{
                  {QStringLiteral("bin/java"),
                   QJsonObject{{QStringLiteral("type"), QStringLiteral("file")},
                               {QStringLiteral("downloads"),
                                QJsonObject{{QStringLiteral("raw"),
                                             QJsonObject{{QStringLiteral("url"),
                                                          QStringLiteral("http://evil.test/java")},
                                                         {QStringLiteral("sha1"),
                                                          QString(40, QLatin1Char('d'))}}}}}}}}}}),
        std::runtime_error);
}

void M6RuntimeTests::resolvesRuntimePathsAndPersistsPreferences()
{
    const QString data = QStringLiteral("/home/test/.local/share/StrataLauncher");
    const QString instance = data + QStringLiteral("/instances/example");
    QCOMPARE(runtime::JavaRuntimeManager::runtimeDirectory(
                 instance, data, QStringLiteral("temurin-java-21"), false),
             data + QStringLiteral("/java-runtimes/temurin-java-21"));
    QCOMPARE(runtime::JavaRuntimeManager::runtimeDirectory(instance, data,
                                                           QStringLiteral("temurin-java-21"), true),
             instance + QStringLiteral("/java-runtimes/temurin-java-21"));
    QVERIFY_EXCEPTION_THROWN(runtime::JavaRuntimeManager::runtimeDirectory(
                                 instance, data, QStringLiteral("../outside"), false),
                             std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(runtime::JavaRuntimeManager::validateRuntimePath(
                                 QStringLiteral("/tmp/runtime"), QStringLiteral("../outside")),
                             std::runtime_error);

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    download::DownloadManager downloads;
    runtime::JavaRuntimeManager manager(downloads, temporaryDirectory.path());
    QCOMPARE(manager.preferences().vendor, runtime::RuntimeVendor::Mojang);
    QVERIFY(!manager.preferences().redundantRuntimes);
    manager.setPreferences({runtime::RuntimeVendor::Temurin, true});
    const auto reloaded = manager.preferences();
    QCOMPARE(reloaded.vendor, runtime::RuntimeVendor::Temurin);
    QVERIFY(reloaded.redundantRuntimes);
    QCOMPARE(runtime::JavaRuntimeManager::vendorName(runtime::RuntimeVendor::Zulu),
             QStringLiteral("zulu"));
    QCOMPARE(runtime::JavaRuntimeManager::parseVendor(QStringLiteral("TEMURIN")),
             runtime::RuntimeVendor::Temurin);
    QCOMPARE(manager.runtimeComponent(runtime::RuntimeVendor::Zulu,
                                      {{QStringLiteral("javaVersion"),
                                        QJsonObject{{QStringLiteral("majorVersion"), 21}}}},
                                      QStringLiteral("1.21.1")),
             QStringLiteral("zulu-java-21"));
}

void M6RuntimeTests::resolvesManagedJavaAndVerifiesItsIntegrity()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    InstanceManager instances(temporaryDirectory.path());
    const Instance instance =
        instances.create(QStringLiteral("Runtime test"), QStringLiteral("1.21.1"));
    const QString metadataPath =
        QDir(instance.gameDirectory()).filePath(QStringLiteral("versions/1.21.1/1.21.1.json"));
    QVERIFY(QDir().mkpath(QFileInfo(metadataPath).absolutePath()));
    QFile metadata(metadataPath);
    QVERIFY(metadata.open(QIODevice::WriteOnly));
    const QByteArray json =
        QJsonDocument(QJsonObject{{QStringLiteral("javaVersion"),
                                   QJsonObject{{QStringLiteral("component"),
                                                QStringLiteral("java-runtime-delta")}}}})
            .toJson();
    QCOMPARE(metadata.write(json), json.size());
    metadata.close();

    const QString runtimeDirectory =
        QDir(temporaryDirectory.path())
            .filePath(QStringLiteral("java-runtimes/java-runtime-delta"));
    const QString executable = QDir(runtimeDirectory).filePath(QStringLiteral("bin/java"));
    QVERIFY(QDir().mkpath(QFileInfo(executable).absolutePath()));
    QFile java(executable);
    QVERIFY(java.open(QIODevice::WriteOnly));
    QCOMPARE(java.write("fake java"), qint64(9));
    java.close();
    QVERIFY(java.setPermissions(java.permissions() | QFileDevice::ExeOwner));
    const QString checksum = QString::fromLatin1(
        QCryptographicHash::hash(QByteArrayLiteral("fake java"), QCryptographicHash::Sha1).toHex());
    QFile marker(QDir(runtimeDirectory).filePath(QStringLiteral(".stratalauncher-java-path")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    const QByteArray markerContents = QByteArrayLiteral("bin/java\n") + checksum.toLatin1() + '\n';
    QCOMPARE(marker.write(markerContents), markerContents.size());
    marker.close();
    QFile versionMarker(
        QDir(runtimeDirectory).filePath(QStringLiteral(".stratalauncher-runtime-version")));
    QVERIFY(versionMarker.open(QIODevice::WriteOnly));
    QCOMPARE(versionMarker.write("21.0.7"), qint64(6));
    versionMarker.close();

    const QString redundantDirectory =
        QDir(instance.directory).filePath(QStringLiteral("java-runtimes/java-runtime-delta"));
    QVERIFY(QDir().mkpath(QDir(redundantDirectory).filePath(QStringLiteral("bin"))));
    QVERIFY(QFile::copy(executable, QDir(redundantDirectory).filePath(QStringLiteral("bin/java"))));
    QVERIFY(QFile::copy(
        marker.fileName(),
        QDir(redundantDirectory).filePath(QStringLiteral(".stratalauncher-java-path"))));
    QVERIFY(QFile::copy(
        versionMarker.fileName(),
        QDir(redundantDirectory).filePath(QStringLiteral(".stratalauncher-runtime-version"))));

    InstanceSettings settings(instance.directory);
    settings.setJavaRuntimeComponent(QStringLiteral("java-runtime-delta"));
    settings.setJavaRuntimeMode(InstanceSettings::JavaRuntimeMode::Managed);
    settings.setJavaRuntimeRedundant(true);
    download::DownloadManager downloads;
    runtime::JavaRuntimeManager runtimeManager(downloads, temporaryDirectory.path());
    runtimeManager.setPreferences({runtime::RuntimeVendor::Mojang, false});
    QCOMPARE(runtimeManager.javaExecutable(instance),
             QDir(redundantDirectory).filePath(QStringLiteral("bin/java")));
    settings.setJavaRuntimeRedundant(false);
    QCOMPARE(runtimeManager.javaExecutable(instance), executable);

    QVERIFY(java.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(java.write("tampered"), qint64(8));
    java.close();
    QVERIFY_EXCEPTION_THROWN(runtimeManager.javaExecutable(instance), std::runtime_error);
}

void M6RuntimeTests::extractsSafeArchivesAndRejectsTraversal()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const auto createArchive =
        [&temporaryDirectory](const QString& name, const char* entryName, const char* contents)
    {
        const QString path = QDir(temporaryDirectory.path()).filePath(name);
        std::unique_ptr<struct archive, decltype(&archive_write_free)> writer(archive_write_new(),
                                                                              &archive_write_free);
        if (!writer || archive_write_set_format_zip(writer.get()) != ARCHIVE_OK)
        {
            return QString();
        }
        const QByteArray encoded = QFile::encodeName(path);
        if (archive_write_open_filename(writer.get(), encoded.constData()) != ARCHIVE_OK)
        {
            return QString();
        }
        std::unique_ptr<struct archive_entry, decltype(&archive_entry_free)> entry(
            archive_entry_new(), &archive_entry_free);
        const qsizetype size = qstrlen(contents);
        archive_entry_set_pathname(entry.get(), entryName);
        archive_entry_set_filetype(entry.get(), AE_IFREG);
        archive_entry_set_perm(entry.get(), 0644);
        archive_entry_set_size(entry.get(), size);
        if (archive_write_header(writer.get(), entry.get()) != ARCHIVE_OK ||
            archive_write_data(writer.get(), contents, size) != size ||
            archive_write_close(writer.get()) != ARCHIVE_OK)
        {
            return QString();
        }
        return path;
    };

    const QString safeArchive = createArchive(QStringLiteral("safe.zip"), "bin/java", "runtime");
    QVERIFY(!safeArchive.isEmpty());
    const QString destination = QDir(temporaryDirectory.path()).filePath(QStringLiteral("runtime"));
    runtime::JavaRuntimeManager::extractArchive(safeArchive, destination);
    QFile extracted(QDir(destination).filePath(QStringLiteral("bin/java")));
    QVERIFY(extracted.open(QIODevice::ReadOnly));
    QCOMPARE(extracted.readAll(), QByteArrayLiteral("runtime"));

    const QString unsafeArchive =
        createArchive(QStringLiteral("unsafe.zip"), "../outside", "escape");
    QVERIFY(!unsafeArchive.isEmpty());
    QVERIFY_EXCEPTION_THROWN(
        runtime::JavaRuntimeManager::extractArchive(
            unsafeArchive,
            QDir(temporaryDirectory.path()).filePath(QStringLiteral("unsafe-runtime"))),
        std::runtime_error);
    QVERIFY(
        !QFileInfo::exists(QDir(temporaryDirectory.path()).filePath(QStringLiteral("outside"))));
}

void M6RuntimeTests::inventoriesOwnersAndBlocksInUseDeletion()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString dataDirectory =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("launcher"));
    const QString instanceDirectory =
        QDir(dataDirectory).filePath(QStringLiteral("instances/test"));
    const QString shared =
        QDir(dataDirectory).filePath(QStringLiteral("java-runtimes/temurin-java-21"));
    QVERIFY(QDir().mkpath(shared));
    QVERIFY(QDir().mkpath(instanceDirectory));
    QFile marker(QDir(shared).filePath(QStringLiteral(".stratalauncher-java-path")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    QCOMPARE(marker.write("bin/java\n0000000000000000000000000000000000000000\n"), qint64(50));
    marker.close();
    QFile version(QDir(shared).filePath(QStringLiteral(".stratalauncher-runtime-version")));
    QVERIFY(version.open(QIODevice::WriteOnly));
    QCOMPARE(version.write("21.0.7"), qint64(6));
    version.close();
    QFile vendor(QDir(shared).filePath(QStringLiteral(".stratalauncher-runtime-vendor")));
    QVERIFY(vendor.open(QIODevice::WriteOnly));
    QCOMPARE(vendor.write("TEMURIN"), qint64(7));
    vendor.close();

    download::DownloadManager downloads;
    runtime::JavaRuntimeManager manager(downloads, dataDirectory);
    const QList<runtime::RuntimeOwner> owners{{QStringLiteral("Instance: Test"), instanceDirectory,
                                               QStringLiteral("temurin-java-21"), true, false}};
    const auto installed = manager.listInstalledRuntimes(owners);
    QCOMPARE(installed.size(), 1);
    QCOMPARE(installed.first().version, QStringLiteral("21.0.7"));
    QCOMPARE(installed.first().vendor, runtime::RuntimeVendor::Temurin);
    QCOMPARE(installed.first().users, QStringList{QStringLiteral("Instance: Test")});
    QVERIFY_EXCEPTION_THROWN(manager.deleteRuntime(installed.first(), owners), std::runtime_error);

    auto redundant = owners;
    redundant[0].redundant = true;
    const QString redundantDirectory =
        QDir(instanceDirectory).filePath(QStringLiteral("java-runtimes/temurin-java-21"));
    QVERIFY(QDir().mkpath(redundantDirectory));
    QVERIFY(QFile::copy(
        marker.fileName(),
        QDir(redundantDirectory).filePath(QStringLiteral(".stratalauncher-java-path"))));
    QVERIFY(QFile::copy(
        version.fileName(),
        QDir(redundantDirectory).filePath(QStringLiteral(".stratalauncher-runtime-version"))));
    QVERIFY(QFile::copy(
        vendor.fileName(),
        QDir(redundantDirectory).filePath(QStringLiteral(".stratalauncher-runtime-vendor"))));
    const auto bothInstalled = manager.listInstalledRuntimes(redundant);
    QCOMPARE(bothInstalled.size(), 2);
    const auto redundantRuntime =
        std::find_if(bothInstalled.cbegin(), bothInstalled.cend(),
                     [&redundantDirectory](const runtime::InstalledRuntime& candidate)
                     { return candidate.directory == redundantDirectory; });
    QVERIFY(redundantRuntime != bothInstalled.cend());
    QCOMPARE(redundantRuntime->users, QStringList{QStringLiteral("Instance: Test")});
    const auto sharedRuntime = std::find_if(bothInstalled.cbegin(), bothInstalled.cend(),
                                            [&shared](const runtime::InstalledRuntime& candidate)
                                            { return candidate.directory == shared; });
    QVERIFY(sharedRuntime != bothInstalled.cend());
    QVERIFY(sharedRuntime->users.isEmpty());
    manager.deleteRuntime(*sharedRuntime, redundant);

    auto unused = installed.first();
    auto noLongerManaged = owners;
    noLongerManaged[0].managed = false;
    noLongerManaged[0].redundant = true;
    unused.directory = redundantDirectory;
    manager.deleteRuntime(unused, noLongerManaged);
    QVERIFY(!QFileInfo::exists(redundantDirectory));
}

QTEST_GUILESS_MAIN(M6RuntimeTests)
