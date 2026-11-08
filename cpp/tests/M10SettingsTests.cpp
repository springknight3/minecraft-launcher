#include "instance/InstanceManager.h"
#include "settings/InstanceSettings.h"
#include "settings/InstanceSettingsController.h"
#include "settings/MinecraftControls.h"
#include "settings/MinecraftOptionValues.h"
#include "settings/NbtFile.h"
#include "settings/OptionsFile.h"
#include "settings/ServerListStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <stdexcept>

using namespace strata::settings;

class M10SettingsTests : public QObject
{
    Q_OBJECT

private slots:
    void preservesUnknownOptionsAndLineEndings();
    void updatesOnlyTheLastDuplicateOption();
    void convertsNormalizedFovValues();
    void identifiesAndDisplaysMinecraftControls();
    void editsServersAndPreservesUnknownNbt();
    void migratesServerCompressionForMinecraft263();
    void refusesMalformedServerListsAndInvalidUpdates();
    void loadsAndSavesSettingsOffTheGuiThread();
};

void M10SettingsTests::preservesUnknownOptionsAndLineEndings()
{
    const QString original = QStringLiteral(
        "# preserved comment\r\nunknown_future_option:opaque:value\r\nmaxFps:120\r\n");
    QCOMPARE(OptionsFile::parse(original).value(QStringLiteral("unknown_future_option")),
             QStringLiteral("opaque:value"));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = temporaryDirectory.filePath(QStringLiteral("options.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(original.toUtf8()), qint64(original.toUtf8().size()));
    file.close();

    OptionsFile::set(path, QStringLiteral("maxFps"), QStringLiteral("240"));

    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(file.readAll()),
             QStringLiteral(
                 "# preserved comment\r\nunknown_future_option:opaque:value\r\nmaxFps:240\r\n"));
}

void M10SettingsTests::updatesOnlyTheLastDuplicateOption()
{
    QString content = OptionsFile::update(QStringLiteral("maxFps:60\nfutureKey:keep\nmaxFps:120"),
                                          QStringLiteral("maxFps"), QStringLiteral("240"));
    content = OptionsFile::update(content, QStringLiteral("newOption"), QStringLiteral("enabled"));
    QCOMPARE(content, QStringLiteral("maxFps:60\nfutureKey:keep\nmaxFps:240\nnewOption:enabled\n"));
    QVERIFY_EXCEPTION_THROWN(
        OptionsFile::update(content, QStringLiteral("bad:key"), QStringLiteral("value")),
        std::invalid_argument);
    QVERIFY_EXCEPTION_THROWN(
        OptionsFile::update(content, QStringLiteral("valid"), QStringLiteral("line\nbreak")),
        std::invalid_argument);
}

void M10SettingsTests::convertsNormalizedFovValues()
{
    QCOMPARE(MinecraftOptionValues::toDisplayValue(QStringLiteral("26.3"), QStringLiteral("fov"),
                                                   QStringLiteral("0.0")),
             QStringLiteral("70"));
    QCOMPARE(MinecraftOptionValues::toDisplayValue(QStringLiteral("26.3"), QStringLiteral("fov"),
                                                   QStringLiteral("1.0")),
             QStringLiteral("110"));
    QCOMPARE(MinecraftOptionValues::toStoredValue(QStringLiteral("26.3"), QStringLiteral("fov"),
                                                  QStringLiteral("30")),
             QStringLiteral("-1.0"));
    QCOMPARE(MinecraftOptionValues::toStoredValue(QStringLiteral("1.21.1"), QStringLiteral("fov"),
                                                  QStringLiteral("110")),
             QStringLiteral("110"));
    QVERIFY_EXCEPTION_THROWN(MinecraftOptionValues::toStoredValue(QStringLiteral("26.3"),
                                                                  QStringLiteral("fov"),
                                                                  QStringLiteral("111")),
                             std::invalid_argument);
}

void M10SettingsTests::identifiesAndDisplaysMinecraftControls()
{
    const QMap<QString, QString> bindings = MinecraftControls::bindings(
        {{QStringLiteral("key_key.forward"), QStringLiteral("key.keyboard.up")},
         {QStringLiteral("key_key.mod.example"), QStringLiteral("key.keyboard.m")}});
    QCOMPARE(MinecraftControls::actionName(QStringLiteral("key_key.forward")),
             QStringLiteral("Move Forward"));
    QCOMPARE(MinecraftControls::displayKey(bindings.value(QStringLiteral("key_key.forward"))),
             QStringLiteral("Up"));
    QCOMPARE(MinecraftControls::actionName(QStringLiteral("key_key.mod.example")),
             QStringLiteral("Mod Example"));

    const QMap<QString, QStringList> conflicts = MinecraftControls::conflicts(
        {{QStringLiteral("key_key.forward"), QStringLiteral("key.keyboard.w")},
         {QStringLiteral("key_key.mod.example"), QStringLiteral("key.keyboard.w")},
         {QStringLiteral("key_key.jump"), QStringLiteral("key.keyboard.unknown")}});
    QCOMPARE(
        conflicts.value(QStringLiteral("key.keyboard.w")),
        QStringList({QStringLiteral("key_key.forward"), QStringLiteral("key_key.mod.example")}));
    QVERIFY(MinecraftControls::conflicts(MinecraftControls::defaultBindings()).isEmpty());
}

void M10SettingsTests::editsServersAndPreservesUnknownNbt()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = temporaryDirectory.filePath(QStringLiteral("servers.dat"));

    auto server = std::make_shared<NbtCompound>();
    server->insert(QStringLiteral("name"), {8, QStringLiteral("Old name")});
    server->insert(QStringLiteral("ip"), {8, QStringLiteral("old.example")});
    server->insert(QStringLiteral("acceptTextures"), {1, qint8(1)});
    server->insert(QStringLiteral("futureServerData"), {3, qint32(42)});
    server->insert(QStringLiteral("futureFloatData"), {5, 3.25F});
    server->insert(QStringLiteral("futureDoubleData"), {6, 6.5});
    auto servers = std::make_shared<NbtList>();
    servers->elementType = 10;
    servers->elements.append({10, server});
    NbtDocument document;
    document.rootName = QStringLiteral("root-name");
    document.root.insert(QStringLiteral("servers"), {9, servers});
    const QString futureText = QStringLiteral("keep-me") + QChar(0) + QString::fromUtf8("日本🌍");
    document.root.insert(QStringLiteral("futureRootData"), {8, futureText});
    NbtFile::write(path, document, true);

    ServerListStore store(path);
    store.update(0, QStringLiteral("New name"), QStringLiteral("new.example"));
    store.add(QStringLiteral("Second"), QStringLiteral("second.example"));
    store.save();

    const ServerListStore reloaded(path);
    const QVector<ServerListStore::Server> expectedServers{
        {QStringLiteral("New name"), QStringLiteral("new.example")},
        {QStringLiteral("Second"), QStringLiteral("second.example")}};
    QCOMPARE(reloaded.servers(), expectedServers);
    const NbtDocument saved = NbtFile::read(path);
    QCOMPARE(std::get<QString>(saved.root.find(QStringLiteral("futureRootData"))->value),
             futureText);
    const auto* savedList =
        std::get<std::shared_ptr<NbtList>>(saved.root.find(QStringLiteral("servers"))->value).get();
    const auto* savedServer =
        std::get<std::shared_ptr<NbtCompound>>(savedList->elements.constFirst().value).get();
    QCOMPARE(std::get<qint8>(savedServer->find(QStringLiteral("acceptTextures"))->value), qint8(1));
    QCOMPARE(std::get<qint32>(savedServer->find(QStringLiteral("futureServerData"))->value),
             qint32(42));
    QCOMPARE(std::get<float>(savedServer->find(QStringLiteral("futureFloatData"))->value), 3.25F);
    QCOMPARE(std::get<double>(savedServer->find(QStringLiteral("futureDoubleData"))->value), 6.5);
    QCOMPARE(saved.rootName, QStringLiteral("root-name"));
}

