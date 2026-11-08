#include "M9ModrinthTests.h"

#include "download/DownloadManager.h"
#include "instance/Instance.h"
#include "loader/LoaderInstaller.h"
#include "modrinth/ContentManager.h"
#include "modrinth/ModrinthClient.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

#include <stdexcept>

namespace
{
    class ModrinthTestServer : public QObject
    {
    public:
        explicit ModrinthTestServer(QObject* parent = nullptr) : QObject(parent)
        {
            connect(&m_server, &QTcpServer::newConnection, this,
                    [this]()
                    {
                        QTcpSocket* socket = m_server.nextPendingConnection();
                        connect(
                            socket, &QTcpSocket::readyRead, socket,
                            [this, socket]()
                            {
                                const QByteArray request = socket->readAll();
                                if (!request.contains("\r\n\r\n"))
                                {
                                    return;
                                }
                                const QByteArray requestLine = request.split('\n').value(0);
                                m_requestTarget = requestLine.trimmed().split(' ').value(1);
                                const QString path =
                                    QUrl::fromEncoded("http://127.0.0.1" + m_requestTarget).path();
                                const QByteArray body = m_responses.value(
                                    path, QByteArrayLiteral("{\"hits\":[],\"total_hits\":37}"));
                                socket->write("HTTP/1.1 200 OK\r\nContent-Type: "
                                              "application/json\r\nContent-Length: " +
                                              QByteArray::number(body.size()) +
                                              "\r\nConnection: close\r\n\r\n" + body);
                                socket->disconnectFromHost();
                            });
                    });
        }

        bool start()
        {
            return m_server.listen(QHostAddress::LocalHost, 0);
        }

        QUrl apiRoot() const
        {
            return QUrl(QStringLiteral("http://127.0.0.1:%1/v2/").arg(m_server.serverPort()));
        }

        QUrl requestUrl() const
        {
            return QUrl::fromEncoded("http://127.0.0.1" + m_requestTarget);
        }

        void setResponse(const QString& path, const QByteArray& body)
        {
            m_responses.insert(path, body);
        }

    private:
        QTcpServer m_server;
        QByteArray m_requestTarget;
        QHash<QString, QByteArray> m_responses;
    };

    strata::Instance makeInstance(const QString& root)
    {
        strata::Instance instance;
        instance.id = QStringLiteral("test-instance");
        instance.name = QStringLiteral("Test instance");
        instance.versionId = QStringLiteral("1.21.1");
        instance.directory = root;
        return instance;
    }

    bool writeFile(const QString& path, const QByteArray& contents)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
    }
} // namespace

void M9ModrinthTests::contentTypesMapToCompatibleDirectories()
{
    using namespace strata::modrinth;

    QCOMPARE(projectType(ContentType::Mod), QStringLiteral("mod"));
    QCOMPARE(directoryName(ContentType::ResourcePack), QStringLiteral("resourcepacks"));
    QCOMPARE(directoryName(ContentType::Shader), QStringLiteral("shaderpacks"));
    QCOMPARE(parseContentType(QStringLiteral("DATA_PACK")), ContentType::DataPack);

    const strata::Instance instance = makeInstance(QStringLiteral("/tmp/instance"));
    QCOMPARE(
        ContentManager::targetDirectory(instance, ContentType::DataPack, QStringLiteral("World 1")),
        QStringLiteral("/tmp/instance/game/saves/World 1/datapacks"));
    QVERIFY_EXCEPTION_THROWN(ContentManager::targetDirectory(instance, ContentType::DataPack,
                                                             QStringLiteral("../escape")),
                             std::runtime_error);
}

