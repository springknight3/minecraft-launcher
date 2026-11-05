#include "MinecraftInstaller.h"

#include "../Paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>

#include <algorithm>
#include <stdexcept>

namespace strata::mojang
{
    namespace
    {
        [[noreturn]] void throwInstallError(const QString& message)
        {
            throw std::runtime_error(message.toStdString());
        }

        QString requiredString(const QJsonObject& object, const QString& key)
        {
            const QJsonValue value = object.value(key);
            if (!value.isString() || value.toString().isEmpty())
            {
                throwInstallError(
                    QStringLiteral("Minecraft version metadata is missing %1").arg(key));
            }
            return value.toString();
        }

        QString safeVersionId(const QString& versionId)
        {
            static const QRegularExpression pattern(
                QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+() -]{0,127}$"));
            if (!pattern.match(versionId).hasMatch() || versionId.endsWith(QLatin1Char(' ')) ||
                versionId.contains(QStringLiteral("..")))
            {
                throwInstallError(QStringLiteral("Invalid Minecraft version identifier"));
            }
            return versionId;
        }

        QString safeFilePath(const QString& root, const QString& relativePath)
        {
            const QString normalized = QDir::fromNativeSeparators(relativePath);
            if (normalized.isEmpty() || normalized.startsWith(QLatin1Char('/')) ||
                normalized.contains(QLatin1Char('\\')))
            {
                throwInstallError(QStringLiteral("Unsafe path in Minecraft metadata"));
            }
            const QStringList segments = normalized.split(QLatin1Char('/'), Qt::KeepEmptyParts);
            if (std::any_of(segments.cbegin(), segments.cend(),
                            [](const QString& segment)
                            {
                                return segment.isEmpty() || segment == QStringLiteral(".") ||
                                       segment == QStringLiteral("..");
                            }))
            {
                throwInstallError(QStringLiteral("Unsafe path in Minecraft metadata"));
            }
            const QString normalizedRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
            const QString result = QDir::cleanPath(QDir(normalizedRoot).filePath(normalized));
            if (!result.startsWith(normalizedRoot + QDir::separator()))
            {
                throwInstallError(QStringLiteral("Unsafe path in Minecraft metadata"));
            }
            return result;
        }

        qint64 artifactSize(const QJsonObject& artifact)
        {
            const QJsonValue size = artifact.value(QStringLiteral("size"));
            if (!size.isDouble())
            {
                return 2LL * 1024 * 1024 * 1024;
            }
            const double value = size.toDouble();
            if (value <= 0 || value > 2.0 * 1024 * 1024 * 1024)
            {
                throwInstallError(QStringLiteral("Minecraft artifact has an invalid size"));
            }
            return static_cast<qint64>(value);
        }

        QString currentOsName()
        {
#if defined(Q_OS_WIN)
            return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
            return QStringLiteral("osx");
#else
            return QStringLiteral("linux");
#endif
        }

        QString nativeArchitecture()
        {
#if defined(Q_PROCESSOR_X86_64)
            return QStringLiteral("64");
#elif defined(Q_PROCESSOR_X86_32)
            return QStringLiteral("32");
#elif defined(Q_PROCESSOR_ARM_64)
            return QStringLiteral("64");
#else
            return QStringLiteral("32");
#endif
        }

