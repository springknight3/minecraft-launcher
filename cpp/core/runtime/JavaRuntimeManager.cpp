#include "JavaRuntimeManager.h"

#include "../settings/InstanceSettings.h"

#include <archive.h>
#include <archive_entry.h>

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSysInfo>
#include <QTimer>

#include <algorithm>
#include <memory>
#include <stdexcept>

namespace strata::runtime
{
    namespace
    {
        constexpr qsizetype maxMetadataBytes = 16 * 1024 * 1024;
        constexpr qint64 maxArchiveBytes = 1'000'000'000;
        constexpr qint64 maxExpandedBytes = 2'000'000'000;
        constexpr int maxArchiveEntries = 200'000;
        const QUrl runtimeIndexUrl(
            QStringLiteral("https://piston-meta.mojang.com/v1/products/java-runtime/"
                           "2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json"));
        const QString javaPathMarker = QStringLiteral(".stratalauncher-java-path");
        const QString versionMarker = QStringLiteral(".stratalauncher-runtime-version");
        const QString vendorMarker = QStringLiteral(".stratalauncher-runtime-vendor");
        const QString runtimeMetadataNames[] = {javaPathMarker, versionMarker, vendorMarker};

        [[noreturn]] void fail(const QString& message)
        {
            throw std::runtime_error(message.toStdString());
        }

        QString requiredString(const QJsonObject& object, const QString& key)
        {
            const QJsonValue value = object.value(key);
            if (!value.isString() || value.toString().isEmpty())
            {
                fail(QStringLiteral("Java runtime metadata is missing %1.").arg(key));
            }
            return value.toString();
        }

        QString sha1(const QByteArray& data)
        {
            return QString::fromLatin1(
                QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
        }

        QString digestFile(const QString& path, QCryptographicHash::Algorithm algorithm)
        {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                fail(
                    QStringLiteral("Unable to read Java runtime file: %1").arg(file.errorString()));
            }
            QCryptographicHash hash(algorithm);
            while (!file.atEnd())
            {
                const QByteArray chunk = file.read(64 * 1024);
                if (chunk.isEmpty() && file.error() != QFileDevice::NoError)
                {
                    fail(QStringLiteral("Unable to read Java runtime file: %1")
                             .arg(file.errorString()));
                }
                hash.addData(chunk);
            }
            return QString::fromLatin1(hash.result().toHex());
        }

        bool safeComponent(const QString& component)
        {
            static const QRegularExpression pattern(QStringLiteral("^[a-z0-9-]+$"));
            return pattern.match(component).hasMatch();
        }

        bool hasSymlinkBetween(const QString& boundary, const QString& path)
        {
            const QString root = QDir::cleanPath(QFileInfo(boundary).absoluteFilePath());
            const QString target = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
            if (target != root && !target.startsWith(root + QDir::separator()))
            {
                return true;
            }
            QString current = root;
            if (QFileInfo(current).isSymLink())
            {
                return true;
            }
            const QString relative = QDir(root).relativeFilePath(target);
            for (const QString& part : relative.split(QDir::separator(), Qt::SkipEmptyParts))
            {
                current = QDir(current).filePath(part);
                if (QFileInfo(current).isSymLink())
                {
                    return true;
                }
            }
            return false;
        }

        bool isWindows()
        {
#if defined(Q_OS_WIN)
            return true;
#else
            return false;
#endif
        }

        QString hostOs()
        {
#if defined(Q_OS_WIN)
            return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
            return QStringLiteral("mac");
#else
            return QStringLiteral("linux");
#endif
        }

        QString hostArchitecture()
        {
#if defined(Q_PROCESSOR_X86_64)
            return QStringLiteral("x64");
#elif defined(Q_PROCESSOR_X86_32)
            return QStringLiteral("x86");
#elif defined(Q_PROCESSOR_ARM_64)
            return QStringLiteral("aarch64");
#elif defined(Q_PROCESSOR_ARM_32)
            return QStringLiteral("arm");
#else
            return QSysInfo::currentCpuArchitecture().toLower();
#endif
        }

        QString checkHttpsHost(const QUrl& url, const QStringList& allowedHosts)
        {
            const QString host = url.host().toLower();
            const bool allowed = std::any_of(
                allowedHosts.cbegin(), allowedHosts.cend(), [&host](const QString& candidate)
                { return host == candidate || host.endsWith(QLatin1Char('.') + candidate); });
            if (url.scheme() != QStringLiteral("https") || !allowed || !url.userInfo().isEmpty() ||
                (url.port() != -1 && url.port() != 443))
            {
                fail(QStringLiteral("Java runtime request used an unapproved HTTPS host."));
            }
            return host;
        }

        QByteArray fetchBytes(const QUrl& url, const QStringList& allowedHosts,
                              qsizetype maximumBytes = maxMetadataBytes)
        {
            checkHttpsHost(url, allowedHosts);
            QNetworkAccessManager manager;
            QNetworkRequest request(url);
            request.setHeader(QNetworkRequest::UserAgentHeader,
                              QStringLiteral("StrataLauncher/0.1.0"));
            request.setTransferTimeout(120'000);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);
            QNetworkReply* reply = manager.get(request);
            QByteArray bytes;
            bool exceeded = false;
            QObject::connect(reply, &QIODevice::readyRead, reply,
                             [&]()
                             {
                                 bytes.append(reply->readAll());
                                 exceeded = bytes.size() > maximumBytes;
                                 if (exceeded)
                                 {
                                     reply->abort();
                                 }
                             });
            QEventLoop loop;
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();
            bytes.append(reply->readAll());
            const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QUrl finalUrl = reply->url();
            const QString error = reply->errorString();
            const auto networkError = reply->error();
            reply->deleteLater();
            checkHttpsHost(finalUrl, allowedHosts);
            if (exceeded || bytes.size() > maximumBytes)
            {
                fail(QStringLiteral("Java runtime metadata exceeds its size limit."));
            }
            if (networkError != QNetworkReply::NoError || status < 200 || status >= 300)
            {
                fail(QStringLiteral("Java runtime request failed (HTTP %1): %2")
                         .arg(status)
                         .arg(error));
            }
            return bytes;
        }

        QJsonObject parseObject(const QByteArray& bytes, const QString& label)
        {
            QJsonParseError error;
            const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
            if (error.error != QJsonParseError::NoError || !document.isObject())
            {
                fail(QStringLiteral("Could not parse %1 metadata: %2")
                         .arg(label, error.errorString()));
            }
            return document.object();
        }

        QJsonArray parseArray(const QByteArray& bytes, const QString& label)
        {
            QJsonParseError error;
            const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
            if (error.error != QJsonParseError::NoError || !document.isArray())
            {
                fail(QStringLiteral("Could not parse %1 metadata: %2")
                         .arg(label, error.errorString()));
            }
            return document.array();
        }

        void writeMarker(const QString& path, const QByteArray& content)
        {
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size() ||
                !file.commit())
            {
                fail(QStringLiteral("Unable to write Java runtime metadata: %1")
                         .arg(file.errorString()));
            }
        }
        void safeLink(const QString& root, const QString& relative, const QString& target)
        {
            if (target.isEmpty() || target.contains(QLatin1Char('\\')) ||
                target.startsWith(QLatin1Char('/')) || target.contains(QLatin1Char(':')))
            {
                fail(QStringLiteral("Java runtime contains an unsafe symbolic link."));
            }
            const QString linkPath = JavaRuntimeManager::validateRuntimePath(root, relative);
            const QString targetPath =
                QDir::cleanPath(QDir(QFileInfo(linkPath).absolutePath()).filePath(target));
            const QString absoluteRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
            if (targetPath != absoluteRoot &&
                !targetPath.startsWith(absoluteRoot + QDir::separator()))
            {
                fail(QStringLiteral("Java runtime symbolic link escapes its installation folder."));
            }
        }

