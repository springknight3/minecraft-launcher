#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "M4InstallTests.h"
#include "download/DownloadManager.h"
#include "instance/InstanceManager.h"
#include "mojang/MinecraftInstaller.h"
#include "mojang/MojangManifestClient.h"

#include <algorithm>
#include <atomic>
#include <future>
#include <mutex>

using namespace strata;
using namespace strata::download;
using namespace strata::mojang;

class HttpTestServer : public QObject
{
public:
    struct Response
    {
        int status = 200;
        QByteArray body;
    };

    explicit HttpTestServer(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this,
                [this]()
                {
                    while (m_server.hasPendingConnections())
                    {
                        QTcpSocket* socket = m_server.nextPendingConnection();
                        connect(socket, &QTcpSocket::readyRead, socket,
                                [this, socket]()
                                {
                                    if (socket->property("handled").toBool())
                                    {
                                        socket->readAll();
                                        return;
                                    }
                                    const QByteArray request = socket->readAll();
                                    if (!request.contains("\r\n\r\n"))
                                    {
                                        return;
                                    }
                                    socket->setProperty("handled", true);
                                    const QList<QByteArray> requestLine =
                                        request.split('\n').value(0).trimmed().split(' ');
                                    const QString path = requestLine.size() > 1
                                                             ? QString::fromUtf8(requestLine.at(1))
                                                             : QStringLiteral("/");
                                    ++m_requestCounts[path];
                                    const Response response =
                                        respond(path, m_requestCounts.value(path));
                                    ++m_activeRequests;
                                    m_maxActiveRequests =
                                        std::max(m_maxActiveRequests, m_activeRequests);
                                    QTimer::singleShot(
                                        50, socket,
                                        [this, socket, response]()
                                        {
                                            const QByteArray reason =
                                                response.status == 200
                                                    ? QByteArrayLiteral("OK")
                                                    : QByteArrayLiteral("Unavailable");
                                            socket->write(
                                                "HTTP/1.1 " + QByteArray::number(response.status) +
                                                " " + reason +
                                                "\r\nContent-Type: "
                                                "application/octet-stream\r\nContent-Length: " +
                                                QByteArray::number(response.body.size()) +
                                                "\r\nConnection: close\r\n\r\n" + response.body);
                                            socket->disconnectFromHost();
                                            --m_activeRequests;
                                        });
                                });
                    }
                });
    }

    bool start()
    {
        return m_server.listen(QHostAddress::LocalHost, 0);
    }

    QUrl url(const QString& path) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(path));
    }

    int requestCount(const QString& path) const
    {
        return m_requestCounts.value(path);
    }

    int maxActiveRequests() const
    {
        return m_maxActiveRequests;
    }

    QByteArray versionMetadata() const
    {
        const QJsonObject client{{QStringLiteral("url"), url(QStringLiteral("/client")).toString()},
                                 {QStringLiteral("sha1"),
                                  QString::fromLatin1(sha1(QByteArrayLiteral("verified payload")))},
                                 {QStringLiteral("size"), 16}};
        return QJsonDocument(QJsonObject{{QStringLiteral("downloads"),
                                          QJsonObject{{QStringLiteral("client"), client}}}})
            .toJson(QJsonDocument::Compact);
    }

    static QByteArray manifestJson()
    {
        return R"({
            "latest":{"release":"1.21.1","snapshot":"24w33a"},
            "versions":[
                {"id":"1.21.1","type":"release","url":"https://example.test/version.json",
                 "releaseTime":"2024-08-08T12:00:00+00:00",
                 "sha1":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
                {"id":"24w33a","type":"snapshot","url":"https://example.test/snapshot.json",
                 "releaseTime":"2024-08-14T12:00:00+00:00"},
                {"id":"1.41.2 Pre-Release 4","type":"release","url":"https://example.test/prerelease.json",
                 "releaseTime":"2024-08-14T12:00:00+00:00"}
            ]
        })";
    }

