#include "DevServerManager.h"

#include "../settings/InstanceSettings.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <stdexcept>

namespace strata::server
{
    namespace
    {
        constexpr qint64 maxPropertiesBytes = 1'000'000;
        constexpr qint64 maxMetadataBytes = 16 * 1024 * 1024;
        constexpr qint64 maxInstallerLogBytes = 64 * 1024;
        constexpr int networkTimeoutMs = 30'000;

        [[noreturn]] void throwServerError(const QString& message)
        {
            throw std::runtime_error(message.toStdString());
        }

        bool pathIsWithin(const QString& root, const QString& path)
        {
            const QString absoluteRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
            const QString absolutePath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
            return absolutePath.startsWith(absoluteRoot + QDir::separator());
        }

        void ensureDirectory(const QString& path)
        {
            if (QFileInfo(path).isSymLink())
            {
                throwServerError(
                    QStringLiteral("Refusing to use a symbolic link as a server directory."));
            }
            if (!QDir().mkpath(path))
            {
                throwServerError(QStringLiteral("Unable to create server directory: %1").arg(path));
            }
        }

        QString requiredString(const QJsonObject& object, const QString& key)
        {
            const QJsonValue value = object.value(key);
            if (!value.isString() || value.toString().isEmpty())
            {
                throwServerError(QStringLiteral("Server configuration is missing %1.").arg(key));
            }
            return value.toString();
        }

        QString requiredJsonString(const QJsonObject& object, const QString& key)
        {
            const QJsonValue value = object.value(key);
            if (!value.isString() || value.toString().isEmpty())
            {
                throwServerError(QStringLiteral("Server metadata is missing %1.").arg(key));
            }
            return value.toString();
        }

        bool allowedHttpsUrl(const QUrl& url, const QStringList& hosts)
        {
            return url.isValid() && url.scheme() == QStringLiteral("https") &&
                   url.userInfo().isEmpty() && (url.port() == -1 || url.port() == 443) &&
                   hosts.contains(url.host(), Qt::CaseInsensitive);
        }

        QByteArray fetchBytes(const QUrl& url, qint64 maxBytes, const QStringList& hosts)
        {
            if (!allowedHttpsUrl(url, hosts))
            {
                throwServerError(
                    QStringLiteral("Refusing to fetch an untrusted server metadata URL."));
            }

            QNetworkAccessManager manager;
            QNetworkRequest request(url);
            request.setHeader(QNetworkRequest::UserAgentHeader,
                              QStringLiteral("StrataLauncher/0.1.0"));
            request.setRawHeader("Accept", "application/json, text/plain;q=0.9");
            request.setTransferTimeout(networkTimeoutMs);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);
            QNetworkReply* reply = manager.get(request);
            QByteArray response;
            bool tooLarge = false;
            QObject::connect(reply, &QIODevice::readyRead, reply,
                             [&]()
                             {
                                 const QByteArray chunk = reply->readAll();
                                 if (chunk.size() > maxBytes - response.size())
                                 {
                                     tooLarge = true;
                                     reply->abort();
                                     return;
                                 }
                                 response.append(chunk);
                             });
            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
            timeout.start(networkTimeoutMs);
            loop.exec();
            if (!tooLarge)
            {
                const QByteArray tail = reply->readAll();
                if (tail.size() > maxBytes - response.size())
                {
                    tooLarge = true;
                }
                else
                {
                    response.append(tail);
                }
            }
            const QUrl finalUrl = reply->url();
            const auto networkError = reply->error();
            const QString error = reply->errorString();
            reply->deleteLater();
            if (tooLarge)
            {
                throwServerError(
                    QStringLiteral("Server metadata exceeds the supported size limit."));
            }
            if (!allowedHttpsUrl(finalUrl, hosts))
            {
                throwServerError(
                    QStringLiteral("Server metadata redirected to an untrusted host."));
            }
            if (networkError != QNetworkReply::NoError)
            {
                throwServerError(QStringLiteral("Unable to fetch server metadata: %1").arg(error));
            }
            return response;
        }

        QJsonObject parseJsonObject(const QByteArray& bytes, const QString& description)
        {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
            {
                throwServerError(QStringLiteral("%1 is invalid JSON: %2")
                                     .arg(description, parseError.errorString()));
            }
            return document.object();
        }