void M10SettingsTests::migratesServerCompressionForMinecraft263()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString path = temporaryDirectory.filePath(QStringLiteral("servers.dat"));
    ServerListStore initial(path, QStringLiteral("1.21.1"));
    initial.add(QStringLiteral("Legacy"), QStringLiteral("legacy.example"));
    initial.save();
    QVERIFY(NbtFile::isCompressed(path));

    ServerListStore migrated(path, QStringLiteral("26.3"));
    QVERIFY(!NbtFile::isCompressed(path));
    QCOMPARE(migrated.servers().size(), 1);
    migrated.add(QStringLiteral("New"), QStringLiteral("new.example"));
    migrated.save();
    QVERIFY(!NbtFile::isCompressed(path));
}

void M10SettingsTests::refusesMalformedServerListsAndInvalidUpdates()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString malformedPath = temporaryDirectory.filePath(QStringLiteral("bad-servers.dat"));
    NbtDocument malformed;
    malformed.root.insert(QStringLiteral("servers"), {8, QStringLiteral("not a list")});
    NbtFile::write(malformedPath, malformed, true);
    QVERIFY_EXCEPTION_THROWN(([&] { ServerListStore store(malformedPath); }()), std::runtime_error);

    const QString stablePath = temporaryDirectory.filePath(QStringLiteral("stable.dat"));
    ServerListStore store(stablePath);
    store.add(QStringLiteral("Original"), QStringLiteral("original.example"));
    QVERIFY_EXCEPTION_THROWN(store.update(0, QStringLiteral("Changed"), QStringLiteral(" ")),
                             std::invalid_argument);
    QCOMPARE(
        store.servers().constFirst(),
        (ServerListStore::Server{QStringLiteral("Original"), QStringLiteral("original.example")}));
    QVERIFY_EXCEPTION_THROWN(store.remove(1), std::out_of_range);
}