        QString safeArchivePath(const QString& root, const QString& relative)
        {
            const QString target = JavaRuntimeManager::validateRuntimePath(root, relative);
            QString current = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
            const QStringList parts = relative.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            for (qsizetype index = 0; index + 1 < parts.size(); ++index)
            {
                current = QDir(current).filePath(parts.at(index));
                if (QFileInfo(current).isSymLink())
                {
                    fail(QStringLiteral(
                        "Java runtime archive attempts to write through a symbolic link."));
                }
            }
            if (QFileInfo(target).isSymLink())
            {
                fail(QStringLiteral("Java runtime archive attempts to overwrite a symbolic link."));
            }
            return target;
        }

        void markExecutable(const QString& path)
        {
            if (isWindows())
            {
                return;
            }
            QFile file(path);
            auto permissions = file.permissions();
            permissions |= QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther;
            if (!file.setPermissions(permissions) || !QFileInfo(path).isExecutable())
            {
                fail(QStringLiteral("Could not mark the managed Java runtime executable."));
            }
        }

        void removeRuntimeTree(const QString& path)
        {
            QFileInfo info(path);
            if (info.isSymLink())
            {
                if (!QFile::remove(path))
                {
                    fail(QStringLiteral("Unable to remove a Java runtime symbolic link."));
                }
                return;
            }
            QDir directory(path);
            if (directory.exists() && !directory.removeRecursively())
            {
                fail(QStringLiteral("Unable to remove old Java runtime files."));
            }
        }

        void replaceRuntime(const QString& staging, const QString& destination)
        {
            const QString parent = QFileInfo(destination).absolutePath();
            if (QFileInfo(staging).absolutePath() != QDir::cleanPath(parent) ||
                QFileInfo(staging).isSymLink() || QFileInfo(destination).isSymLink())
            {
                fail(QStringLiteral("Java runtime replacement path is unsafe."));
            }
            const QString backup =
                QDir(parent).filePath(QStringLiteral(".previous-%1")
                                          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
            const bool hadPrevious = QFileInfo::exists(destination);
            if (hadPrevious && !QDir().rename(destination, backup))
            {
                fail(QStringLiteral("Unable to stage the previous Java runtime."));
            }
            if (!QDir().rename(staging, destination))
            {
                if (hadPrevious)
                {
                    QDir().rename(backup, destination);
                }
                fail(QStringLiteral("Unable to replace the Java runtime installation."));
            }
            if (hadPrevious)
            {
                removeRuntimeTree(backup);
            }
        }

        QJsonObject runtimeMetadata(const QString& instanceDirectory, const QString& versionId)
        {
            const QString path =
                QDir(instanceDirectory)
                    .filePath(QStringLiteral("game/versions/%1/%1.json").arg(versionId));
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly) || file.size() > maxMetadataBytes)
            {
                fail(QStringLiteral("Installed Minecraft metadata is missing or too large."));
            }
            return parseObject(file.readAll(), QStringLiteral("Minecraft version"));
        }

        struct RuntimePackage
        {
            QUrl url;
            QString checksum;
            QString version;
            QString shaAlgorithm;
            bool zip = false;
        };

        RuntimePackage fetchExternalPackage(RuntimeVendor vendor, int major)
        {
            const QString os = hostOs();
            const QString arch = hostArchitecture();
            if ((arch != QStringLiteral("x64") && arch != QStringLiteral("aarch64")) ||
                (os != QStringLiteral("windows") && os != QStringLiteral("linux") &&
                 os != QStringLiteral("mac")))
            {
                fail(QStringLiteral(
                    "Temurin and Zulu support 64-bit ARM and x64 on Windows, macOS, and Linux."));
            }
            if (vendor == RuntimeVendor::Temurin)
            {
                const QUrl api(QStringLiteral("https://api.adoptium.net/v3/assets/latest/%1/hotspot"
                                              "?architecture=%2&heap_size=normal&image_type=jre"
                                              "&jvm_impl=hotspot&os=%3&vendor=eclipse")
                                   .arg(major)
                                   .arg(arch, os));
                const QJsonArray assets =
                    parseArray(fetchBytes(api, {QStringLiteral("api.adoptium.net")}),
                               QStringLiteral("Eclipse Temurin"));
                if (assets.isEmpty() || !assets.first().isObject())
                {
                    fail(QStringLiteral("Eclipse Temurin has no compatible Java %1 runtime.")
                             .arg(major));
                }
                const QJsonObject asset = assets.first().toObject();
                const QJsonObject binary = asset.value(QStringLiteral("binary")).toObject();
                const QJsonObject package = binary.value(QStringLiteral("package")).toObject();
                const QString checksum =
                    requiredString(package, QStringLiteral("checksum")).toLower();
                static const QRegularExpression sha256Pattern(QStringLiteral("^[0-9a-f]{64}$"));
                if (!sha256Pattern.match(checksum).hasMatch())
                {
                    fail(QStringLiteral("Eclipse Temurin returned an invalid SHA-256 checksum."));
                }
                const QString version =
                    requiredString(asset.value(QStringLiteral("version")).toObject(),
                                   QStringLiteral("openjdk_version"));
                const QString name = requiredString(package, QStringLiteral("name"));
                const QUrl url(requiredString(package, QStringLiteral("link")));
                checkHttpsHost(url, {QStringLiteral("github.com")});
                return {url, checksum, version, QStringLiteral("SHA-256"),
                        name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)};
            }