void M9ModrinthTests::modSearchAppliesMinecraftLoaderAndPaginationFilters()
{
    ModrinthTestServer server;
    QVERIFY(server.start());
    strata::modrinth::ModrinthClient client(QStringLiteral("StrataLauncher"),
                                            QStringLiteral("0.1.0"), server.apiRoot());

    const strata::modrinth::SearchPage page =
        client.search(strata::modrinth::ContentType::Mod, QStringLiteral("map"),
                      QStringLiteral("1.21.1"), strata::loader::LoaderType::Fabric, 20, 20);
    QCOMPARE(page.total, 37);
    QCOMPARE(page.offset, 20);
    QCOMPARE(page.limit, 20);

    const QUrlQuery query(server.requestUrl());
    QCOMPARE(query.queryItemValue(QStringLiteral("query")), QStringLiteral("map"));
    QCOMPARE(query.queryItemValue(QStringLiteral("offset")), QStringLiteral("20"));
    QCOMPARE(query.queryItemValue(QStringLiteral("limit")), QStringLiteral("20"));
    QCOMPARE(query.queryItemValue(QStringLiteral("index")), QStringLiteral("downloads"));
    const QJsonDocument facets =
        QJsonDocument::fromJson(query.queryItemValue(QStringLiteral("facets")).toUtf8());
    QVERIFY(facets.isArray());
    QCOMPARE(facets.array().size(), 3);
    QCOMPARE(facets.array().at(0).toArray().at(0).toString(), QStringLiteral("project_type:mod"));
    QCOMPARE(facets.array().at(1).toArray().at(0).toString(), QStringLiteral("versions:1.21.1"));
    QCOMPARE(facets.array().at(2).toArray().at(0).toString(), QStringLiteral("categories:fabric"));
}

void M9ModrinthTests::parsesVersionsAndRejectsUnsafeDownloads()
{
    using namespace strata::modrinth;

    const QJsonObject file{
        {QStringLiteral("filename"), QStringLiteral("example.jar")},
        {QStringLiteral("url"), QStringLiteral("https://cdn.modrinth.com/data/a/example.jar")},
        {QStringLiteral("primary"), true},
        {QStringLiteral("size"), 16},
        {QStringLiteral("hashes"),
         QJsonObject{{QStringLiteral("sha1"),
                      QStringLiteral("0123456789012345678901234567890123456789")}}}};
    const QJsonObject versionJson{
        {QStringLiteral("id"), QStringLiteral("version_1")},
        {QStringLiteral("project_id"), QStringLiteral("project_1")},
        {QStringLiteral("version_number"), QStringLiteral("1.0")},
        {QStringLiteral("game_versions"), QJsonArray{QStringLiteral("1.21.1")}},
        {QStringLiteral("loaders"), QJsonArray{QStringLiteral("fabric")}},
        {QStringLiteral("files"), QJsonArray{file}},
        {QStringLiteral("dependencies"),
         QJsonArray{QJsonObject{{QStringLiteral("project_id"), QStringLiteral("dep_1")},
                                {QStringLiteral("dependency_type"), QStringLiteral("required")}}}}};

    const ProjectVersion version = ModrinthClient::parseVersion(versionJson, QStringLiteral("mod"));
    QCOMPARE(version.primaryFile().filename, QStringLiteral("example.jar"));
    QCOMPARE(version.dependencies.size(), 1);
    QCOMPARE(version.dependencies.constFirst().projectId, QStringLiteral("dep_1"));
    QVERIFY_EXCEPTION_THROWN(
        ModrinthClient::validateFile(
            {QStringLiteral("../example.jar"), QStringLiteral("https://cdn.modrinth.com/file"),
             QStringLiteral("0123456789012345678901234567890123456789"), true, 16}),
        std::runtime_error);
    QVERIFY_EXCEPTION_THROWN(
        ModrinthClient::validateFile(
            {QStringLiteral("example.jar"), QStringLiteral("http://cdn.modrinth.com/file"),
             QStringLiteral("0123456789012345678901234567890123456789"), true, 16}),
        std::runtime_error);
}