        bool allowedByRules(const QJsonObject& item)
        {
            const QJsonValue rulesValue = item.value(QStringLiteral("rules"));
            if (!rulesValue.isArray())
            {
                return true;
            }
            bool allowed = false;
            const QString osName = currentOsName();
            for (const QJsonValue& value : rulesValue.toArray())
            {
                if (!value.isObject())
                {
                    continue;
                }
                const QJsonObject rule = value.toObject();
                const QJsonObject os = rule.value(QStringLiteral("os")).toObject();
                if (!os.value(QStringLiteral("name")).toString().isEmpty() &&
                    os.value(QStringLiteral("name")).toString() != osName)
                {
                    continue;
                }
                const QString arch = os.value(QStringLiteral("arch")).toString();
                if (!arch.isEmpty() &&
                    !QRegularExpression(arch).match(QSysInfo::currentCpuArchitecture()).hasMatch())
                {
                    continue;
                }
                const QJsonObject features = rule.value(QStringLiteral("features")).toObject();
                bool matchesFeatures = true;
                for (auto feature = features.begin(); feature != features.end(); ++feature)
                {
                    if (!feature.value().isBool() || feature.value().toBool())
                    {
                        matchesFeatures = false;
                        break;
                    }
                }
                if (matchesFeatures)
                {
                    allowed =
                        rule.value(QStringLiteral("action")).toString() == QStringLiteral("allow");
                }
            }
            return allowed;
        }

        void addArtifact(QList<download::DownloadTask>& tasks, const QJsonObject& artifact,
                         const QString& root, const QString& description)
        {
            const QUrl url(requiredString(artifact, QStringLiteral("url")));
            const QString path =
                safeFilePath(root, requiredString(artifact, QStringLiteral("path")));
            const QString sha1 = artifact.value(QStringLiteral("sha1")).toString();
            tasks.append({url, path, sha1, description, artifactSize(artifact)});
        }
    } // namespace

    MinecraftInstaller::MinecraftInstaller(const download::DownloadManager& downloadManager,
                                           QString dataDirectory)
        : m_downloadManager(downloadManager),
          m_dataDirectory(dataDirectory.isEmpty() ? Paths::dataDirectory()
                                                  : QFileInfo(dataDirectory).absoluteFilePath())
    {
    }

