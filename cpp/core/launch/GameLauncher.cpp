#include "GameLauncher.h"

#include "../settings/InstanceSettings.h"

#include <archive.h>
#include <archive_entry.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMap>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>

#include <algorithm>
#include <memory>
#include <stdexcept>

namespace strata::launch
{
    namespace
    {
        constexpr qsizetype maxMetadataBytes = 32 * 1024 * 1024;
        constexpr int maxNativeEntries = 10'000;
        constexpr qint64 maxNativeBytes = 512LL * 1024 * 1024;

        [[noreturn]] void throwLaunchError(const QString& message)
        {
            throw std::runtime_error(message.toStdString());
        }

        QString osName()
        {
#if defined(Q_OS_WIN)
            return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
            return QStringLiteral("osx");
#else
            return QStringLiteral("linux");
#endif
        }

        QString architecture()
        {
#if defined(Q_PROCESSOR_X86_64)
            return QStringLiteral("x86_64");
#elif defined(Q_PROCESSOR_X86_32)
            return QStringLiteral("x86");
#elif defined(Q_PROCESSOR_ARM_64)
            return QStringLiteral("aarch64");
#elif defined(Q_PROCESSOR_ARM_32)
            return QStringLiteral("arm");
#else
            return QSysInfo::currentCpuArchitecture();
#endif
        }

        QString safePath(const QString& root, const QString& relative)
        {
            const QString normalized = QDir::fromNativeSeparators(relative);
            const QStringList parts = normalized.split(QLatin1Char('/'), Qt::KeepEmptyParts);
            if (normalized.isEmpty() || normalized.startsWith(QLatin1Char('/')) ||
                std::any_of(parts.cbegin(), parts.cend(),
                            [](const QString& part)
                            {
                                return part.isEmpty() || part == QStringLiteral(".") ||
                                       part == QStringLiteral("..");
                            }))
            {
                throwLaunchError(QStringLiteral("Unsafe path in Minecraft metadata"));
            }
            const QString absoluteRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
            const QString result = QDir::cleanPath(QDir(absoluteRoot).filePath(normalized));
            if (!result.startsWith(absoluteRoot + QDir::separator()))
            {
                throwLaunchError(QStringLiteral("Unsafe path in Minecraft metadata"));
            }
            return result;
        }