void M9ModrinthTests::resolvesNestedRequiredDependenciesInInstallOrder()
{
    ModrinthTestServer server;
    QVERIFY(server.start());
    const auto projectJson = [](const QString& id, const QString& title)
    {
        return QJsonDocument(QJsonObject{{QStringLiteral("id"), id},
                                         {QStringLiteral("slug"), id},
                                         {QStringLiteral("title"), title},
                                         {QStringLiteral("project_type"), QStringLiteral("mod")}})
            .toJson(QJsonDocument::Compact);
    };
    const auto versionJson = [](const QString& id, const QString& projectId,
                                const QString& versionNumber, const QJsonArray& dependencies)
    {
        const QJsonObject file{
            {QStringLiteral("filename"), id + QStringLiteral(".jar")},
            {QStringLiteral("url"), QStringLiteral("https://cdn.modrinth.com/%1.jar").arg(id)},
            {QStringLiteral("primary"), true},
            {QStringLiteral("hashes"),
             QJsonObject{{QStringLiteral("sha1"),
                          QStringLiteral("0123456789012345678901234567890123456789")}}}};
        return QJsonDocument(QJsonObject{{QStringLiteral("id"), id},
                                         {QStringLiteral("project_id"), projectId},
                                         {QStringLiteral("version_number"), versionNumber},
                                         {QStringLiteral("game_versions"), QJsonArray{"1.21.1"}},
                                         {QStringLiteral("loaders"), QJsonArray{"fabric"}},
                                         {QStringLiteral("files"), QJsonArray{file}},
                                         {QStringLiteral("dependencies"), dependencies}})
            .toJson(QJsonDocument::Compact);
    };

    server.setResponse(QStringLiteral("/v2/project/dep_1"),
                       projectJson(QStringLiteral("dep_1"), QStringLiteral("Dependency One")));
    server.setResponse(
        QStringLiteral("/v2/version/dep_v1"),
        versionJson(QStringLiteral("dep_v1"), QStringLiteral("dep_1"), QStringLiteral("1.0"),
                    QJsonArray{QJsonObject{
                        {QStringLiteral("project_id"), QStringLiteral("dep_2")},
                        {QStringLiteral("dependency_type"), QStringLiteral("required")}}}));
    server.setResponse(QStringLiteral("/v2/project/dep_2"),
                       projectJson(QStringLiteral("dep_2"), QStringLiteral("Dependency Two")));
    const QJsonObject subVersion =
        QJsonDocument::fromJson(versionJson(QStringLiteral("dep_v2"), QStringLiteral("dep_2"),
                                            QStringLiteral("2.0"), {}))
            .object();
    server.setResponse(QStringLiteral("/v2/project/dep_2/version"),
                       QJsonDocument(QJsonArray{subVersion}).toJson(QJsonDocument::Compact));

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const strata::Instance instance = makeInstance(temporaryDirectory.path());
    QVERIFY(QDir().mkpath(instance.gameDirectory()));
    strata::download::DownloadManager downloads;
    strata::modrinth::ModrinthClient client(QStringLiteral("StrataLauncher"),
                                            QStringLiteral("0.1.0"), server.apiRoot());
    strata::modrinth::ContentManager manager(client, downloads);
    const strata::modrinth::Project rootProject{QStringLiteral("root"),
                                                QStringLiteral("root"),
                                                QStringLiteral("Root mod"),
                                                {},
                                                QStringLiteral("mod"),
                                                0,
                                                {},
                                                {}};
    strata::modrinth::ProjectVersion rootVersion;
    rootVersion.id = QStringLiteral("root_v1");
    rootVersion.projectId = rootProject.id;
    rootVersion.projectType = QStringLiteral("mod");
    rootVersion.gameVersions = {instance.versionId};
    rootVersion.loaders = {QStringLiteral("fabric")};
    rootVersion.dependencies = {
        {QStringLiteral("dep_1"), QStringLiteral("dep_v1"), QStringLiteral("required")}};

    const QList<strata::modrinth::DependencyInstall> dependencies =
        manager.resolveRequiredDependencies(instance, strata::loader::LoaderType::Fabric,
                                            rootProject, rootVersion);
    QCOMPARE(dependencies.size(), 2);
    QCOMPARE(dependencies.at(0).project.id, QStringLiteral("dep_2"));
    QCOMPARE(dependencies.at(1).project.id, QStringLiteral("dep_1"));
    QCOMPARE(dependencies.at(1).version.id, QStringLiteral("dep_v1"));
}