    void MinecraftInstaller::install(const strata::Instance& instance, const Version& version,
                                     download::DownloadManager::ProgressCallback progress) const
    {
        const QString versionId = safeVersionId(version.id);
        const QString versionDirectory =
            QDir(instance.gameDirectory()).filePath(QStringLiteral("versions/%1").arg(versionId));
        const QString metadataPath =
            QDir(versionDirectory).filePath(versionId + QStringLiteral(".json"));

        if (progress)
        {
            progress({0, 2, QStringLiteral("Minecraft %1 metadata").arg(versionId)});
        }
        m_downloadManager.downloadAll({{version.url, metadataPath, version.sha1,
                                        QStringLiteral("Minecraft %1 metadata").arg(versionId)}});
        QFile metadataFile(metadataPath);
        if (!metadataFile.open(QIODevice::ReadOnly))
        {
            throwInstallError(QStringLiteral("Unable to read downloaded Minecraft metadata: %1")
                                  .arg(metadataFile.errorString()));
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(metadataFile.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            throwInstallError(QStringLiteral("Downloaded Minecraft version metadata is invalid: %1")
                                  .arg(parseError.errorString()));
        }

        const QJsonObject versionMetadata = document.object();
        const QJsonObject downloads = versionMetadata.value(QStringLiteral("downloads")).toObject();
        const QJsonObject client = downloads.value(QStringLiteral("client")).toObject();
        QList<download::DownloadTask> initialTasks;
        const QString clientPath =
            QDir(versionDirectory).filePath(versionId + QStringLiteral(".jar"));
        initialTasks.append({QUrl(requiredString(client, QStringLiteral("url"))), clientPath,
                             client.value(QStringLiteral("sha1")).toString(),
                             QStringLiteral("Minecraft %1 client").arg(versionId),
                             artifactSize(client)});

        const QString librariesDirectory =
            QDir(m_dataDirectory).filePath(QStringLiteral("libraries"));
        const QString assetsDirectory = QDir(m_dataDirectory).filePath(QStringLiteral("assets"));
        const QJsonValue librariesValue = versionMetadata.value(QStringLiteral("libraries"));
        QList<QString> nativeJars;
        if (librariesValue.isArray())
        {
            for (const QJsonValue& libraryValue : librariesValue.toArray())
            {
                if (!libraryValue.isObject())
                {
                    throwInstallError(
                        QStringLiteral("Minecraft version metadata contains an invalid library"));
                }
                const QJsonObject library = libraryValue.toObject();
                if (!allowedByRules(library))
                {
                    continue;
                }
                const QString name = requiredString(library, QStringLiteral("name"));
                const QJsonObject libraryDownloads =
                    library.value(QStringLiteral("downloads")).toObject();
                const QJsonObject artifact =
                    libraryDownloads.value(QStringLiteral("artifact")).toObject();
                if (!artifact.isEmpty())
                {
                    addArtifact(initialTasks, artifact, librariesDirectory, name);
                }
                else
                {
                    const QStringList coordinates = name.split(QLatin1Char(':'));
                    if (coordinates.size() < 3)
                    {
                        throwInstallError(QStringLiteral("Invalid Minecraft library coordinates"));
                    }
                    QString group = coordinates.at(0);
                    group.replace(QLatin1Char('.'), QLatin1Char('/'));
                    const QString relative =
                        group + QLatin1Char('/') + coordinates.at(1) + QLatin1Char('/') +
                        coordinates.at(2) + QLatin1Char('/') + coordinates.at(1) +
                        QLatin1Char('-') + coordinates.at(2) + QStringLiteral(".jar");
                    QString repository = library.value(QStringLiteral("url")).toString();
                    if (repository.isEmpty())
                    {
                        repository = QStringLiteral("https://libraries.minecraft.net/");
                    }
                    if (!repository.endsWith(QLatin1Char('/')))
                    {
                        repository.append(QLatin1Char('/'));
                    }
                    initialTasks.append({QUrl(repository + relative),
                                         safeFilePath(librariesDirectory, relative),
                                         {},
                                         QFileInfo(relative).fileName()});
                }

                const QJsonObject natives = library.value(QStringLiteral("natives")).toObject();
                const QString nativeKey = currentOsName();
                const QString nativePattern = natives.value(nativeKey).toString();
                const QJsonObject classifiers =
                    libraryDownloads.value(QStringLiteral("classifiers")).toObject();
                if (!nativePattern.isEmpty() && !classifiers.isEmpty())
                {
                    QString classifier = nativePattern;
                    classifier.replace(QStringLiteral("${arch}"), nativeArchitecture());
                    const QJsonObject nativeArtifact = classifiers.value(classifier).toObject();
                    if (!nativeArtifact.isEmpty())
                    {
                        addArtifact(initialTasks, nativeArtifact, librariesDirectory,
                                    QStringLiteral("%1 natives").arg(name));
                        nativeJars.append(
                            safeFilePath(librariesDirectory,
                                         requiredString(nativeArtifact, QStringLiteral("path"))));
                    }
                }
            }
        }

        if (progress)
        {
            progress({0, 0, QStringLiteral("Downloading Minecraft files")});
        }

        const QJsonObject assetIndex =
            versionMetadata.value(QStringLiteral("assetIndex")).toObject();
        QString assetIndexPath;
        QString assetIndexId;
        if (!assetIndex.isEmpty())
        {
            assetIndexId = requiredString(assetIndex, QStringLiteral("id"));
            const QStringList assetIndexParts = assetIndexId.split(QLatin1Char('/'));
            if (assetIndexParts.size() != 1 || assetIndexId == QStringLiteral(".") ||
                assetIndexId == QStringLiteral(".."))
            {
                throwInstallError(QStringLiteral("Invalid Minecraft asset index identifier"));
            }
            assetIndexPath = safeFilePath(QDir(assetsDirectory).filePath(QStringLiteral("indexes")),
                                          assetIndexId + QStringLiteral(".json"));
            initialTasks.append({QUrl(requiredString(assetIndex, QStringLiteral("url"))),
                                 assetIndexPath,
                                 assetIndex.value(QStringLiteral("sha1")).toString(),
                                 QStringLiteral("Minecraft asset index %1").arg(assetIndexId),
                                 artifactSize(assetIndex)});
        }
        const QJsonObject logging = versionMetadata.value(QStringLiteral("logging"))
                                        .toObject()
                                        .value(QStringLiteral("client"))
                                        .toObject();
        if (logging.value(QStringLiteral("file")).isObject())
        {
            const QJsonObject loggingFile = logging.value(QStringLiteral("file")).toObject();
            const QString loggingId = requiredString(loggingFile, QStringLiteral("id"));
            initialTasks.append(
                {QUrl(requiredString(loggingFile, QStringLiteral("url"))),
                 safeFilePath(QDir(assetsDirectory).filePath(QStringLiteral("log_configs")),
                              loggingId),
                 loggingFile.value(QStringLiteral("sha1")).toString(),
                 QStringLiteral("Minecraft logging configuration"), artifactSize(loggingFile)});
        }
        m_downloadManager.downloadAll(initialTasks, progress);

        if (!assetIndexPath.isEmpty())
        {
            QFile assetIndexFile(assetIndexPath);
            if (!assetIndexFile.open(QIODevice::ReadOnly))
            {
                throwInstallError(QStringLiteral("Unable to read Minecraft asset index: %1")
                                      .arg(assetIndexFile.errorString()));
            }
            QJsonParseError assetError;
            const QJsonDocument assetDocument =
                QJsonDocument::fromJson(assetIndexFile.readAll(), &assetError);
            if (assetError.error != QJsonParseError::NoError || !assetDocument.isObject())
            {
                throwInstallError(QStringLiteral("Minecraft asset index is invalid: %1")
                                      .arg(assetError.errorString()));
            }
            const QJsonObject objects =
                assetDocument.object().value(QStringLiteral("objects")).toObject();
            if (objects.size() > 200'000)
            {
                throwInstallError(
                    QStringLiteral("Minecraft asset index exceeds the supported object limit"));
            }
            QList<download::DownloadTask> assetTasks;
            for (auto object = objects.begin(); object != objects.end(); ++object)
            {
                if (!object.value().isObject())
                {
                    throwInstallError(
                        QStringLiteral("Minecraft asset index contains an invalid entry"));
                }
                const QJsonObject asset = object.value().toObject();
                const QString hash = requiredString(asset, QStringLiteral("hash")).toLower();
                static const QRegularExpression hashPattern(QStringLiteral("^[0-9a-f]{40}$"));
                if (!hashPattern.match(hash).hasMatch())
                {
                    throwInstallError(
                        QStringLiteral("Minecraft asset index contains an invalid hash"));
                }
                const QString relative = hash.left(2) + QLatin1Char('/') + hash;
                const double sizeValue = asset.value(QStringLiteral("size")).toDouble();
                const qint64 size = sizeValue > 0 && sizeValue <= 512.0 * 1024 * 1024
                                        ? static_cast<qint64>(sizeValue)
                                        : 512LL * 1024 * 1024;
                assetTasks.append(
                    {QUrl(QStringLiteral("https://resources.download.minecraft.net/%1")
                              .arg(relative)),
                     safeFilePath(QDir(assetsDirectory).filePath(QStringLiteral("objects")),
                                  relative),
                     hash, object.key(), size});
            }
            m_downloadManager.downloadAll(assetTasks, progress);
        }

        for (const QString& nativeJar : nativeJars)
        {
            Q_UNUSED(nativeJar)
        }
        const QString marker =
            QDir(instance.gameDirectory()).filePath(QStringLiteral(".stratalauncher-installed"));
        QSaveFile markerFile(marker);
        if (!markerFile.open(QIODevice::WriteOnly) ||
            markerFile.write(versionId.toUtf8()) != versionId.toUtf8().size() ||
            !markerFile.commit())
        {
            throwInstallError(QStringLiteral("Unable to mark the Minecraft installation complete"));
        }
        if (progress)
        {
            progress({2, 2, QStringLiteral("Minecraft %1 client").arg(versionId)});
        }
    }
} // namespace strata::mojang