        QJsonObject readJsonObject(const QString& path)
        {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                throwLaunchError(QStringLiteral("Unable to read Minecraft metadata %1: %2")
                                     .arg(path, file.errorString()));
            }
            if (file.size() > maxMetadataBytes)
            {
                throwLaunchError(
                    QStringLiteral("Minecraft metadata exceeds the supported size limit"));
            }
            QJsonParseError error;
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject())
            {
                throwLaunchError(
                    QStringLiteral("Minecraft metadata is invalid: %1").arg(error.errorString()));
            }
            return document.object();
        }

        QString requiredString(const QJsonObject& object, const QString& key)
        {
            const QJsonValue value = object.value(key);
            if (!value.isString() || value.toString().isEmpty())
            {
                throwLaunchError(QStringLiteral("Minecraft metadata is missing %1").arg(key));
            }
            return value.toString();
        }

        QString substitute(QString value, const QMap<QString, QString>& substitutions)
        {
            for (auto item = substitutions.cbegin(); item != substitutions.cend(); ++item)
            {
                value.replace(QStringLiteral("${%1}").arg(item.key()), item.value());
            }
            return value;
        }

        QString mavenPath(const QString& coordinates)
        {
            const QStringList parts = coordinates.split(QLatin1Char(':'));
            if (parts.size() < 3 || parts.size() > 4)
            {
                throwLaunchError(QStringLiteral("Invalid Minecraft library coordinates"));
            }
            QString group = parts.at(0);
            const QString artifact = parts.at(1);
            const QString version = parts.at(2);
            const QString classifier =
                parts.size() == 4 ? QLatin1Char('-') + parts.at(3) : QString();
            return group.replace(QLatin1Char('.'), QLatin1Char('/')) + QLatin1Char('/') + artifact +
                   QLatin1Char('/') + version + QLatin1Char('/') + artifact + QLatin1Char('-') +
                   version + classifier + QStringLiteral(".jar");
        }

        QString nativeClassifier(QString name)
        {
            name.replace(QStringLiteral("${arch}"), architecture().contains(QStringLiteral("64"))
                                                        ? QStringLiteral("64")
                                                        : QStringLiteral("32"));
            return name;
        }

        QByteArray archiveError(struct archive* reader)
        {
            return QByteArray(archive_error_string(reader) == nullptr
                                  ? "unknown archive error"
                                  : archive_error_string(reader));
        }

        bool safeArchiveEntry(const QString& entry)
        {
            const QString normalized = QDir::fromNativeSeparators(entry);
            if (normalized.isEmpty() || normalized.startsWith(QLatin1Char('/')) ||
                normalized.contains(QLatin1Char(':')))
            {
                return false;
            }
            const QStringList parts = normalized.split(QLatin1Char('/'));
            return !std::any_of(parts.cbegin(), parts.cend(),
                                [](const QString& part) { return part == QStringLiteral(".."); });
        }
    } // namespace

    bool GameLauncher::isAllowedByRules(const QJsonObject& item, const QString& platform,
                                        const QString& arch, const QJsonObject& features)
    {
        const QJsonValue rulesValue = item.value(QStringLiteral("rules"));
        if (!rulesValue.isArray())
        {
            return true;
        }
        bool allowed = false;
        for (const QJsonValue& value : rulesValue.toArray())
        {
            if (!value.isObject())
            {
                continue;
            }
            const QJsonObject rule = value.toObject();
            const QJsonObject os = rule.value(QStringLiteral("os")).toObject();
            if (!os.value(QStringLiteral("name")).toString().isEmpty() &&
                os.value(QStringLiteral("name")).toString() != platform)
            {
                continue;
            }
            const QString archPattern = os.value(QStringLiteral("arch")).toString();
            if (!archPattern.isEmpty() && !QRegularExpression(archPattern).match(arch).hasMatch())
            {
                continue;
            }
            const QString versionPattern = os.value(QStringLiteral("version")).toString();
            if (!versionPattern.isEmpty() &&
                !QRegularExpression(versionPattern).match(QSysInfo::productVersion()).hasMatch())
            {
                continue;
            }
            const QJsonObject requiredFeatures = rule.value(QStringLiteral("features")).toObject();
            bool matchesFeatures = true;
            for (auto feature = requiredFeatures.begin(); feature != requiredFeatures.end();
                 ++feature)
            {
                if (!feature.value().isBool() ||
                    features.value(feature.key()).toBool(false) != feature.value().toBool())
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

    QStringList GameLauncher::resolveArguments(const QJsonArray& arguments,
                                               const QMap<QString, QString>& substitutions,
                                               const QString& platform, const QString& arch)
    {
        QStringList result;
        for (const QJsonValue& argument : arguments)
        {
            if (argument.isString())
            {
                result.append(substitute(argument.toString(), substitutions));
                continue;
            }
            if (!argument.isObject())
            {
                throwLaunchError(QStringLiteral("Minecraft arguments contain an invalid entry"));
            }
            const QJsonObject conditional = argument.toObject();
            if (!isAllowedByRules(conditional, platform, arch))
            {
                continue;
            }
            const QJsonValue value = conditional.value(QStringLiteral("value"));
            if (value.isString())
            {
                result.append(substitute(value.toString(), substitutions));
            }
            else if (value.isArray())
            {
                for (const QJsonValue& entry : value.toArray())
                {
                    if (!entry.isString())
                    {
                        throwLaunchError(
                            QStringLiteral("Minecraft argument value must be a string"));
                    }
                    result.append(substitute(entry.toString(), substitutions));
                }
            }
            else
            {
                throwLaunchError(QStringLiteral("Minecraft argument entry has no valid value"));
            }
        }
        return result;
    }

    QStringList GameLauncher::composeJvmArguments(const QStringList& metadataArguments,
                                                  const QStringList& customArguments)
    {
        const bool customCollector =
            std::any_of(customArguments.cbegin(), customArguments.cend(),
                        [](const QString& flag)
                        {
                            return QRegularExpression(QStringLiteral("^-XX:\\+Use[A-Za-z0-9]+GC$"))
                                .match(flag)
                                .hasMatch();
                        });
        QStringList result;
        static const QRegularExpression collector(QStringLiteral("^-XX:[+-]Use[A-Za-z0-9]+GC$"));
        for (const QString& argument : metadataArguments)
        {
            if (!customCollector || !collector.match(argument).hasMatch())
            {
                result.append(argument);
            }
        }
        result.append(customArguments);
        return result;
    }

    LaunchPlan GameLauncher::buildPlan(const strata::Instance& instance,
                                       const auth::OfflineAccount& account,
                                       const QString& dataDirectory, const QString& javaExecutable,
                                       const QStringList& globalJavaArguments)
    {
        auth::OfflineAccountStore::validateUsername(account.name);
        if (account.uuid != auth::OfflineAccountStore::offlineUuid(account.name))
        {
            throwLaunchError(QStringLiteral("Offline account UUID does not match the player name"));
        }

        const QString versionId = instance.versionId;
        static const QRegularExpression versionPattern(
            QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+() -]{0,127}$"));
        if (!versionPattern.match(versionId).hasMatch() || versionId.contains(QStringLiteral("..")))
        {
            throwLaunchError(QStringLiteral("Invalid Minecraft version identifier"));
        }
        const QString gameDirectory =
            QDir::cleanPath(QFileInfo(instance.gameDirectory()).absoluteFilePath());
        const QString versionDirectory =
            safePath(QDir(gameDirectory).filePath(QStringLiteral("versions")), versionId);
        const QString metadataPath =
            QDir(versionDirectory).filePath(versionId + QStringLiteral(".json"));
        const QString clientJar =
            QDir(versionDirectory).filePath(versionId + QStringLiteral(".jar"));
        if (!QFileInfo(clientJar).isFile())
        {
            throwLaunchError(QStringLiteral("Minecraft client is not installed for this instance"));
        }
        const QString marker =
            QDir(gameDirectory).filePath(QStringLiteral(".stratalauncher-installed"));
        QFile installedMarker(marker);
        if (!installedMarker.open(QIODevice::ReadOnly) ||
            installedMarker.readAll() != versionId.toUtf8())
        {
            throwLaunchError(QStringLiteral(
                "Minecraft installation is incomplete. Install or repair the instance first."));
        }

        const QJsonObject metadata = readJsonObject(metadataPath);
        const QString mainClass = requiredString(metadata, QStringLiteral("mainClass"));
        const QJsonObject assetIndex = metadata.value(QStringLiteral("assetIndex")).toObject();
        const QString assetIndexId = requiredString(assetIndex, QStringLiteral("id"));
        const QString assetsRoot =
            QDir(QFileInfo(dataDirectory).absoluteFilePath()).filePath(QStringLiteral("assets"));
        const QString assetIndexPath =
            safePath(QDir(assetsRoot).filePath(QStringLiteral("indexes")),
                     assetIndexId + QStringLiteral(".json"));
        if (!QFileInfo(assetIndexPath).isFile())
        {
            throwLaunchError(QStringLiteral(
                "Minecraft asset index is missing. Install or repair this instance."));
        }

        const QString librariesRoot =
            QDir(QFileInfo(dataDirectory).absoluteFilePath()).filePath(QStringLiteral("libraries"));
        const QString nativesDirectory =
            QDir(versionDirectory).filePath(versionId + QStringLiteral("-natives"));
        const QJsonObject features{{QStringLiteral("has_custom_resolution"), false},
                                   {QStringLiteral("has_quick_plays_support"), false},
                                   {QStringLiteral("is_demo_user"), false}};
        QStringList classpath{clientJar};
        QStringList nativeArchives;
        const QJsonArray libraries = metadata.value(QStringLiteral("libraries")).toArray();
        for (const QJsonValue& value : libraries)
        {
            if (!value.isObject())
            {
                throwLaunchError(
                    QStringLiteral("Minecraft metadata contains an invalid library entry"));
            }
            const QJsonObject library = value.toObject();
            if (!isAllowedByRules(library, osName(), architecture(), features))
            {
                continue;
            }
            const QString name = requiredString(library, QStringLiteral("name"));
            const QJsonObject libraryDownloads =
                library.value(QStringLiteral("downloads")).toObject();
            const QJsonObject artifact =
                libraryDownloads.value(QStringLiteral("artifact")).toObject();
            const QString relative = artifact.isEmpty()
                                         ? mavenPath(name)
                                         : requiredString(artifact, QStringLiteral("path"));
            const QString path = safePath(librariesRoot, relative);
            if (!QFileInfo(path).isFile())
            {
                throwLaunchError(
                    QStringLiteral(
                        "Minecraft library is missing: %1. Install or repair this instance.")
                        .arg(relative));
            }
            classpath.append(path);

            const QString nativeKey = osName();
            const QString nativeName = nativeClassifier(
                library.value(QStringLiteral("natives")).toObject().value(nativeKey).toString());
            const QJsonObject nativeArtifact = libraryDownloads.value(QStringLiteral("classifiers"))
                                                   .toObject()
                                                   .value(nativeName)
                                                   .toObject();
            if (!nativeArtifact.isEmpty())
            {
                const QString nativePath =
                    safePath(librariesRoot, requiredString(nativeArtifact, QStringLiteral("path")));
                if (!QFileInfo(nativePath).isFile())
                {
                    throwLaunchError(
                        QStringLiteral("Minecraft native library is missing: %1").arg(nativePath));
                }
                nativeArchives.append(nativePath);
            }
        }

        extractNativeArchives(nativeArchives, nativesDirectory);
        QMap<QString, QString> substitutions{
            {QStringLiteral("auth_player_name"), account.name},
            {QStringLiteral("version_name"), versionId},
            {QStringLiteral("game_directory"), gameDirectory},
            {QStringLiteral("assets_root"), assetsRoot},
            {QStringLiteral("assets_index_name"), assetIndexId},
            {QStringLiteral("auth_uuid"), account.uuid},
            {QStringLiteral("auth_access_token"), QStringLiteral("0")},
            {QStringLiteral("clientid"), QString()},
            {QStringLiteral("auth_xuid"), QString()},
            {QStringLiteral("user_type"), QStringLiteral("legacy")},
            {QStringLiteral("version_type"), QStringLiteral("StrataLauncher")},
            {QStringLiteral("natives_directory"), nativesDirectory},
            {QStringLiteral("library_directory"), librariesRoot},
            {QStringLiteral("classpath"), classpath.join(QDir::listSeparator())},
            {QStringLiteral("launcher_name"), QStringLiteral("StrataLauncher")},
            {QStringLiteral("launcher_version"), QStringLiteral("0.1.0")},
            {QStringLiteral("user_properties"), QStringLiteral("{}")},
            {QStringLiteral("resolution_width"), QStringLiteral("854")},
            {QStringLiteral("resolution_height"), QStringLiteral("480")}};

        QStringList jvmArguments;
        const QJsonObject arguments = metadata.value(QStringLiteral("arguments")).toObject();
        if (arguments.value(QStringLiteral("jvm")).isArray())
        {
            jvmArguments = resolveArguments(arguments.value(QStringLiteral("jvm")).toArray(),
                                            substitutions, osName(), architecture());
        }
        if (jvmArguments.isEmpty())
        {
            jvmArguments = {QStringLiteral("-Xms1G"), QStringLiteral("-Xmx4G")};
        }
        QStringList flags = globalJavaArguments;
        if (flags.isEmpty())
        {
            flags = {QStringLiteral("-Xms1G"), QStringLiteral("-Xmx4G"),
                     QStringLiteral("-XX:+UseG1GC")};
        }
        const InstanceSettings settings(instance.directory);
        flags = settings.effectiveJavaLaunchFlags(flags);
        jvmArguments = composeJvmArguments(jvmArguments, flags);
        if (!jvmArguments.contains(QStringLiteral("-Djava.library.path=%1").arg(nativesDirectory)))
        {
            jvmArguments.append(QStringLiteral("-Djava.library.path=%1").arg(nativesDirectory));
        }
        if (!jvmArguments.contains(QStringLiteral("-cp")) &&
            !jvmArguments.contains(QStringLiteral("-classpath")) &&
            !jvmArguments.contains(QStringLiteral("--class-path")))
        {
            jvmArguments.append(QStringLiteral("-cp"));
            jvmArguments.append(classpath.join(QDir::listSeparator()));
        }

        QStringList gameArguments;
        if (arguments.value(QStringLiteral("game")).isArray())
        {
            gameArguments = resolveArguments(arguments.value(QStringLiteral("game")).toArray(),
                                             substitutions, osName(), architecture());
        }
        else
        {
            const QString legacyArguments =
                metadata.value(QStringLiteral("minecraftArguments")).toString();
            const QStringList tokens = legacyArguments.split(
                QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            for (const QString& token : tokens)
            {
                gameArguments.append(substitute(token, substitutions));
            }
        }
        const QString program = javaExecutable.isEmpty() ? QStandardPaths::findExecutable(
#if defined(Q_OS_WIN)
                                                               QStringLiteral("java.exe")
#else
                                                               QStringLiteral("java")
#endif
                                                                   )
                                                         : javaExecutable;
        if (program.isEmpty())
        {
            throwLaunchError(
                QStringLiteral("Java was not found. Configure a Java runtime before launching."));
        }
        return {program, jvmArguments + QStringList{mainClass} + gameArguments, gameDirectory,
                QDir(gameDirectory).filePath(QStringLiteral("launcher.log"))};
    }

    bool GameLauncher::isGameReadyMarker(const QByteArray& line)
    {
        return line.contains("Sound engine started") || line.contains("Reload complete") ||
               line.contains("Minecraft is ready");
    }

    void GameLauncher::extractNativeArchives(const QStringList& archives,
                                             const QString& destination)
    {
        if (archives.isEmpty())
        {
            QDir().mkpath(destination);
            return;
        }
        const QFileInfo targetInfo(destination);
        if (targetInfo.isSymLink())
        {
            throwLaunchError(QStringLiteral("Native extraction destination is a symbolic link"));
        }
        if (QFileInfo::exists(destination) && !QDir(destination).removeRecursively())
        {
            throwLaunchError(QStringLiteral("Unable to clear old native libraries"));
        }
        if (!QDir().mkpath(destination))
        {
            throwLaunchError(QStringLiteral("Unable to create native library directory"));
        }
        int totalEntries = 0;
        qint64 totalBytes = 0;
        for (const QString& path : archives)
        {
            std::unique_ptr<struct archive, decltype(&archive_read_free)> reader(
                archive_read_new(), &archive_read_free);
            if (!reader)
            {
                throwLaunchError(QStringLiteral("Unable to allocate native archive reader"));
            }
            archive_read_support_format_zip(reader.get());
            archive_read_support_filter_all(reader.get());
            const QByteArray encodedPath = QFile::encodeName(path);
            if (archive_read_open_filename(reader.get(), encodedPath.constData(), 64 * 1024) !=
                ARCHIVE_OK)
            {
                const QString message = QString::fromUtf8(archiveError(reader.get()));
                throwLaunchError(
                    QStringLiteral("Unable to open native library archive: %1").arg(message));
            }
            struct archive_entry* entry = nullptr;
            int status = ARCHIVE_OK;
            while ((status = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK)
            {
                if (++totalEntries > maxNativeEntries)
                {
                    throwLaunchError(QStringLiteral("Native archive exceeds the file-count limit"));
                }
                const QString entryName = QString::fromUtf8(archive_entry_pathname(entry));
                if (!safeArchiveEntry(entryName))
                {
                    throwLaunchError(QStringLiteral("Unsafe path in native library archive"));
                }
                if (archive_entry_symlink(entry) != nullptr ||
                    archive_entry_hardlink(entry) != nullptr)
                {
                    throwLaunchError(QStringLiteral("Native library archive contains a link"));
                }
                if (entryName.startsWith(QStringLiteral("META-INF/")))
                {
                    archive_read_data_skip(reader.get());
                    continue;
                }
                const QString outputPath = safePath(destination, entryName);
                const auto fileType = archive_entry_filetype(entry);
                if (fileType == AE_IFDIR)
                {
                    if (!QDir().mkpath(outputPath))
                    {
                        throwLaunchError(QStringLiteral("Unable to create native directory"));
                    }
                    continue;
                }
                if (fileType != AE_IFREG)
                {
                    archive_read_data_skip(reader.get());
                    continue;
                }
                if (!QDir().mkpath(QFileInfo(outputPath).absolutePath()))
                {
                    throwLaunchError(
                        QStringLiteral("Unable to create native file parent directory"));
                }
                QFile output(outputPath);
                if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
                {
                    throwLaunchError(QStringLiteral("Unable to write native library: %1")
                                         .arg(output.errorString()));
                }
                char buffer[64 * 1024];
                la_ssize_t bytesRead = 0;
                while ((bytesRead = archive_read_data(reader.get(), buffer, sizeof(buffer))) > 0)
                {
                    totalBytes += bytesRead;
                    if (totalBytes > maxNativeBytes || output.write(buffer, bytesRead) != bytesRead)
                    {
                        throwLaunchError(
                            QStringLiteral("Native archive exceeds size limit or failed to write"));
                    }
                }
                if (bytesRead < 0)
                {
                    const QString message = QString::fromUtf8(archiveError(reader.get()));
                    throwLaunchError(
                        QStringLiteral("Unable to extract native archive: %1").arg(message));
                }
            }
            if (status != ARCHIVE_EOF)
            {
                const QString message = QString::fromUtf8(archiveError(reader.get()));
                throwLaunchError(QStringLiteral("Unable to read native archive: %1").arg(message));
            }
        }
    }
} // namespace strata::launch