private:
    QTcpServer m_server;
    QHash<QString, int> m_requestCounts;
    int m_activeRequests = 0;
    int m_maxActiveRequests = 0;

    Response respond(const QString& path, int requestNumber) const
    {
        if (path == QStringLiteral("/retry") && requestNumber == 1)
        {
            return {503, {}};
        }
        if (path == QStringLiteral("/bad"))
        {
            return {200, QByteArrayLiteral("bad checksum")};
        }
        if (path == QStringLiteral("/manifest"))
        {
            return {200, manifestJson()};
        }
        if (path == QStringLiteral("/version"))
        {
            return {200, versionMetadata()};
        }
        return {200, QByteArrayLiteral("verified payload")};
    }

    static QByteArray sha1(const QByteArray& value)
    {
        return QCryptographicHash::hash(value, QCryptographicHash::Sha1).toHex();
    }
};

QByteArray M4InstallTests::sha1(const QByteArray& value)
{
    return QCryptographicHash::hash(value, QCryptographicHash::Sha1).toHex();
}

void M4InstallTests::waitFor(std::future<void>& future)
{
    while (future.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    future.get();
}

void M4InstallTests::parsesManifestAndLatestRelease()
{
    const VersionManifest manifest =
        MojangManifestClient::parseManifest(HttpTestServer::manifestJson());
    QCOMPARE(manifest.latestRelease, QStringLiteral("1.21.1"));
    QCOMPARE(manifest.latestSnapshot, QStringLiteral("24w33a"));
    QCOMPARE(manifest.latestReleaseVersion().id, QStringLiteral("1.21.1"));
    QCOMPARE(manifest.versions.size(), 3);
    QCOMPARE(manifest.versions.at(0).sha1,
             QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    QCOMPARE(manifest.versions.at(2).id, QStringLiteral("1.41.2 Pre-Release 4"));
    QVERIFY_EXCEPTION_THROWN(MojangManifestClient::parseManifest(QByteArrayLiteral("{")),
                             std::runtime_error);
    QByteArray traversalManifest = HttpTestServer::manifestJson();
    traversalManifest.replace("\"1.21.1\"", "\"../outside\"");
    QVERIFY_EXCEPTION_THROWN(MojangManifestClient::parseManifest(traversalManifest),
                             std::runtime_error);
}

void M4InstallTests::atomicallyCachesManifestAndUsesItWhenOffline()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    HttpTestServer server;
    QVERIFY(server.start());
    const QString cachePath =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("cache/manifest.json"));
    MojangManifestClient online(cachePath, server.url(QStringLiteral("/manifest")));
    const VersionManifest fetched = online.fetchManifest();
    QVERIFY(!fetched.usedCache);
    QVERIFY(QFileInfo(cachePath).isFile());

    MojangManifestClient offline(cachePath, QUrl(QStringLiteral("http://127.0.0.1:1/unreachable")));
    const VersionManifest cached = offline.fetchManifest();
    QVERIFY(cached.usedCache);
    QCOMPARE(cached.latestRelease, fetched.latestRelease);

    MojangManifestClient noCache(
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("missing.json")),
        QUrl(QStringLiteral("http://127.0.0.1:1/unreachable")));
    QVERIFY_EXCEPTION_THROWN(noCache.fetchManifest(), std::runtime_error);
}