void M10SettingsTests::loadsAndSavesSettingsOffTheGuiThread()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    strata::InstanceManager manager(temporaryDirectory.path());
    const strata::Instance instance =
        manager.create(QStringLiteral("Settings"), QStringLiteral("26.3"));
    QMutex instanceMutex;
    const QList<strata::Instance> instances{instance};
    strata::ui::InstanceSettingsController controller(instanceMutex, instances);

    controller.saveOption(QStringLiteral("maxFps"), QStringLiteral("120"));
    QVERIFY(controller.status().contains(QStringLiteral("Select an instance")));
    controller.loadInstance(instance.id);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    QCOMPARE(controller.instanceName(), QStringLiteral("Settings"));
    QCOMPARE(controller.commonOptions().value(QStringLiteral("fov")).toString(),
             QStringLiteral("70"));
    QVERIFY(!controller.controls().isEmpty());

    QVariantMap changes = controller.commonOptions();
    changes.insert(QStringLiteral("fov"), 90);
    changes.insert(QStringLiteral("gamma"), 0.5);
    controller.saveCommonOptions(changes);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    QCOMPARE(
        OptionsFile::read(QDir(instance.gameDirectory()).filePath(QStringLiteral("options.txt")))
            .value(QStringLiteral("fov")),
        QStringLiteral("0.5"));

    const QString optionsPath =
        QDir(instance.gameDirectory()).filePath(QStringLiteral("options.txt"));
    OptionsFile::set(optionsPath, QStringLiteral("future_option"), QStringLiteral("kept"));
    controller.saveControls(
        {{QStringLiteral("key_key.forward"), QStringLiteral("key.keyboard.up")}});
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    const QMap<QString, QString> savedOptions = OptionsFile::read(optionsPath);
    QCOMPARE(savedOptions.value(QStringLiteral("key_key.forward")),
             QStringLiteral("key.keyboard.up"));
    QCOMPARE(savedOptions.value(QStringLiteral("future_option")), QStringLiteral("kept"));

    controller.saveJavaFlags(true, QStringLiteral("-Xmx3G\n-XX:+UseG1GC"));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    const strata::InstanceSettings settings(instance.directory);
    QVERIFY(settings.hasJavaLaunchFlagsOverride());
    QCOMPARE(settings.javaLaunchFlagsOverride(),
             QStringList({QStringLiteral("-Xmx3G"), QStringLiteral("-XX:+UseG1GC")}));

    controller.addServer(QStringLiteral("Test server"), QStringLiteral("test.example"));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
    QCOMPARE(controller.servers().size(), 1);
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    M10SettingsTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "M10SettingsTests.moc"