            const QString archiveType =
                os == QStringLiteral("windows") ? QStringLiteral("zip") : QStringLiteral("tar.gz");
            const QString zuluArch = arch == QStringLiteral("aarch64") ? QStringLiteral("aarch64")
                                                                       : QStringLiteral("x86");
            const QUrl api(
                QStringLiteral(
                    "https://api.azul.com/metadata/v1/zulu/packages/"
                    "?java_version=%1&os=%2&arch=%3&hw_bitness=64&archive_type=%4"
                    "&java_package_type=jre&release_status=ga&availability_types=CA&latest=true")
                    .arg(major)
                    .arg(os == QStringLiteral("mac") ? QStringLiteral("macos") : os, zuluArch,
                         archiveType));
            const QJsonArray packages = parseArray(
                fetchBytes(api, {QStringLiteral("api.azul.com")}), QStringLiteral("Azul Zulu"));
            if (packages.isEmpty() || !packages.first().isObject())
            {
                fail(QStringLiteral("Azul Zulu has no compatible Java %1 runtime.").arg(major));
            }
            const QJsonObject package = packages.first().toObject();
            const QUrl url(requiredString(package, QStringLiteral("download_url")));
            checkHttpsHost(url, {QStringLiteral("cdn.azul.com")});
            QString version;
            const QJsonValue javaVersion = package.value(QStringLiteral("java_version"));
            if (javaVersion.isArray())
            {
                QStringList parts;
                for (const QJsonValue& value : javaVersion.toArray())
                {
                    if (!value.isString() && !value.isDouble())
                    {
                        fail(QStringLiteral("Azul Zulu returned an invalid runtime version."));
                    }
                    parts.append(value.toVariant().toString());
                }
                while (parts.size() > 3 && parts.last() == QStringLiteral("0"))
                {
                    parts.removeLast();
                }
                version = parts.join(QLatin1Char('.'));
            }
            else
            {
                version = requiredString(package, QStringLiteral("name"));
            }
            return {url, {}, version, {}, os == QStringLiteral("windows")};
        }

        void downloadExternalArchive(const RuntimePackage& package, const QString& destination)
        {
            checkHttpsHost(package.url, {QStringLiteral("github.com"),
                                         QStringLiteral("release-assets.githubusercontent.com"),
                                         QStringLiteral("cdn.azul.com")});
            QNetworkAccessManager manager;
            QNetworkRequest request(package.url);
            request.setHeader(QNetworkRequest::UserAgentHeader,
                              QStringLiteral("StrataLauncher/0.1.0"));
            request.setTransferTimeout(600'000);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);
            QNetworkReply* reply = manager.get(request);
            QSaveFile output(destination);
            if (!output.open(QIODevice::WriteOnly))
            {
                reply->abort();
                fail(QStringLiteral("Unable to stage Java runtime archive: %1")
                         .arg(output.errorString()));
            }
            QCryptographicHash hash(package.shaAlgorithm == QStringLiteral("SHA-256")
                                        ? QCryptographicHash::Sha256
                                        : QCryptographicHash::Sha1);
            qint64 totalBytes = 0;
            bool writeFailed = false;
            QObject::connect(reply, &QIODevice::readyRead, reply,
                             [&]()
                             {
                                 const QByteArray data = reply->readAll();
                                 totalBytes += data.size();
                                 if (totalBytes > maxArchiveBytes ||
                                     output.write(data) != data.size())
                                 {
                                     writeFailed = true;
                                     reply->abort();
                                     return;
                                 }
                                 hash.addData(data);
                             });
            QEventLoop loop;
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();
            const QByteArray trailing = reply->readAll();
            if (!trailing.isEmpty())
            {
                totalBytes += trailing.size();
                if (totalBytes > maxArchiveBytes || output.write(trailing) != trailing.size())
                {
                    writeFailed = true;
                }
                else
                {
                    hash.addData(trailing);
                }
            }
            const QUrl finalUrl = reply->url();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QString networkError = reply->errorString();
            const auto errorCode = reply->error();
            reply->deleteLater();
            checkHttpsHost(finalUrl, {QStringLiteral("github.com"),
                                      QStringLiteral("release-assets.githubusercontent.com"),
                                      QStringLiteral("cdn.azul.com")});
            if (writeFailed || errorCode != QNetworkReply::NoError || status < 200 || status >= 300)
            {
                output.cancelWriting();
                fail(QStringLiteral("Java runtime download failed (HTTP %1): %2")
                         .arg(status)
                         .arg(networkError));
            }
            if (!package.checksum.isEmpty() &&
                QString::fromLatin1(hash.result().toHex())
                        .compare(package.checksum, Qt::CaseInsensitive) != 0)
            {
                output.cancelWriting();
                fail(QStringLiteral("Java runtime archive failed its SHA-256 integrity check."));
            }
            if (!output.commit())
            {
                fail(QStringLiteral("Unable to save Java runtime archive: %1")
                         .arg(output.errorString()));
            }
        }

        QString findJavaExecutable(const QString& root)
        {
            const QStringList candidates =
                isWindows()
                    ? QStringList{QStringLiteral("bin/javaw.exe"), QStringLiteral("bin/java.exe")}
                    : QStringList{QStringLiteral("bin/java"),
                                  QStringLiteral("Contents/Home/bin/java")};
            QDirIterator iterator(root, QDir::Files | QDir::NoDotAndDotDot,
                                  QDirIterator::Subdirectories);
            QString best;
            int bestRank = candidates.size() + 1;
            while (iterator.hasNext())
            {
                const QString path = iterator.next();
                if (QFileInfo(path).isSymLink())
                {
                    continue;
                }
                const QString relative = QDir(root).relativeFilePath(path);
                for (qsizetype i = 0; i < candidates.size(); ++i)
                {
                    if (relative.endsWith(candidates.at(i)) && i < bestRank)
                    {
                        best = relative;
                        bestRank = static_cast<int>(i);
                    }
                }
            }
            if (best.isEmpty())
            {
                fail(QStringLiteral("Java runtime archive contains no executable."));
            }
            return best;
        }

    } // namespace

    JavaRuntimeManager::JavaRuntimeManager(const download::DownloadManager& downloadManager,
                                           QString dataDirectory)
        : m_downloadManager(downloadManager),
          m_dataDirectory(QDir::cleanPath(QFileInfo(dataDirectory).absoluteFilePath()))
    {
    }

    RuntimePreferences JavaRuntimeManager::preferences() const
    {
        const QString path =
            QDir(m_dataDirectory).filePath(QStringLiteral("runtime-settings.json"));
        QFile file(path);
        if (!file.exists())
        {
            return {};
        }
        if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
        {
            fail(QStringLiteral("Unable to read runtime preferences: %1").arg(file.errorString()));
        }
        const QJsonObject settings =
            parseObject(file.readAll(), QStringLiteral("runtime preferences"));
        const QString vendorValue =
            settings.value(QStringLiteral("vendor")).toString(QStringLiteral("MOJANG"));
        return {parseVendor(vendorValue),
                settings.value(QStringLiteral("redundantRuntimes")).toBool(false)};
    }

    void JavaRuntimeManager::setPreferences(RuntimePreferences preferences) const
    {
        const QString parent = QFileInfo(m_dataDirectory).absoluteFilePath();
        if (!QDir().mkpath(parent))
        {
            fail(QStringLiteral("Unable to create runtime settings directory."));
        }
        writeMarker(
            QDir(parent).filePath(QStringLiteral("runtime-settings.json")),
            QJsonDocument(
                QJsonObject{{QStringLiteral("vendor"), vendorName(preferences.vendor).toUpper()},
                            {QStringLiteral("redundantRuntimes"), preferences.redundantRuntimes}})
                .toJson(QJsonDocument::Indented));
    }

    QString JavaRuntimeManager::install(const strata::Instance& instance,
                                        const QJsonObject& versionMetadata,
                                        ProgressCallback progress) const
    {
        const RuntimePreferences current = preferences();
        return install(instance.directory, instance.versionId, versionMetadata, current.vendor,
                       current.redundantRuntimes, std::move(progress));
    }

    QString JavaRuntimeManager::install(const QString& ownerDirectory, const QString& versionId,
                                        const QJsonObject& versionMetadata, RuntimeVendor vendor,
                                        bool redundant, ProgressCallback progress) const
    {
        if (vendor == RuntimeVendor::Mojang)
        {
            return installMojang(ownerDirectory, versionId, versionMetadata, redundant, progress);
        }
        return installExternal(ownerDirectory, versionId, versionMetadata, vendor, redundant,
                               progress);
    }

    QString JavaRuntimeManager::runtimeComponent(RuntimeVendor vendor, const QJsonObject& metadata,
                                                 const QString& versionId) const
    {
        if (vendor == RuntimeVendor::Mojang)
        {
            return requiredComponent(metadata, versionId);
        }
        return QStringLiteral("%1-java-%2")
            .arg(vendorName(vendor).toLower())
            .arg(requiredJavaMajor(metadata, versionId));
    }

    QString JavaRuntimeManager::javaExecutable(const strata::Instance& instance) const
    {
        const InstanceSettings settings(instance.directory);
        if (settings.javaRuntimeMode() != InstanceSettings::JavaRuntimeMode::Managed)
        {
            fail(QStringLiteral("This instance uses System Java."));
        }
        const QJsonObject metadata = runtimeMetadata(instance.directory, instance.versionId);
        QString component = settings.javaRuntimeComponent();
        if (component.isEmpty())
        {
            component = runtimeComponent(preferences().vendor, metadata, instance.versionId);
        }
        return javaExecutable(instance.directory, instance.versionId, component,
                              settings.javaRuntimeRedundant());
    }

    QList<InstalledRuntime>
    JavaRuntimeManager::listInstalledRuntimes(const QList<RuntimeOwner>& owners) const
    {
        QList<InstalledRuntime> result;
        QHash<QString, qsizetype> byPath;
        QStringList roots{QDir(m_dataDirectory).filePath(QStringLiteral("java-runtimes"))};
        for (const RuntimeOwner& owner : owners)
        {
            roots.append(QDir(owner.directory).filePath(QStringLiteral("java-runtimes")));
        }
        for (const QString& root : roots)
        {
            const QFileInfo rootInfo(root);
            if (!rootInfo.isDir() || hasSymlinkBetween(m_dataDirectory, root))
            {
                continue;
            }
            QDirIterator iterator(root, QDir::Dirs | QDir::NoDotAndDotDot,
                                  QDirIterator::NoIteratorFlags);
            while (iterator.hasNext())
            {
                const QString directory = iterator.next();
                const QFileInfo directoryInfo(directory);
                if (hasSymlinkBetween(root, directory))
                {
                    continue;
                }
                QFile marker(QDir(directory).filePath(javaPathMarker));
                QFile version(QDir(directory).filePath(versionMarker));
                if (!marker.open(QIODevice::ReadOnly) || !version.open(QIODevice::ReadOnly))
                {
                    continue;
                }
                const QStringList markerLines =
                    QString::fromUtf8(marker.readAll())
                        .split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
                if (markerLines.size() != 2 || !safeComponent(directoryInfo.fileName()))
                {
                    continue;
                }
                const QString versionName = QString::fromUtf8(version.readAll()).trimmed();
                if (versionName.isEmpty())
                {
                    continue;
                }
                RuntimeVendor vendor = RuntimeVendor::Mojang;
                QFile vendorFile(QDir(directory).filePath(vendorMarker));
                if (vendorFile.open(QIODevice::ReadOnly))
                {
                    vendor = parseVendor(QString::fromUtf8(vendorFile.readAll()).trimmed());
                }
                QStringList users;
                const QString cleanDirectory = QDir::cleanPath(directory);
                for (const RuntimeOwner& owner : owners)
                {
                    if (!owner.managed || owner.component != directoryInfo.fileName())
                    {
                        continue;
                    }
                    const bool shared =
                        !owner.redundant &&
                        cleanDirectory ==
                            QDir::cleanPath(
                                QDir(m_dataDirectory)
                                    .filePath(
                                        QStringLiteral("java-runtimes/%1").arg(owner.component)));
                    const bool ownedCopy =
                        owner.redundant &&
                        cleanDirectory ==
                            QDir::cleanPath(
                                QDir(owner.directory)
                                    .filePath(
                                        QStringLiteral("java-runtimes/%1").arg(owner.component)));
                    if (shared || ownedCopy)
                    {
                        users.append(owner.name);
                    }
                }
                users.removeDuplicates();
                std::sort(users.begin(), users.end());
                const auto prior = byPath.constFind(cleanDirectory);
                if (prior == byPath.cend())
                {
                    byPath.insert(cleanDirectory, result.size());
                    result.append(
                        {cleanDirectory, directoryInfo.fileName(), vendor, versionName, users});
                }
                else
                {
                    result[prior.value()].users.append(users);
                    result[prior.value()].users.removeDuplicates();
                }
            }
        }
        std::sort(result.begin(), result.end(),
                  [](const InstalledRuntime& left, const InstalledRuntime& right)
                  {
                      if (left.vendor != right.vendor)
                      {
                          return static_cast<int>(left.vendor) < static_cast<int>(right.vendor);
                      }
                      return left.version < right.version;
                  });
        return result;
    }

    void JavaRuntimeManager::deleteRuntime(const InstalledRuntime& runtime,
                                           const QList<RuntimeOwner>& owners) const
    {
        const auto installed = listInstalledRuntimes(owners);
        const QString requestedPath =
            QDir::cleanPath(QFileInfo(runtime.directory).absoluteFilePath());
        const auto current = std::find_if(
            installed.cbegin(), installed.cend(),
            [&requestedPath](const InstalledRuntime& candidate)
            {
                return QDir::cleanPath(QFileInfo(candidate.directory).absoluteFilePath()) ==
                       requestedPath;
            });
        if (current == installed.cend())
        {
            fail(QStringLiteral("Java runtime is not in the installed runtime inventory."));
        }
        if (!current->users.isEmpty())
        {
            fail(QStringLiteral("This Java runtime is still used by %1.")
                     .arg(current->users.join(QStringLiteral(", "))));
        }
        const QString path = requestedPath;
        const QString dataRoot = QDir::cleanPath(QFileInfo(m_dataDirectory).absoluteFilePath());
        const QString instancesRoot = QDir(dataRoot).filePath(QStringLiteral("instances"));
        const QString runtimeRoot = QDir::cleanPath(QFileInfo(path).dir().absolutePath());
        const QString ownerDirectory = QDir::cleanPath(QFileInfo(runtimeRoot).dir().absolutePath());
        const bool shared = runtimeRoot == QDir(dataRoot).filePath(QStringLiteral("java-runtimes"));
        const bool instanceCopy =
            QFileInfo(runtimeRoot).fileName() == QStringLiteral("java-runtimes") &&
            QDir::cleanPath(QFileInfo(ownerDirectory).dir().absolutePath()) ==
                QDir::cleanPath(instancesRoot);
        if ((!shared && !instanceCopy) || !safeComponent(QFileInfo(path).fileName()) ||
            hasSymlinkBetween(dataRoot, path))
        {
            fail(QStringLiteral("Java runtime path is outside approved runtime folders."));
        }
        removeRuntimeTree(path);
    }

    QString JavaRuntimeManager::requiredComponent(const QJsonObject& metadata,
                                                  const QString& versionId)
    {
        const QJsonObject javaVersion = metadata.value(QStringLiteral("javaVersion")).toObject();
        if (javaVersion.contains(QStringLiteral("component")))
        {
            const QString component = javaVersion.value(QStringLiteral("component")).toString();
            if (!safeComponent(component))
            {
                fail(QStringLiteral(
                    "Minecraft metadata contains an invalid Java runtime component."));
            }
            return component;
        }
        static const QRegularExpression numbered(
            QStringLiteral("^1\\.(\\d+)(?:\\.(\\d+))?(?:[-+].*)?$"));
        const auto match = numbered.match(versionId);
        if (match.hasMatch())
        {
            bool minorOk = false;
            bool patchOk = false;
            const int minor = match.captured(1).toInt(&minorOk);
            const int patch = match.captured(2).isEmpty() ? 0 : match.captured(2).toInt(&patchOk);
            if (!minorOk || (!match.captured(2).isEmpty() && !patchOk))
            {
                fail(QStringLiteral("Could not determine Java required by Minecraft %1.")
                         .arg(versionId));
            }
            if (minor <= 16)
                return QStringLiteral("jre-legacy");
            if (minor == 17)
                return QStringLiteral("java-runtime-alpha");
            if (minor <= 19 || (minor == 20 && patch <= 4))
                return QStringLiteral("java-runtime-gamma");
            if (minor == 20 || minor <= 25)
                return QStringLiteral("java-runtime-delta");
        }
        if (QRegularExpression(QStringLiteral("^26(?:\\..*)?$")).match(versionId).hasMatch())
        {
            return QStringLiteral("java-runtime-epsilon");
        }
        if (QRegularExpression(QStringLiteral("^(?:a|b|rc)1\\.[0-9]+\\.[0-9]+$"))
                .match(versionId)
                .hasMatch())
        {
            return QStringLiteral("jre-legacy");
        }
        const auto snapshot =
            QRegularExpression(QStringLiteral("^(1[0-9]|2[0-9])w[0-9]+[a-z]$")).match(versionId);
        if (snapshot.hasMatch())
        {
            const int year = snapshot.captured(1).toInt();
            if (year <= 20)
                return QStringLiteral("jre-legacy");
            if (year == 21)
                return QStringLiteral("java-runtime-alpha");
            if (year <= 23)
                return QStringLiteral("java-runtime-gamma");
            return year >= 26 ? QStringLiteral("java-runtime-epsilon")
                              : QStringLiteral("java-runtime-delta");
        }
        fail(QStringLiteral("Minecraft %1 does not declare its Java runtime requirement.")
                 .arg(versionId));
    }

    int JavaRuntimeManager::requiredJavaMajor(const QJsonObject& metadata, const QString& versionId)
    {
        const QJsonValue majorValue = metadata.value(QStringLiteral("javaVersion"))
                                          .toObject()
                                          .value(QStringLiteral("majorVersion"));
        if (!majorValue.isUndefined())
        {
            if (!majorValue.isDouble())
            {
                fail(QStringLiteral("Minecraft metadata contains an invalid Java major version."));
            }
            const double majorNumber = majorValue.toDouble(-1);
            if (majorNumber < 8 || majorNumber > 99 || majorNumber != static_cast<int>(majorNumber))
            {
                fail(QStringLiteral("Minecraft metadata contains an invalid Java major version."));
            }
            return static_cast<int>(majorNumber);
        }
        const QString component = requiredComponent(metadata, versionId);
        if (component == QStringLiteral("jre-legacy"))
            return 8;
        if (component == QStringLiteral("java-runtime-alpha"))
            return 16;
        if (component == QStringLiteral("java-runtime-gamma"))
            return 17;
        if (component == QStringLiteral("java-runtime-delta"))
            return 21;
        if (component == QStringLiteral("java-runtime-epsilon"))
            return 25;
        fail(QStringLiteral("Could not determine Java required by Minecraft %1.").arg(versionId));
    }

    QString JavaRuntimeManager::platformName(const QString& osName, const QString& architecture)
    {
        const QString os = osName.toLower();
        const QString arch = architecture.toLower();
        const bool arm64 = arch == QStringLiteral("arm64") || arch == QStringLiteral("aarch64");
        const bool x64 = arch == QStringLiteral("amd64") || arch == QStringLiteral("x86_64") ||
                         arch == QStringLiteral("x64");
        const bool x86 = arch == QStringLiteral("x86") || arch.startsWith(QStringLiteral("i3")) ||
                         arch.startsWith(QStringLiteral("i4")) ||
                         arch.startsWith(QStringLiteral("i5")) ||
                         arch.startsWith(QStringLiteral("i6"));
        if (os.contains(QStringLiteral("linux")))
        {
            if (x64)
                return QStringLiteral("linux");
            if (x86)
                return QStringLiteral("linux-i386");
        }
        if (os.contains(QStringLiteral("mac")) || os.contains(QStringLiteral("darwin")))
        {
            if (arm64)
                return QStringLiteral("mac-os-arm64");
            if (x64)
                return QStringLiteral("mac-os");
        }
        if (os.contains(QStringLiteral("win")))
        {
            if (arm64)
                return QStringLiteral("windows-arm64");
            if (x64)
                return QStringLiteral("windows-x64");
            if (x86)
                return QStringLiteral("windows-x86");
        }
        fail(
            QStringLiteral("Mojang does not provide Java for this operating system/architecture."));
    }

    QString JavaRuntimeManager::vendorName(RuntimeVendor vendor)
    {
        switch (vendor)
        {
        case RuntimeVendor::Mojang:
            return QStringLiteral("mojang");
        case RuntimeVendor::Temurin:
            return QStringLiteral("temurin");
        case RuntimeVendor::Zulu:
            return QStringLiteral("zulu");
        }
        fail(QStringLiteral("Unknown Java runtime vendor."));
    }

    RuntimeVendor JavaRuntimeManager::parseVendor(const QString& name)
    {
        if (name.compare(QStringLiteral("MOJANG"), Qt::CaseInsensitive) == 0)
            return RuntimeVendor::Mojang;
        if (name.compare(QStringLiteral("TEMURIN"), Qt::CaseInsensitive) == 0)
            return RuntimeVendor::Temurin;
        if (name.compare(QStringLiteral("ZULU"), Qt::CaseInsensitive) == 0)
            return RuntimeVendor::Zulu;
        fail(QStringLiteral("Runtime preferences contain an unknown Java vendor."));
    }

    QString JavaRuntimeManager::selectOfficialRuntime(const QJsonObject& index,
                                                      const QString& platform,
                                                      const QString& component,
                                                      QJsonObject* selectedVersion)
    {
        const QJsonArray candidates = index.value(platform).toObject().value(component).toArray();
        if (candidates.isEmpty())
        {
            fail(QStringLiteral("Mojang has no %1 runtime for %2.").arg(component, platform));
        }
        QJsonObject bestManifest;
        QJsonObject bestVersion;
        QString bestRelease;
        for (const QJsonValue& value : candidates)
        {
            if (!value.isObject())
                continue;
            const QJsonObject entry = value.toObject();
            const QJsonObject manifest = entry.value(QStringLiteral("manifest")).toObject();
            const QJsonObject version = entry.value(QStringLiteral("version")).toObject();
            const QString release = requiredString(version, QStringLiteral("released"));
            const QUrl url(requiredString(manifest, QStringLiteral("url")));
            checkHttpsHost(url, {QStringLiteral("mojang.com")});
            const QString checksum = requiredString(manifest, QStringLiteral("sha1"));
            static const QRegularExpression checksumPattern(QStringLiteral("^[0-9a-fA-F]{40}$"));
            if (!checksumPattern.match(checksum).hasMatch())
            {
                fail(QStringLiteral("Mojang returned an invalid runtime manifest checksum."));
            }
            if (bestManifest.isEmpty() || release > bestRelease)
            {
                bestRelease = release;
                bestManifest = manifest;
                bestVersion = version;
            }
        }
        if (bestManifest.isEmpty())
        {
            fail(QStringLiteral("Mojang's runtime index is invalid."));
        }
        if (selectedVersion != nullptr)
        {
            *selectedVersion = bestVersion;
        }
        const QString version = requiredString(bestVersion, QStringLiteral("name"));
        if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9._+-]{1,128}$"))
                 .match(version)
                 .hasMatch())
        {
            fail(QStringLiteral("Mojang runtime metadata has an invalid version."));
        }
        return version;
    }

    QList<RuntimeFile> JavaRuntimeManager::parseRuntimeFiles(const QJsonObject& manifest)
    {
        const QJsonObject files = manifest.value(QStringLiteral("files")).toObject();
        if (files.isEmpty())
        {
            fail(QStringLiteral("Mojang runtime manifest contains no files."));
        }
        QList<RuntimeFile> result;
        for (auto item = files.begin(); item != files.end(); ++item)
        {
            const QString path = item.key();
            validateRuntimePath(QStringLiteral("/runtime-root"), path);
            for (const QString& metadataName : runtimeMetadataNames)
            {
                if (path == metadataName)
                {
                    fail(
                        QStringLiteral("Mojang runtime manifest collides with launcher metadata."));
                }
            }
            if (!item.value().isObject())
            {
                fail(QStringLiteral("Mojang runtime manifest contains an invalid file entry."));
            }
            const QJsonObject entry = item.value().toObject();
            const QString type = requiredString(entry, QStringLiteral("type"));
            if (type == QStringLiteral("directory"))
            {
                result.append({path, RuntimeFile::Type::Directory, {}, {}, false, {}});
            }
            else if (type == QStringLiteral("link"))
            {
                const QString target = requiredString(entry, QStringLiteral("target"));
                safeLink(QStringLiteral("/runtime-root"), path, target);
                result.append({path, RuntimeFile::Type::Link, {}, {}, false, target});
            }
            else if (type == QStringLiteral("file"))
            {
                const QJsonObject raw = entry.value(QStringLiteral("downloads"))
                                            .toObject()
                                            .value(QStringLiteral("raw"))
                                            .toObject();
                const QUrl url(requiredString(raw, QStringLiteral("url")));
                checkHttpsHost(url, {QStringLiteral("mojang.com")});
                const QString checksum = requiredString(raw, QStringLiteral("sha1"));
                static const QRegularExpression checksumPattern(
                    QStringLiteral("^[0-9a-fA-F]{40}$"));
                if (!checksumPattern.match(checksum).hasMatch())
                {
                    fail(QStringLiteral("Mojang runtime file has an invalid SHA-1 checksum."));
                }
                result.append({path,
                               RuntimeFile::Type::File,
                               url,
                               checksum,
                               entry.value(QStringLiteral("executable")).toBool(),
                               {}});
            }
            else
            {
                fail(QStringLiteral("Unsupported Mojang runtime file type: %1").arg(type));
            }
        }
        return result;
    }

    QString JavaRuntimeManager::runtimeDirectory(const QString& ownerDirectory,
                                                 const QString& dataDirectory,
                                                 const QString& component, bool redundant)
    {
        if (!safeComponent(component))
        {
            fail(QStringLiteral("Invalid Java runtime component."));
        }
        const QString owner = QDir::cleanPath(QFileInfo(ownerDirectory).absoluteFilePath());
        const QString data = QDir::cleanPath(QFileInfo(dataDirectory).absoluteFilePath());
        const QString instancesRoot = QDir(data).filePath(QStringLiteral("instances"));
        if (QDir::cleanPath(QFileInfo(owner).dir().absolutePath()) !=
            QDir::cleanPath(instancesRoot))
        {
            fail(QStringLiteral("Java runtime owner is outside the launcher instances directory."));
        }
        const QString root = redundant ? QDir(owner).filePath(QStringLiteral("java-runtimes"))
                                       : QDir(data).filePath(QStringLiteral("java-runtimes"));
        const QString result = QDir::cleanPath(QDir(root).filePath(component));
        if (!result.startsWith(QDir::cleanPath(root) + QDir::separator()))
        {
            fail(QStringLiteral("Java runtime path is outside approved runtime folders."));
        }
        if (redundant ? hasSymlinkBetween(owner, result) : hasSymlinkBetween(data, result))
        {
            fail(QStringLiteral("Java runtime path cannot pass through a symbolic link."));
        }
        return result;
    }

    QString JavaRuntimeManager::validateRuntimePath(const QString& root, const QString& relative)
    {
        if (relative.isEmpty() || relative.startsWith(QLatin1Char('/')) ||
            relative.contains(QLatin1Char('\\')) || relative.contains(QLatin1Char(':')))
        {
            fail(QStringLiteral("Java runtime manifest contains an unsafe path."));
        }
        const QStringList parts = relative.split(QLatin1Char('/'), Qt::KeepEmptyParts);
        if (std::any_of(parts.cbegin(), parts.cend(),
                        [](const QString& part)
                        {
                            return part.isEmpty() || part == QStringLiteral(".") ||
                                   part == QStringLiteral("..");
                        }))
        {
            fail(QStringLiteral("Java runtime manifest contains an unsafe path."));
        }
        const QString cleanRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
        const QString resolved = QDir::cleanPath(QDir(cleanRoot).filePath(relative));
        if (!resolved.startsWith(cleanRoot + QDir::separator()))
        {
            fail(QStringLiteral("Java runtime manifest path escapes its installation directory."));
        }
        return resolved;
    }

    void JavaRuntimeManager::extractArchive(const QString& archivePath, const QString& destination)
    {
        if (QFileInfo(destination).isSymLink())
        {
            fail(QStringLiteral("Java runtime extraction destination is a symbolic link."));
        }
        if (!QDir().mkpath(destination))
        {
            fail(QStringLiteral("Unable to create Java runtime staging directory."));
        }
        std::unique_ptr<struct archive, decltype(&archive_read_free)> reader(archive_read_new(),
                                                                             &archive_read_free);
        if (!reader)
        {
            fail(QStringLiteral("Unable to allocate Java runtime archive reader."));
        }
        archive_read_support_format_all(reader.get());
        archive_read_support_filter_all(reader.get());
        const QByteArray encoded = QFile::encodeName(archivePath);
        if (archive_read_open_filename(reader.get(), encoded.constData(), 64 * 1024) != ARCHIVE_OK)
        {
            fail(QStringLiteral("Unable to open Java runtime archive."));
        }
        int entries = 0;
        qint64 totalBytes = 0;
        struct archive_entry* entry = nullptr;
        int status = ARCHIVE_OK;
        while ((status = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK)
        {
            if (++entries > maxArchiveEntries)
            {
                fail(QStringLiteral("Java runtime archive contains too many files."));
            }
            const char* rawPath = archive_entry_pathname(entry);
            if (rawPath == nullptr)
            {
                fail(QStringLiteral("Java runtime archive contains an invalid path."));
            }
            const auto type = archive_entry_filetype(entry);
            QString relative = QString::fromUtf8(rawPath);
            if (type == AE_IFDIR)
            {
                while (relative.endsWith(QLatin1Char('/')))
                {
                    relative.chop(1);
                }
            }
            const QString target = safeArchivePath(destination, relative);
            if (type == AE_IFDIR)
            {
                if (!QDir().mkpath(target))
                {
                    fail(QStringLiteral("Unable to create Java runtime directory."));
                }
                continue;
            }
            if (archive_entry_hardlink(entry) != nullptr)
            {
                fail(QStringLiteral("Java runtime archive contains an unsupported hard link."));
            }
            if (archive_entry_symlink(entry) != nullptr)
            {
                const QString linkTarget = QString::fromUtf8(archive_entry_symlink(entry));
                safeLink(destination, relative, linkTarget);
                if (!QDir().mkpath(QFileInfo(target).absolutePath()) ||
                    !QFile::link(linkTarget, target))
                {
                    fail(QStringLiteral("Unable to create Java runtime symbolic link."));
                }
                archive_read_data_skip(reader.get());
                continue;
            }
            if (type != AE_IFREG)
            {
                fail(QStringLiteral("Java runtime archive contains an unsupported file type."));
            }
            if (!QDir().mkpath(QFileInfo(target).absolutePath()))
            {
                fail(QStringLiteral("Unable to create Java runtime file directory."));
            }
            QFile output(target);
            if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
            {
                fail(QStringLiteral("Unable to write Java runtime file: %1")
                         .arg(output.errorString()));
            }
            char buffer[64 * 1024];
            la_ssize_t bytes = 0;
            while ((bytes = archive_read_data(reader.get(), buffer, sizeof(buffer))) > 0)
            {
                totalBytes += bytes;
                if (totalBytes > maxExpandedBytes || output.write(buffer, bytes) != bytes)
                {
                    fail(QStringLiteral(
                        "Java runtime archive exceeds the extraction limit or failed to write."));
                }
            }
            if (bytes < 0)
            {
                fail(QStringLiteral("Unable to extract Java runtime archive."));
            }
            if ((archive_entry_perm(entry) & 0111) != 0)
            {
                markExecutable(target);
            }
        }
        if (status != ARCHIVE_EOF)
        {
            fail(QStringLiteral("Unable to read Java runtime archive."));
        }
    }

    QString JavaRuntimeManager::installMojang(const QString& ownerDirectory,
                                              const QString& versionId, const QJsonObject& metadata,
                                              bool redundant,
                                              const ProgressCallback& progress) const
    {
        const QString component = requiredComponent(metadata, versionId);
        const QJsonObject index =
            parseObject(fetchBytes(runtimeIndexUrl, {QStringLiteral("mojang.com")}),
                        QStringLiteral("Mojang runtime index"));
        QJsonObject selectedVersion;
        const QString runtimeVersion = selectOfficialRuntime(
            index, platformName(hostOs(), hostArchitecture()), component, &selectedVersion);
        const QJsonObject manifestInfo =
            selectedVersion.value(QStringLiteral("manifest")).toObject();
        const QUrl manifestUrl(requiredString(manifestInfo, QStringLiteral("url")));
        const QString manifestSha1 = requiredString(manifestInfo, QStringLiteral("sha1"));
        const QString manifestText = fetchRuntimeMetadata(manifestUrl, manifestSha1);
        const QList<RuntimeFile> files = parseRuntimeFiles(
            parseObject(manifestText.toUtf8(), QStringLiteral("Mojang runtime manifest")));
        const QString platform = platformName(hostOs(), hostArchitecture());
        const QStringList executableCandidates =
            platform.startsWith(QStringLiteral("windows"))
                ? QStringList{QStringLiteral("bin/javaw.exe"), QStringLiteral("bin/java.exe")}
                : QStringList{QStringLiteral("bin/java"), QStringLiteral("Contents/Home/bin/java")};
        QString executableRelative;
        for (const RuntimeFile& file : files)
        {
            if (file.type != RuntimeFile::Type::File)
            {
                continue;
            }
            for (const QString& candidate : executableCandidates)
            {
                if (file.path.endsWith(candidate))
                {
                    executableRelative = file.path;
                    break;
                }
            }
            if (!executableRelative.isEmpty())
            {
                break;
            }
        }
        if (executableRelative.isEmpty())
        {
            fail(QStringLiteral("Mojang runtime manifest has no Java executable."));
        }

        const QString runtimeDirectory =
            managedRuntimeDirectory(ownerDirectory, component, redundant);
        const QString executable = QDir(runtimeDirectory).filePath(executableRelative);
        const QString markerPath = QDir(runtimeDirectory).filePath(javaPathMarker);
        QFile marker(markerPath);
        QFile versionFile(QDir(runtimeDirectory).filePath(versionMarker));
        if (marker.open(QIODevice::ReadOnly) && versionFile.open(QIODevice::ReadOnly))
        {
            const QStringList markerLines =
                QString::fromUtf8(marker.readAll())
                    .split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
            if (markerLines.size() == 2 && markerLines.first() == executableRelative &&
                QString::fromUtf8(versionFile.readAll()).trimmed() == runtimeVersion &&
                QFileInfo(executable).isFile() &&
                markerLines.at(1) == digestFile(executable, QCryptographicHash::Sha1) &&
                (isWindows() || QFileInfo(executable).isExecutable()))
            {
                if (progress)
                    progress(
                        {1, 1, QStringLiteral("Java %1 is already installed").arg(runtimeVersion)});
                return component;
            }
        }

        const QString runtimeRoot = QFileInfo(runtimeDirectory).absolutePath();
        if (QFileInfo(ownerDirectory).isSymLink() || QFileInfo(runtimeRoot).isSymLink() ||
            !QDir().mkpath(runtimeRoot))
        {
            fail(QStringLiteral("Unable to safely create Java runtime storage."));
        }
        const QString staging =
            QDir(runtimeRoot)
                .filePath(QStringLiteral(".staging-%1")
                              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
        if (!QDir().mkpath(staging))
        {
            fail(QStringLiteral("Unable to create Java runtime staging folder."));
        }
        try
        {
            QList<download::DownloadTask> downloads;
            for (const RuntimeFile& file : files)
            {
                const QString target = validateRuntimePath(staging, file.path);
                if (file.type == RuntimeFile::Type::Directory)
                {
                    if (!QDir().mkpath(target))
                        fail(QStringLiteral("Unable to stage Java runtime directory."));
                }
                else if (file.type == RuntimeFile::Type::File)
                {
                    downloads.append({file.url, target, file.sha1,
                                      QStringLiteral("Java %1 %2").arg(runtimeVersion, file.path),
                                      maxArchiveBytes});
                }
            }
            m_downloadManager.downloadAll(downloads, progress);
            for (const RuntimeFile& file : files)
            {
                if (file.type == RuntimeFile::Type::Link)
                {
                    const QString destination = validateRuntimePath(staging, file.path);
                    safeLink(staging, file.path, file.target);
                    if (!QDir().mkpath(QFileInfo(destination).absolutePath()) ||
                        !QFile::link(file.target, destination))
                    {
                        fail(QStringLiteral("Unable to create Mojang runtime symbolic link."));
                    }
                }
                else if (file.type == RuntimeFile::Type::File && file.executable)
                {
                    markExecutable(validateRuntimePath(staging, file.path));
                }
            }
            const QString javaExecutable = validateRuntimePath(staging, executableRelative);
            writeMarker(QDir(staging).filePath(javaPathMarker),
                        (executableRelative + QLatin1Char('\n') +
                         digestFile(javaExecutable, QCryptographicHash::Sha1) + QLatin1Char('\n'))
                            .toUtf8());
            writeMarker(QDir(staging).filePath(versionMarker), runtimeVersion.toUtf8());
            writeMarker(QDir(staging).filePath(vendorMarker), QByteArrayLiteral("MOJANG"));
            replaceRuntime(staging, runtimeDirectory);
            if (progress)
            {
                progress({1, 1,
                          QStringLiteral("Installed Java %1 for Minecraft %2")
                              .arg(runtimeVersion, versionId)});
            }
        }
        catch (...)
        {
            removeRuntimeTree(staging);
            throw;
        }
        return component;
    }

    QString JavaRuntimeManager::installExternal(const QString& ownerDirectory,
                                                const QString& versionId,
                                                const QJsonObject& metadata, RuntimeVendor vendor,
                                                bool redundant,
                                                const ProgressCallback& progress) const
    {
        const int major = requiredJavaMajor(metadata, versionId);
        const QString component = QStringLiteral("%1-java-%2").arg(vendorName(vendor)).arg(major);
        const QString runtimeDirectory =
            managedRuntimeDirectory(ownerDirectory, component, redundant);
        const RuntimePackage package = fetchExternalPackage(vendor, major);
        QFile installedVersion(QDir(runtimeDirectory).filePath(versionMarker));
        QFile installedMarker(QDir(runtimeDirectory).filePath(javaPathMarker));
        if (installedVersion.open(QIODevice::ReadOnly) &&
            installedMarker.open(QIODevice::ReadOnly) &&
            QString::fromUtf8(installedVersion.readAll()).trimmed() == package.version)
        {
            const QStringList markerLines =
                QString::fromUtf8(installedMarker.readAll())
                    .split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
            if (markerLines.size() == 2 && QRegularExpression(QStringLiteral("^[0-9a-f]{40}$"))
                                               .match(markerLines.at(1))
                                               .hasMatch())
            {
                const QString installedExecutable =
                    validateRuntimePath(runtimeDirectory, markerLines.first());
                if (QFileInfo(installedExecutable).isFile() &&
                    !QFileInfo(installedExecutable).isSymLink() &&
                    (isWindows() || QFileInfo(installedExecutable).isExecutable()) &&
                    digestFile(installedExecutable, QCryptographicHash::Sha1) == markerLines.at(1))
                {
                    if (progress)
                    {
                        progress({1, 1,
                                  QStringLiteral("%1 Java %2 is already installed")
                                      .arg(vendorName(vendor), package.version)});
                    }
                    return component;
                }
            }
        }
        if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9._+-]{1,128}$"))
                 .match(package.version)
                 .hasMatch())
        {
            fail(QStringLiteral("Java runtime provider returned an invalid runtime version."));
        }
        const QString runtimeRoot = QFileInfo(runtimeDirectory).absolutePath();
        if (QFileInfo(ownerDirectory).isSymLink() || QFileInfo(runtimeRoot).isSymLink() ||
            !QDir().mkpath(runtimeRoot))
        {
            fail(QStringLiteral("Unable to safely create Java runtime storage."));
        }
        const QString staging =
            QDir(runtimeRoot)
                .filePath(QStringLiteral(".staging-%1")
                              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
        if (!QDir().mkpath(staging))
        {
            fail(QStringLiteral("Unable to create Java runtime staging folder."));
        }
        const QString archivePath =
            QDir(runtimeRoot)
                .filePath(QStringLiteral(".runtime-%1.archive")
                              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
        try
        {
            if (progress)
                progress({0, 1,
                          QStringLiteral("Downloading %1 Java %2")
                              .arg(vendorName(vendor), package.version)});
            downloadExternalArchive(package, archivePath);
            extractArchive(archivePath, staging);
            const QString executableRelative = findJavaExecutable(staging);
            const QString executable = validateRuntimePath(staging, executableRelative);
            markExecutable(executable);
            writeMarker(QDir(staging).filePath(javaPathMarker),
                        (executableRelative + QLatin1Char('\n') +
                         digestFile(executable, QCryptographicHash::Sha1) + QLatin1Char('\n'))
                            .toUtf8());
            writeMarker(QDir(staging).filePath(versionMarker), package.version.toUtf8());
            writeMarker(QDir(staging).filePath(vendorMarker),
                        vendorName(vendor).toUpper().toUtf8());
            replaceRuntime(staging, runtimeDirectory);
            if (progress)
                progress({1, 1,
                          QStringLiteral("Installed %1 Java %2")
                              .arg(vendorName(vendor), package.version)});
        }
        catch (...)
        {
            QFile::remove(archivePath);
            removeRuntimeTree(staging);
            throw;
        }
        QFile::remove(archivePath);
        return component;
    }

    QString JavaRuntimeManager::javaExecutable(const QString& ownerDirectory,
                                               const QString& versionId, const QString& component,
                                               bool redundant) const
    {
        if (!safeComponent(component))
        {
            fail(QStringLiteral("Invalid managed Java runtime identifier."));
        }
        const QString candidates[] = {
            managedRuntimeDirectory(ownerDirectory, component, redundant),
            managedRuntimeDirectory(ownerDirectory, component, !redundant)};
        for (const QString& directory : candidates)
        {
            if (hasSymlinkBetween(directory, directory))
            {
                continue;
            }
            QFile marker(QDir(directory).filePath(javaPathMarker));
            if (!marker.open(QIODevice::ReadOnly))
            {
                continue;
            }
            const QStringList lines =
                QString::fromUtf8(marker.readAll())
                    .split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
            QFile versionFile(QDir(directory).filePath(versionMarker));
            if (lines.size() != 2 ||
                !QRegularExpression(QStringLiteral("^[0-9a-f]{40}$"))
                     .match(lines.at(1))
                     .hasMatch() ||
                !versionFile.open(QIODevice::ReadOnly) ||
                QString::fromUtf8(versionFile.readAll()).trimmed().isEmpty())
            {
                continue;
            }
            const QString executable = validateRuntimePath(directory, lines.first());
            if (!QFileInfo(executable).isFile() || hasSymlinkBetween(directory, executable) ||
                (!isWindows() && !QFileInfo(executable).isExecutable()))
            {
                continue;
            }
            if (digestFile(executable, QCryptographicHash::Sha1) != lines.at(1))
            {
                fail(QStringLiteral(
                    "Managed Java runtime failed integrity verification; reinstall it."));
            }
            return executable;
        }
        fail(QStringLiteral(
                 "The managed Java runtime for Minecraft %1 is not installed. Install it in this "
                 "instance's Java runtime settings or select System Java.")
                 .arg(versionId));
    }

    QString JavaRuntimeManager::fetchRuntimeMetadata(const QUrl& url,
                                                     const QString& expectedSha1) const
    {
        checkHttpsHost(url, {QStringLiteral("mojang.com")});
        const QByteArray data = fetchBytes(url, {QStringLiteral("mojang.com")});
        if (!expectedSha1.isEmpty() && sha1(data).compare(expectedSha1, Qt::CaseInsensitive) != 0)
        {
            fail(QStringLiteral("Mojang Java runtime manifest checksum does not match."));
        }
        return QString::fromUtf8(data);
    }

    QString JavaRuntimeManager::managedRuntimeDirectory(const QString& ownerDirectory,
                                                        const QString& component,
                                                        bool redundant) const
    {
        return runtimeDirectory(ownerDirectory, m_dataDirectory, component, redundant);
    }
} // namespace strata::runtime