void M4InstallTests::downloadsConcurrentlyRetriesAndVerifiesChecksums()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    HttpTestServer server;
    QVERIFY(server.start());
    DownloadManager manager(3, 3);
    const QByteArray payload = QByteArrayLiteral("verified payload");
    const QString checksum = QString::fromLatin1(sha1(payload));
    const QList<DownloadTask> tasks{
        {server.url(QStringLiteral("/retry")),
         QDir(temporaryDirectory.path()).filePath(QStringLiteral("retry.jar")), checksum,
         QStringLiteral("retry artifact")},
        {server.url(QStringLiteral("/one")),
         QDir(temporaryDirectory.path()).filePath(QStringLiteral("one.jar")), checksum,
         QStringLiteral("artifact one")},
        {server.url(QStringLiteral("/two")),
         QDir(temporaryDirectory.path()).filePath(QStringLiteral("two.jar")), checksum,
         QStringLiteral("artifact two")}};
    std::mutex progressMutex;
    QList<Progress> progress;
    auto future = std::async(std::launch::async,
                             [&]()
                             {
                                 manager.downloadAll(tasks,
                                                     [&](const Progress& update)
                                                     {
                                                         std::lock_guard lock(progressMutex);
                                                         progress.append(update);
                                                     });
                             });
    waitFor(future);

    QCOMPARE(server.requestCount(QStringLiteral("/retry")), 2);
    QVERIFY(server.maxActiveRequests() >= 2);
    for (const DownloadTask& task : tasks)
    {
        QFile downloaded(task.destination);
        QVERIFY(downloaded.open(QIODevice::ReadOnly));
        QCOMPARE(downloaded.readAll(), payload);
    }
    QVERIFY(std::any_of(progress.cbegin(), progress.cend(), [](const Progress& update)
                        { return update.completed == 3 && update.total == 3; }));
}

void M4InstallTests::rejectsChecksumMismatchWithoutPartialFile()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    HttpTestServer server;
    QVERIFY(server.start());
    const QString destination =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("bad/file.jar"));
    DownloadManager manager(1, 2);
    const QList<DownloadTask> tasks{{server.url(QStringLiteral("/bad")), destination,
                                     QStringLiteral("0000000000000000000000000000000000000000"),
                                     QStringLiteral("bad artifact")}};
    auto future = std::async(std::launch::async, [&]() { manager.downloadAll(tasks); });
    QVERIFY_EXCEPTION_THROWN(waitFor(future), std::runtime_error);
    QVERIFY(!QFileInfo::exists(destination));
    const QDir outputDirectory(QFileInfo(destination).absolutePath());
    QVERIFY(outputDirectory.exists());
    QCOMPARE(outputDirectory.entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot).size(),
             0);
}

void M4InstallTests::installsMinecraftMetadataAndClientJar()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    HttpTestServer server;
    QVERIFY(server.start());
    InstanceManager instanceManager(
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("data")));
    const Instance instance =
        instanceManager.create(QStringLiteral("Install test"), QStringLiteral("1.21.1"));
    const QByteArray metadata = server.versionMetadata();
    const QString metadataChecksum = QString::fromLatin1(sha1(metadata));
    DownloadManager downloads(2, 2);
    MinecraftInstaller installer(downloads);
    const Version version{QStringLiteral("1.21.1"), QStringLiteral("release"),
                          server.url(QStringLiteral("/version")), QDateTime::currentDateTimeUtc(),
                          metadataChecksum};
    QList<Progress> progress;
    auto future = std::async(std::launch::async,
                             [&]()
                             {
                                 installer.install(instance, version, [&](const Progress& update)
                                                   { progress.append(update); });
                             });
    waitFor(future);

    const QString installedDirectory =
        QDir(instance.gameDirectory()).filePath(QStringLiteral("versions/1.21.1"));
    QFile installedMetadata(QDir(installedDirectory).filePath(QStringLiteral("1.21.1.json")));
    QFile installedClient(QDir(installedDirectory).filePath(QStringLiteral("1.21.1.jar")));
    QVERIFY(installedMetadata.open(QIODevice::ReadOnly));
    QVERIFY(installedClient.open(QIODevice::ReadOnly));
    QCOMPARE(installedClient.readAll(), QByteArrayLiteral("verified payload"));
    QVERIFY(std::any_of(progress.cbegin(), progress.cend(), [](const Progress& update)
                        { return update.completed == 2 && update.total == 2; }));
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    M4InstallTests tests;
    return QTest::qExec(&tests, argc, argv);
}