void M9ModrinthTests::readsLegacyStateAndTracksEnableAndRemoval()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const strata::Instance instance = makeInstance(temporaryDirectory.path());
    const QString gameDirectory = instance.gameDirectory();
    QVERIFY(QDir().mkpath(QDir(gameDirectory).filePath(QStringLiteral("mods"))));
    QVERIFY(writeFile(QDir(gameDirectory).filePath(QStringLiteral("mods/example.jar")),
                      QByteArrayLiteral("content")));
    const QByteArray state = R"({
        "entries": [{
            "type": "MOD",
            "projectId": "project_1",
            "slug": "example",
            "title": "Example",
            "versionId": "version_1",
            "versionNumber": "1.0",
            "worldName": "",
            "filename": "example.jar",
            "installedAt": "2025-01-01T00:00:00Z"
        }]
    })";
    QVERIFY(writeFile(QDir(gameDirectory).filePath(QStringLiteral(".stratalauncher-content.json")),
                      state));

    strata::download::DownloadManager downloads;
    strata::modrinth::ModrinthClient client;
    strata::modrinth::ContentManager manager(client, downloads);
    QList<strata::modrinth::InstalledContent> installed =
        manager.installed(instance, strata::modrinth::ContentType::Mod);
    QCOMPARE(installed.size(), 1);
    QVERIFY(installed.constFirst().enabled);

    manager.setEnabled(instance, installed.constFirst(), false);
    QVERIFY(QFileInfo::exists(
        QDir(gameDirectory).filePath(QStringLiteral("mods/example.jar.disabled"))));
    installed = manager.installed(instance, strata::modrinth::ContentType::Mod);
    QVERIFY(!installed.constFirst().enabled);
    manager.remove(instance, installed.constFirst());
    QVERIFY(manager.installed(instance, strata::modrinth::ContentType::Mod).isEmpty());
    QVERIFY(!QFileInfo::exists(
        QDir(gameDirectory).filePath(QStringLiteral("mods/example.jar.disabled"))));
}

void M9ModrinthTests::requiredDependenciesBlockInstallation()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const strata::Instance instance = makeInstance(temporaryDirectory.path());
    QVERIFY(QDir().mkpath(instance.gameDirectory()));

    strata::download::DownloadManager downloads;
    strata::modrinth::ModrinthClient client;
    strata::modrinth::ContentManager manager(client, downloads);
    const strata::modrinth::Project project{QStringLiteral("project_1"),
                                            QStringLiteral("example"),
                                            QStringLiteral("Example"),
                                            {},
                                            QStringLiteral("resourcepack"),
                                            0,
                                            {},
                                            {}};
    strata::modrinth::ProjectVersion version;
    version.id = QStringLiteral("version_1");
    version.projectId = project.id;
    version.versionNumber = QStringLiteral("1.0");
    version.projectType = QStringLiteral("resourcepack");
    version.gameVersions = {instance.versionId};
    version.dependencies = {{QStringLiteral("required_project"), {}, QStringLiteral("required")}};

    QVERIFY_EXCEPTION_THROWN(manager.install(instance, strata::modrinth::ContentType::ResourcePack,
                                             strata::loader::LoaderType::Vanilla, {}, project,
                                             version),
                             std::runtime_error);

    const QByteArray installedDependency = R"({
        "entries": [{
            "type": "MOD",
            "projectId": "required_project",
            "slug": "dependency",
            "title": "Dependency",
            "versionId": "dependency_version",
            "versionNumber": "1.0",
            "worldName": "",
            "filename": "dependency.jar",
            "installedAt": "2025-01-01T00:00:00Z",
            "enabled": true
        }]
    })";
    QVERIFY(writeFile(
        QDir(instance.gameDirectory()).filePath(QStringLiteral(".stratalauncher-content.json")),
        installedDependency));
    version.dependencies = {
        {QStringLiteral("required_project"), {}, QStringLiteral("incompatible")}};
    QVERIFY_EXCEPTION_THROWN(manager.install(instance, strata::modrinth::ContentType::ResourcePack,
                                             strata::loader::LoaderType::Vanilla, {}, project,
                                             version),
                             std::runtime_error);
}

QTEST_GUILESS_MAIN(M9ModrinthTests)