        QJsonArray parseJsonArray(const QByteArray& bytes, const QString& description)
        {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isArray())
            {
                throwServerError(QStringLiteral("%1 is invalid JSON: %2")
                                     .arg(description, parseError.errorString()));
            }
            return document.array();
        }

        QString sha256ForFile(const QString& path)
        {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                throwServerError(
                    QStringLiteral("Unable to read server JAR for integrity verification: %1")
                        .arg(file.errorString()));
            }
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (!hash.addData(&file))
            {
                throwServerError(QStringLiteral("Unable to hash the downloaded server JAR."));
            }
            return QString::fromLatin1(hash.result().toHex());
        }

        QString safeEncodedPathSegment(const QString& segment)
        {
            if (segment.isEmpty() || segment == QStringLiteral(".") ||
                segment == QStringLiteral("..") || segment.contains(QLatin1Char('/')) ||
                segment.contains(QLatin1Char('\\')))
            {
                throwServerError(QStringLiteral("Server version contains an unsafe path segment."));
            }
            return QString::fromLatin1(QUrl::toPercentEncoding(segment));
        }

        QString javaExecutableName()
        {
#ifdef Q_OS_WIN
            return QStringLiteral("java.exe");
#else
            return QStringLiteral("java");
#endif
        }

        bool optionalBoolean(const QJsonObject& object, const QString& key, bool fallback)
        {
            const QJsonValue value = object.value(key);
            return value.isBool() ? value.toBool() : fallback;
        }

        int optionalInteger(const QJsonObject& object, const QString& key, int fallback)
        {
            const QJsonValue value = object.value(key);
            return value.isDouble() ? value.toInt(fallback) : fallback;
        }

        QString updatePropertyLines(const QString& current, const QString& key,
                                    const QString& value)
        {
            static const QRegularExpression lineEndings(QStringLiteral("\\r\\n|\\n|\\r"));
            const QStringList lines = current.split(lineEndings);
            QStringList updated;
            updated.reserve(lines.size() + 1);
            bool found = false;
            for (const QString& line : lines)
            {
                if (line.startsWith(key + QLatin1Char('=')))
                {
                    updated.append(key + QLatin1Char('=') + value);
                    found = true;
                }
                else
                {
                    updated.append(line);
                }
            }
            while (!updated.isEmpty() && updated.constLast().isEmpty())
            {
                updated.removeLast();
            }
            if (!found)
            {
                updated.append(key + QLatin1Char('=') + value);
            }
            const qsizetype crlf = current.indexOf(QStringLiteral("\r\n"));
            const qsizetype lf = current.indexOf(QLatin1Char('\n'));
            const qsizetype cr = current.indexOf(QLatin1Char('\r'));
            QString newline = QStringLiteral("\n");
            if (crlf >= 0 && (lf < 0 || crlf <= lf))
            {
                newline = QStringLiteral("\r\n");
            }
            else if (cr >= 0 && (lf < 0 || cr < lf))
            {
                newline = QStringLiteral("\r");
            }
            return updated.join(newline) + newline;
        }
    } // namespace

    DevServerManager::DevServerManager(QString dataDirectory,
                                       const download::DownloadManager& downloadManager,
                                       const mojang::MojangManifestClient& manifestClient,
                                       runtime::JavaRuntimeManager* javaRuntimeManager)
        : m_dataDirectory(QFileInfo(dataDirectory).absoluteFilePath()),
          m_serversDirectory(QDir(m_dataDirectory).filePath(QStringLiteral("servers"))),
          m_installerCache(QDir(m_dataDirectory).filePath(QStringLiteral("server-installers"))),
          m_downloadManager(downloadManager), m_manifestClient(manifestClient),
          m_javaRuntimeManager(javaRuntimeManager)
    {
    }

    QVector<ServerConfig> DevServerManager::listServers() const
    {
        ensureDirectory(m_serversDirectory);
        QDir directory(m_serversDirectory);
        const QFileInfoList entries =
            directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        QVector<ServerConfig> servers;
        for (const QFileInfo& entry : entries)
        {
            if (entry.isSymLink())
            {
                continue;
            }
            const QString configPath =
                QDir(entry.absoluteFilePath()).filePath(QStringLiteral("server.json"));
            const QFileInfo configInfo(configPath);
            if (!configInfo.exists())
            {
                continue;
            }
            if (configInfo.isSymLink() || !configInfo.isFile())
            {
                throwServerError(
                    QStringLiteral("Refusing to read an unsafe server configuration: %1")
                        .arg(configPath));
            }
            ServerConfig config = readConfig(configPath);
            if (entry.fileName() != config.id)
            {
                throwServerError(QStringLiteral(
                    "Server configuration identifier does not match its directory."));
            }
            servers.append(config);
        }
        std::sort(servers.begin(), servers.end(),
                  [](const ServerConfig& left, const ServerConfig& right)
                  { return left.name.toCaseFolded() < right.name.toCaseFolded(); });
        return servers;
    }

    ServerConfig DevServerManager::create(const QString& name, const QString& minecraftVersion,
                                          ServerDistribution distribution) const
    {
        const QString normalizedName = requireName(name);
        const QString normalizedVersion = requireVersion(minecraftVersion);
        const QString serializedDistribution = distributionName(distribution);
        Q_UNUSED(serializedDistribution)
        ensureDirectory(m_serversDirectory);
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
        const QString root = QDir(m_serversDirectory).filePath(id);
        if (!QDir().mkdir(root))
        {
            throwServerError(
                QStringLiteral("Unable to create isolated server directory: %1").arg(root));
        }
        const ServerConfig config{id,
                                  normalizedName,
                                  normalizedVersion,
                                  distribution,
                                  false,
                                  false,
                                  {},
                                  QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
                                  10,
                                  true,
                                  false,
                                  {}};
        try
        {
            ensureDirectory(serverDirectory(config));
            writeConfig(config, root);
            writeTextAtomically(
                QDir(serverDirectory(config)).filePath(QStringLiteral("eula.txt")),
                QStringLiteral("# Accept the Minecraft EULA from the server manager before "
                               "starting this server.\neula=false\n"));
            writeServerProperties(config);
        }
        catch (...)
        {
            QDir(root).removeRecursively();
            throw;
        }
        return config;
    }

    ServerConfig DevServerManager::updateSettings(const ServerConfig& server, const QString& name,
                                                  bool eulaAccepted,
                                                  const QStringList& javaFlags) const
    {
        const ServerConfig stored = requireServer(server.id);
        return updateSettings(stored, name, eulaAccepted, javaFlags, stored.viewDistance,
                              stored.onlineMode, stored.useManagedJavaRuntime);
    }

    ServerConfig DevServerManager::updateSettings(const ServerConfig& server, const QString& name,
                                                  bool eulaAccepted, const QStringList& javaFlags,
                                                  int viewDistance, bool onlineMode,
                                                  bool useManagedJavaRuntime) const
    {
        const ServerConfig stored = requireServer(server.id);
        requireViewDistance(viewDistance);
        if (useManagedJavaRuntime && stored.javaRuntimeComponent.isEmpty())
        {
            throw std::invalid_argument(
                "Install a managed Java runtime before selecting it for this server.");
        }
        const QStringList normalizedFlags = strata::InstanceSettings::validateFlags(javaFlags);
        const ServerConfig updated{stored.id,
                                   requireName(name),
                                   stored.minecraftVersion,
                                   stored.distribution,
                                   stored.installed,
                                   eulaAccepted,
                                   normalizedFlags,
                                   stored.createdAt,
                                   viewDistance,
                                   onlineMode,
                                   useManagedJavaRuntime,
                                   useManagedJavaRuntime ? stored.javaRuntimeComponent : QString()};
        writeTextAtomically(QDir(serverDirectory(updated)).filePath(QStringLiteral("eula.txt")),
                            eulaAccepted ? QStringLiteral("eula=true\n")
                                         : QStringLiteral("eula=false\n"));
        writeServerProperties(updated);
        writeConfig(updated, serverRoot(stored.id));
        return updated;
    }

    ServerConfig DevServerManager::setEulaAccepted(const ServerConfig& server, bool accepted) const
    {
        const ServerConfig stored = requireServer(server.id);
        const ServerConfig updated{stored.id,
                                   stored.name,
                                   stored.minecraftVersion,
                                   stored.distribution,
                                   stored.installed,
                                   accepted,
                                   stored.javaFlags,
                                   stored.createdAt,
                                   stored.viewDistance,
                                   stored.onlineMode,
                                   stored.useManagedJavaRuntime,
                                   stored.javaRuntimeComponent};
        writeTextAtomically(QDir(serverDirectory(updated)).filePath(QStringLiteral("eula.txt")),
                            accepted ? QStringLiteral("eula=true\n")
                                     : QStringLiteral("eula=false\n"));
        writeConfig(updated, serverRoot(stored.id));
        return updated;
    }

    ServerConfig
    DevServerManager::install(const ServerConfig& server,
                              download::DownloadManager::ProgressCallback progress) const
    {
        const ServerConfig stored = requireServer(server.id);
        const QString directory = serverDirectory(stored);
        ensureDirectory(directory);
        const auto report = [&progress](int completed, int total, const QString& item)
        {
            if (progress)
            {
                progress({completed, total, item});
            }
        };
        report(0, 1, QStringLiteral("Resolving Minecraft %1").arg(stored.minecraftVersion));
        const QJsonObject metadata = fetchVersionMetadata(stored.minecraftVersion, progress);

        if (stored.distribution == ServerDistribution::Vanilla)
        {
            const QJsonObject downloads = metadata.value(QStringLiteral("downloads")).toObject();
            const QJsonObject artifact = downloads.value(QStringLiteral("server")).toObject();
            const QUrl url(requiredJsonString(artifact, QStringLiteral("url")));
            const QString sha1 = requiredJsonString(artifact, QStringLiteral("sha1"));
            static const QRegularExpression sha1Pattern(QStringLiteral("^[0-9a-fA-F]{40}$"));
            if (!allowedHttpsUrl(url, {QStringLiteral("piston-data.mojang.com"),
                                       QStringLiteral("launcher.mojang.com")}) ||
                !sha1Pattern.match(sha1).hasMatch())
            {
                throwServerError(QStringLiteral("Mojang server download metadata is invalid."));
            }
            const QString target = QDir(directory).filePath(QStringLiteral("server.jar"));
            m_downloadManager.downloadAll(
                {{url, target, sha1, QStringLiteral("Minecraft server JAR")}}, progress);
        }
        else if (stored.distribution == ServerDistribution::Paper)
        {
            const QUrl buildsUrl(
                QStringLiteral("https://fill.papermc.io/v3/projects/paper/versions/%1/builds")
                    .arg(safeEncodedPathSegment(stored.minecraftVersion)));
            const QJsonArray builds = parseJsonArray(
                fetchBytes(buildsUrl, maxMetadataBytes, {QStringLiteral("fill.papermc.io")}),
                QStringLiteral("Paper build list"));
            const PaperBuild build = latestStablePaperBuild(builds);
            const QString target = QDir(directory).filePath(QStringLiteral("server.jar"));
            m_downloadManager.downloadAll(
                {{build.url,
                  target,
                  {},
                  QStringLiteral("Paper %1 build %2").arg(stored.minecraftVersion).arg(build.id)}},
                progress);
            if (sha256ForFile(target).compare(build.sha256, Qt::CaseInsensitive) != 0)
            {
                QFile::remove(target);
                throwServerError(QStringLiteral("Paper server JAR checksum verification failed."));
            }
        }
        else if (stored.distribution == ServerDistribution::Fabric)
        {
            const QString encodedVersion = safeEncodedPathSegment(stored.minecraftVersion);
            const QUrl fabricVersions(
                QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/%1")
                    .arg(encodedVersion));
            const QJsonArray loaders = parseJsonArray(
                fetchBytes(fabricVersions, maxMetadataBytes, {QStringLiteral("meta.fabricmc.net")}),
                QStringLiteral("Fabric loader versions"));
            if (loaders.isEmpty() || !loaders.first().isObject())
            {
                throwServerError(QStringLiteral("Fabric has no server loader for Minecraft %1.")
                                     .arg(stored.minecraftVersion));
            }
            const QJsonObject loader =
                loaders.first().toObject().value(QStringLiteral("loader")).toObject();
            const QString loaderVersion = requiredJsonString(loader, QStringLiteral("version"));

            const QJsonArray installers = parseJsonArray(
                fetchBytes(QUrl(QStringLiteral("https://meta.fabricmc.net/v2/versions/installer")),
                           maxMetadataBytes, {QStringLiteral("meta.fabricmc.net")}),
                QStringLiteral("Fabric installer versions"));
            if (installers.isEmpty() || !installers.first().isObject())
            {
                throwServerError(QStringLiteral("Fabric did not return an installer version."));
            }
            const QString installerVersion =
                requiredJsonString(installers.first().toObject(), QStringLiteral("version"));
            static const QRegularExpression safeInstallerVersion(
                QStringLiteral("^[A-Za-z0-9._+-]{1,64}$"));
            if (!safeInstallerVersion.match(installerVersion).hasMatch())
            {
                throwServerError(QStringLiteral("Fabric returned an invalid installer version."));
            }
            const QUrl installerUrl(
                QStringLiteral("https://maven.fabricmc.net/net/fabricmc/fabric-installer/%1/"
                               "fabric-installer-%1.jar")
                    .arg(installerVersion));
            const QUrl checksumUrl(installerUrl.toString() + QStringLiteral(".sha1"));
            const QString checksum =
                QString::fromLatin1(
                    fetchBytes(checksumUrl, 256, {QStringLiteral("maven.fabricmc.net")}))
                    .trimmed()
                    .section(QRegularExpression(QStringLiteral("\\s+")), 0, 0);
            static const QRegularExpression sha1Pattern(QStringLiteral("(?i)^[0-9a-f]{40}$"));
            if (!sha1Pattern.match(checksum).hasMatch())
            {
                throwServerError(QStringLiteral("Fabric installer checksum is invalid."));
            }
            ensureDirectory(m_installerCache);
            const QString installerPath =
                QDir(m_installerCache)
                    .filePath(QStringLiteral("fabric-installer-%1.jar").arg(installerVersion));
            m_downloadManager.downloadAll(
                {{installerUrl, installerPath, checksum,
                  QStringLiteral("Fabric server installer %1").arg(installerVersion)}},
                progress);

            const QString java = QStandardPaths::findExecutable(javaExecutableName());
            if (java.isEmpty())
            {
                throwServerError(
                    QStringLiteral("System Java is required to run the Fabric server installer."));
            }
            const QString logPath =
                QDir(directory).filePath(QStringLiteral("fabric-installer.log"));
            if (QFileInfo(logPath).isSymLink())
            {
                throwServerError(
                    QStringLiteral("Fabric installer log must not be a symbolic link."));
            }
            QProcess installer;
            installer.setProgram(java);
            installer.setArguments({QStringLiteral("-jar"), installerPath, QStringLiteral("server"),
                                    QStringLiteral("-mcversion"), stored.minecraftVersion,
                                    QStringLiteral("-loader"), loaderVersion,
                                    QStringLiteral("-dir"), directory,
                                    QStringLiteral("-downloadMinecraft")});
            installer.setWorkingDirectory(directory);
            installer.setProcessChannelMode(QProcess::MergedChannels);
            installer.setStandardOutputFile(logPath);
            installer.start();
            if (!installer.waitForStarted(10'000))
            {
                throwServerError(QStringLiteral("Unable to start Fabric server installer: %1")
                                     .arg(installer.errorString()));
            }
            if (!installer.waitForFinished(5 * 60 * 1000))
            {
                installer.kill();
                installer.waitForFinished(5'000);
                throwServerError(QStringLiteral("Fabric server installer timed out."));
            }
            const QString launchJar =
                QDir(directory).filePath(QStringLiteral("fabric-server-launch.jar"));
            if (installer.exitStatus() != QProcess::NormalExit || installer.exitCode() != 0 ||
                !QFileInfo(launchJar).isFile() || QFileInfo(launchJar).isSymLink())
            {
                QFile log(logPath);
                QString details;
                if (log.open(QIODevice::ReadOnly))
                {
                    if (log.size() > maxInstallerLogBytes)
                    {
                        log.seek(log.size() - maxInstallerLogBytes);
                    }
                    details = QString::fromUtf8(log.read(maxInstallerLogBytes));
                }
                throwServerError(
                    QStringLiteral("Fabric server installation failed: %1").arg(details.trimmed()));
            }
        }
        else
        {
            throw std::invalid_argument("Unsupported server distribution.");
        }

        const ServerConfig installed{stored.id,
                                     stored.name,
                                     stored.minecraftVersion,
                                     stored.distribution,
                                     true,
                                     stored.eulaAccepted,
                                     stored.javaFlags,
                                     stored.createdAt,
                                     stored.viewDistance,
                                     stored.onlineMode,
                                     stored.useManagedJavaRuntime,
                                     stored.javaRuntimeComponent};
        writeConfig(installed, serverRoot(stored.id));
        report(1, 1,
               QStringLiteral("%1 server installation complete")
                   .arg(distributionName(stored.distribution)));
        return installed;
    }

    ServerConfig DevServerManager::installManagedJavaRuntime(
        const ServerConfig& server, download::DownloadManager::ProgressCallback progress) const
    {
        if (m_javaRuntimeManager == nullptr)
        {
            throwServerError(QStringLiteral("Managed Java runtime support is not available."));
        }
        const ServerConfig stored = requireServer(server.id);
        const QJsonObject metadata = fetchVersionMetadata(stored.minecraftVersion, progress);
        const runtime::RuntimePreferences preferences = m_javaRuntimeManager->preferences();
        const QString directory = serverDirectory(stored);
        const QString component = m_javaRuntimeManager->install(
            directory, stored.minecraftVersion, metadata, preferences.vendor,
            preferences.redundantRuntimes, std::move(progress));

        strata::Instance instance;
        instance.id = stored.id;
        instance.name = stored.name;
        instance.versionId = stored.minecraftVersion;
        instance.directory = directory;
        strata::InstanceSettings settings(directory);
        settings.setJavaRuntimeMode(strata::InstanceSettings::JavaRuntimeMode::Managed);
        settings.setJavaRuntimeComponent(component);
        settings.setJavaRuntimeRedundant(preferences.redundantRuntimes);

        const ServerConfig updated{stored.id,
                                   stored.name,
                                   stored.minecraftVersion,
                                   stored.distribution,
                                   stored.installed,
                                   stored.eulaAccepted,
                                   stored.javaFlags,
                                   stored.createdAt,
                                   stored.viewDistance,
                                   stored.onlineMode,
                                   true,
                                   component};
        writeConfig(updated, serverRoot(stored.id));
        return updated;
    }

    ServerLaunchPlan DevServerManager::launchPlan(const ServerConfig& server) const
    {
        const ServerConfig stored = requireServer(server.id);
        if (!stored.installed)
        {
            throwServerError(QStringLiteral("Install this server before starting it."));
        }
        if (!stored.eulaAccepted)
        {
            throwServerError(
                QStringLiteral("Accept the Minecraft EULA in server settings before starting."));
        }
        const QString directory = serverDirectory(stored);
        const QString jarName = stored.distribution == ServerDistribution::Fabric
                                    ? QStringLiteral("fabric-server-launch.jar")
                                    : QStringLiteral("server.jar");
        const QString jarPath = QDir(directory).filePath(jarName);
        const QFileInfo jarInfo(jarPath);
        if (!jarInfo.isFile() || jarInfo.isSymLink())
        {
            throwServerError(
                QStringLiteral("The installed server JAR is missing. Reinstall this server."));
        }

        QString java;
        if (stored.useManagedJavaRuntime)
        {
            if (m_javaRuntimeManager == nullptr || stored.javaRuntimeComponent.isEmpty())
            {
                throwServerError(QStringLiteral(
                    "Install a managed Java runtime for this server or select System Java."));
            }
            const strata::InstanceSettings settings(directory);
            java = m_javaRuntimeManager->javaExecutable(directory, stored.minecraftVersion,
                                                        stored.javaRuntimeComponent,
                                                        settings.javaRuntimeRedundant());
        }
        else
        {
            java = QStandardPaths::findExecutable(javaExecutableName());
            if (java.isEmpty())
            {
                throwServerError(QStringLiteral("System Java could not be found on PATH."));
            }
        }
        QStringList arguments = stored.javaFlags;
        arguments.append(QStringLiteral("-jar"));
        arguments.append(jarPath);
        arguments.append(QStringLiteral("--nogui"));
        return {java, arguments, directory, stored.javaFlags};
    }

    PaperBuild DevServerManager::latestStablePaperBuild(const QJsonArray& builds)
    {
        static const QRegularExpression safeFilename(QStringLiteral("^[A-Za-z0-9._+-]+\\.jar$"));
        static const QRegularExpression sha256Pattern(QStringLiteral("(?i)^[0-9a-f]{64}$"));
        PaperBuild latest;
        bool found = false;
        for (const QJsonValue& value : builds)
        {
            if (!value.isObject())
            {
                throwServerError(QStringLiteral("Paper returned an invalid build entry."));
            }
            const QJsonObject build = value.toObject();
            if (build.value(QStringLiteral("channel")).toString() != QStringLiteral("STABLE"))
            {
                continue;
            }
            const QJsonValue idValue = build.value(QStringLiteral("id"));
            if (!idValue.isDouble() || idValue.toInt() < 1)
            {
                throwServerError(QStringLiteral("Paper returned an invalid stable build number."));
            }
            const QJsonObject downloads = build.value(QStringLiteral("downloads")).toObject();
            const QJsonObject download =
                downloads.value(QStringLiteral("server:default")).toObject();
            const QJsonObject checksums = download.value(QStringLiteral("checksums")).toObject();
            const QString filename = requiredJsonString(download, QStringLiteral("name"));
            const QString checksum =
                requiredJsonString(checksums, QStringLiteral("sha256")).toLower();
            const QUrl url(requiredJsonString(download, QStringLiteral("url")));
            if (!safeFilename.match(filename).hasMatch() ||
                !sha256Pattern.match(checksum).hasMatch() ||
                !allowedHttpsUrl(url, {QStringLiteral("fill-data.papermc.io")}))
            {
                throwServerError(QStringLiteral("Paper returned invalid stable build metadata."));
            }
            const int id = idValue.toInt();
            if (!found || id > latest.id)
            {
                latest = {id, filename, url, checksum};
                found = true;
            }
        }
        if (!found)
        {
            throwServerError(
                QStringLiteral("Paper has no stable builds for this Minecraft version."));
        }
        return latest;
    }

    QString DevServerManager::serverDirectoryPath(const ServerConfig& server) const
    {
        return serverDirectory(requireServer(server.id));
    }

    QString DevServerManager::readServerProperties(const ServerConfig& server) const
    {
        const ServerConfig stored = requireServer(server.id);
        const QString path =
            QDir(serverDirectory(stored)).filePath(QStringLiteral("server.properties"));
        const QFileInfo info(path);
        if (info.isSymLink())
        {
            throwServerError(QStringLiteral("Server properties file must not be a symbolic link."));
        }
        if (!info.isFile())
        {
            return {};
        }
        if (info.size() > maxPropertiesBytes)
        {
            throwServerError(QStringLiteral("Server properties exceed the supported size limit."));
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            throwServerError(
                QStringLiteral("Unable to read server properties: %1").arg(file.errorString()));
        }
        const QByteArray contents = file.read(maxPropertiesBytes + 1);
        if (file.error() != QFileDevice::NoError)
        {
            throwServerError(
                QStringLiteral("Unable to read server properties: %1").arg(file.errorString()));
        }
        if (contents.size() > maxPropertiesBytes)
        {
            throwServerError(QStringLiteral("Server properties exceed the supported size limit."));
        }
        return QString::fromUtf8(contents);
    }

    ServerConfig DevServerManager::saveServerProperties(const ServerConfig& server,
                                                        const QString& contents) const
    {
        const ServerConfig stored = requireServer(server.id);
        if (contents.size() > maxPropertiesBytes || contents.contains(QChar::Null))
        {
            throw std::invalid_argument(
                "Server properties text is invalid or too large to save safely.");
        }
        int viewDistance = stored.viewDistance;
        bool onlineMode = stored.onlineMode;
        bool hasViewDistance = false;
        bool hasOnlineMode = false;
        static const QRegularExpression lineEndings(QStringLiteral("\\r\\n|\\n|\\r"));
        for (const QString& line : contents.split(lineEndings))
        {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            {
                continue;
            }
            const qsizetype separator = trimmed.indexOf(QLatin1Char('='));
            if (separator <= 0)
            {
                continue;
            }
            const QString key = trimmed.left(separator).trimmed();
            const QString value = trimmed.mid(separator + 1).trimmed();
            if (key == QStringLiteral("view-distance"))
            {
                if (hasViewDistance)
                {
                    throw std::invalid_argument(
                        "server.properties contains duplicate view-distance entries.");
                }
                bool ok = false;
                viewDistance = value.toInt(&ok);
                if (!ok)
                {
                    throw std::invalid_argument(
                        "View distance must be a whole number between 2 and 32.");
                }
                requireViewDistance(viewDistance);
                hasViewDistance = true;
            }
            else if (key == QStringLiteral("online-mode"))
            {
                if (hasOnlineMode ||
                    (value.compare(QStringLiteral("true"), Qt::CaseInsensitive) != 0 &&
                     value.compare(QStringLiteral("false"), Qt::CaseInsensitive) != 0))
                {
                    throw std::invalid_argument(
                        "online-mode must appear once and be true or false.");
                }
                onlineMode = value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
                hasOnlineMode = true;
            }
        }

        const QString path =
            QDir(serverDirectory(stored)).filePath(QStringLiteral("server.properties"));
        if (QFileInfo(path).isSymLink())
        {
            throwServerError(QStringLiteral("Server properties file must not be a symbolic link."));
        }
        writeTextAtomically(path, contents);
        const ServerConfig updated{stored.id,
                                   stored.name,
                                   stored.minecraftVersion,
                                   stored.distribution,
                                   stored.installed,
                                   stored.eulaAccepted,
                                   stored.javaFlags,
                                   stored.createdAt,
                                   viewDistance,
                                   onlineMode,
                                   stored.useManagedJavaRuntime,
                                   stored.javaRuntimeComponent};
        writeConfig(updated, serverRoot(stored.id));
        return updated;
    }

    void DevServerManager::deleteServer(const ServerConfig& server) const
    {
        const ServerConfig stored = requireServer(server.id);
        const QString root = serverRoot(stored.id);
        if (!QDir(root).removeRecursively())
        {
            throwServerError(QStringLiteral("Unable to delete server directory: %1").arg(root));
        }
    }

    ServerConfig DevServerManager::requireServer(const QString& id) const
    {
        const QString root = serverRoot(id);
        const QString configPath = QDir(root).filePath(QStringLiteral("server.json"));
        if (QFileInfo(configPath).isSymLink() || !QFileInfo(configPath).isFile())
        {
            throwServerError(QStringLiteral("Server does not exist: %1").arg(id));
        }
        const ServerConfig server = readConfig(configPath);
        if (server.id != id)
        {
            throwServerError(
                QStringLiteral("Server configuration identifier does not match its directory."));
        }
        return server;
    }

    QJsonObject DevServerManager::fetchVersionMetadata(
        const QString& versionId, const download::DownloadManager::ProgressCallback& progress) const
    {
        const mojang::VersionManifest manifest = m_manifestClient.fetchManifest();
        const auto version = std::find_if(manifest.versions.cbegin(), manifest.versions.cend(),
                                          [&versionId](const mojang::Version& candidate)
                                          { return candidate.id == versionId; });
        if (version == manifest.versions.cend())
        {
            throwServerError(
                QStringLiteral("Minecraft version is not available in the Mojang manifest."));
        }
        if (!allowedHttpsUrl(version->url, {QStringLiteral("piston-meta.mojang.com"),
                                            QStringLiteral("launchermeta.mojang.com")}))
        {
            throwServerError(QStringLiteral("Mojang version metadata URL is not trusted."));
        }
        ensureDirectory(m_installerCache);
        const QString path =
            QDir(m_installerCache).filePath(QStringLiteral("minecraft-%1.json").arg(versionId));
        m_downloadManager.downloadAll(
            {{version->url, path, version->sha1,
              QStringLiteral("Minecraft %1 metadata").arg(versionId), maxMetadataBytes}},
            progress);
        if (QFileInfo(path).isSymLink() || QFileInfo(path).size() > maxMetadataBytes)
        {
            throwServerError(QStringLiteral("Minecraft version metadata is unsafe or too large."));
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            throwServerError(QStringLiteral("Unable to read Minecraft version metadata: %1")
                                 .arg(file.errorString()));
        }
        const QByteArray bytes = file.read(maxMetadataBytes + 1);
        if (file.error() != QFileDevice::NoError || bytes.size() > maxMetadataBytes)
        {
            throwServerError(QStringLiteral("Minecraft version metadata is too large to read."));
        }
        return parseJsonObject(bytes, QStringLiteral("Minecraft version metadata"));
    }

    QString DevServerManager::serverRoot(const QString& id) const
    {
        static const QRegularExpression idPattern(
            QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"),
            QRegularExpression::CaseInsensitiveOption);
        if (!idPattern.match(id).hasMatch())
        {
            throwServerError(QStringLiteral("Invalid server identifier."));
        }
        const QString root = QDir(m_serversDirectory).filePath(id);
        if (!pathIsWithin(m_serversDirectory, root) || QFileInfo(root).isSymLink())
        {
            throwServerError(QStringLiteral("Server path escapes the launcher data directory."));
        }
        return root;
    }

    QString DevServerManager::serverDirectory(const ServerConfig& server) const
    {
        const QString root = serverRoot(server.id);
        const QString directory = QDir(root).filePath(QStringLiteral("server"));
        if (!pathIsWithin(root, directory) || QFileInfo(directory).isSymLink())
        {
            throwServerError(
                QStringLiteral("Server directory escapes the launcher data directory."));
        }
        return directory;
    }

    void DevServerManager::writeServerProperties(const ServerConfig& server) const
    {
        const QString path =
            QDir(serverDirectory(server)).filePath(QStringLiteral("server.properties"));
        if (QFileInfo(path).isSymLink())
        {
            throwServerError(QStringLiteral("Server properties file must not be a symbolic link."));
        }
        QString contents;
        const QFileInfo info(path);
        if (info.isFile())
        {
            if (info.size() > maxPropertiesBytes)
            {
                throwServerError(
                    QStringLiteral("Server properties exceed the supported size limit."));
            }
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                throwServerError(
                    QStringLiteral("Unable to read server properties: %1").arg(file.errorString()));
            }
            const QByteArray bytes = file.read(maxPropertiesBytes + 1);
            if (file.error() != QFileDevice::NoError)
            {
                throwServerError(
                    QStringLiteral("Unable to read server properties: %1").arg(file.errorString()));
            }
            if (bytes.size() > maxPropertiesBytes)
            {
                throwServerError(
                    QStringLiteral("Server properties exceed the supported size limit."));
            }
            contents = QString::fromUtf8(bytes);
        }
        contents = updatePropertyLines(contents, QStringLiteral("view-distance"),
                                       QString::number(server.viewDistance));
        contents = updatePropertyLines(contents, QStringLiteral("online-mode"),
                                       server.onlineMode ? QStringLiteral("true")
                                                         : QStringLiteral("false"));
        writeTextAtomically(path, contents);
    }

    void DevServerManager::writeConfig(const ServerConfig& server, const QString& root) const
    {
        ensureDirectory(root);
        const QString path = QDir(root).filePath(QStringLiteral("server.json"));
        if (QFileInfo(path).isSymLink())
        {
            throwServerError(
                QStringLiteral("Refusing to replace a symbolic-link server configuration."));
        }
        QJsonArray flags;
        for (const QString& flag : server.javaFlags)
        {
            flags.append(flag);
        }
        QJsonObject object{{QStringLiteral("id"), server.id},
                           {QStringLiteral("name"), server.name},
                           {QStringLiteral("minecraftVersion"), server.minecraftVersion},
                           {QStringLiteral("distribution"), distributionName(server.distribution)},
                           {QStringLiteral("installed"), server.installed},
                           {QStringLiteral("eulaAccepted"), server.eulaAccepted},
                           {QStringLiteral("javaFlags"), flags},
                           {QStringLiteral("createdAt"), server.createdAt},
                           {QStringLiteral("viewDistance"), server.viewDistance},
                           {QStringLiteral("onlineMode"), server.onlineMode},
                           {QStringLiteral("useManagedJavaRuntime"), server.useManagedJavaRuntime}};
        if (!server.javaRuntimeComponent.isEmpty())
        {
            object.insert(QStringLiteral("javaRuntimeComponent"), server.javaRuntimeComponent);
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly))
        {
            throwServerError(
                QStringLiteral("Unable to write server configuration: %1").arg(file.errorString()));
        }
        const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Indented);
        if (file.write(json) != json.size() || !file.commit())
        {
            throwServerError(QStringLiteral("Unable to commit server configuration: %1")
                                 .arg(file.errorString()));
        }
    }

    ServerConfig DevServerManager::readConfig(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            throwServerError(
                QStringLiteral("Unable to read server configuration: %1").arg(file.errorString()));
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            throwServerError(QStringLiteral("Server configuration is invalid: %1")
                                 .arg(parseError.errorString()));
        }
        const QJsonObject object = document.object();
        ServerConfig server;
        server.id = requiredString(object, QStringLiteral("id"));
        server.name = requireName(requiredString(object, QStringLiteral("name")));
        server.minecraftVersion =
            requireVersion(requiredString(object, QStringLiteral("minecraftVersion")));
        server.distribution =
            parseDistribution(requiredString(object, QStringLiteral("distribution")));
        server.createdAt = requiredString(object, QStringLiteral("createdAt"));
        server.installed = optionalBoolean(object, QStringLiteral("installed"), false);
        server.eulaAccepted = optionalBoolean(object, QStringLiteral("eulaAccepted"), false);
        server.viewDistance =
            requireViewDistance(optionalInteger(object, QStringLiteral("viewDistance"), 10));
        server.onlineMode = optionalBoolean(object, QStringLiteral("onlineMode"), true);
        server.useManagedJavaRuntime =
            optionalBoolean(object, QStringLiteral("useManagedJavaRuntime"), false);
        const QJsonValue component = object.value(QStringLiteral("javaRuntimeComponent"));
        if (component.isString())
        {
            server.javaRuntimeComponent = component.toString();
        }
        const QJsonValue flags = object.value(QStringLiteral("javaFlags"));
        if (flags.isArray())
        {
            for (const QJsonValue& flag : flags.toArray())
            {
                if (!flag.isString())
                {
                    throwServerError(
                        QStringLiteral("Server configuration contains invalid Java flags."));
                }
                server.javaFlags.append(flag.toString());
            }
        }
        try
        {
            server.javaFlags = strata::InstanceSettings::validateFlags(server.javaFlags);
        }
        catch (const std::invalid_argument& exception)
        {
            throwServerError(QStringLiteral("Server configuration contains invalid Java flags: %1")
                                 .arg(QString::fromUtf8(exception.what())));
        }
        return server;
    }

    QString DevServerManager::requireName(const QString& value)
    {
        static const QRegularExpression unsafe(QStringLiteral("[\\\\/:*?\"<>|\\p{Cc}]"));
        const QString name = value.trimmed();
        if (name.isEmpty() || name.size() > 80 || name == QStringLiteral(".") ||
            name == QStringLiteral("..") || unsafe.match(name).hasMatch())
        {
            throw std::invalid_argument(
                "Server name must be 1-80 characters and cannot contain path characters.");
        }
        return name;
    }

    QString DevServerManager::requireVersion(const QString& value)
    {
        static const QRegularExpression safe(QStringLiteral("^[A-Za-z0-9._+-]+$"));
        const QString version = value.trimmed();
        if (!safe.match(version).hasMatch())
        {
            throw std::invalid_argument(
                "Minecraft server version contains unsupported characters.");
        }
        return version;
    }

    int DevServerManager::requireViewDistance(int value)
    {
        if (value < 2 || value > 32)
        {
            throw std::invalid_argument("View distance must be between 2 and 32 chunks.");
        }
        return value;
    }

    QString DevServerManager::distributionName(ServerDistribution distribution)
    {
        switch (distribution)
        {
        case ServerDistribution::Vanilla:
            return QStringLiteral("VANILLA");
        case ServerDistribution::Fabric:
            return QStringLiteral("FABRIC");
        case ServerDistribution::Paper:
            return QStringLiteral("PAPER");
        }
        throw std::invalid_argument("Invalid server distribution.");
    }

    ServerDistribution DevServerManager::parseDistribution(const QString& value)
    {
        if (value == QStringLiteral("VANILLA"))
        {
            return ServerDistribution::Vanilla;
        }
        if (value == QStringLiteral("FABRIC"))
        {
            return ServerDistribution::Fabric;
        }
        if (value == QStringLiteral("PAPER"))
        {
            return ServerDistribution::Paper;
        }
        throwServerError(
            QStringLiteral("Server configuration contains an unsupported distribution."));
    }

    void DevServerManager::writeTextAtomically(const QString& path, const QString& contents)
    {
        const QString parent = QFileInfo(path).absolutePath();
        ensureDirectory(parent);
        if (QFileInfo(path).isSymLink())
        {
            throwServerError(
                QStringLiteral("Refusing to replace a symbolic-link file: %1").arg(path));
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly))
        {
            throwServerError(
                QStringLiteral("Unable to write file %1: %2").arg(path, file.errorString()));
        }
        const QByteArray bytes = contents.toUtf8();
        if (file.write(bytes) != bytes.size() || !file.commit())
        {
            throwServerError(
                QStringLiteral("Unable to commit file %1: %2").arg(path, file.errorString()));
        }
    }
} // namespace strata::server
