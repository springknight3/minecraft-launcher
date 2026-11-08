#include "LoaderInstaller.h"

#include "../launch/GameLauncher.h"
#include "../mojang/MojangManifestClient.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSysInfo>
#include <QTimer>
#include <QVersionNumber>
#include <QXmlStreamReader>

#include <algorithm>
#include <stdexcept>

namespace strata::loader
{
    namespace
    {
        constexpr qsizetype kMetadataLimit = 8 * 1024 * 1024;
        const QRegularExpression
            kVersionPattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+-]{0,127}$"));

        [[noreturn]] void fail(const QString& message)
        {
            throw std::runtime_error(message.toStdString());
        }

        bool isVersion(const QString& version)
        {
            return kVersionPattern.match(version).hasMatch() &&
                   !version.contains(QStringLiteral(".."));
        }

        QByteArray fetchBytes(const QUrl& url, qsizetype maxBytes)
        {
            if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.host().isEmpty())
            {
                fail(QStringLiteral("Loader metadata URL must use HTTPS."));
            }

            QNetworkAccessManager manager;
            QNetworkRequest request(url);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);
            request.setTransferTimeout(30000);
            request.setRawHeader("User-Agent", "StrataLauncher/0.1.0");
            QNetworkReply* reply = manager.get(request);
            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            QByteArray body;
            bool tooLarge = false;
            bool timedOut = false;

            QObject::connect(reply, &QIODevice::readyRead, &loop,
                             [&]()
                             {
                                 const QByteArray chunk = reply->readAll();
                                 if (body.size() + chunk.size() > maxBytes)
                                 {
                                     tooLarge = true;
                                     reply->abort();
                                     return;
                                 }
                                 body.append(chunk);
                             });
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            QObject::connect(&timeout, &QTimer::timeout, &loop,
                             [&]()
                             {
                                 timedOut = true;
                                 reply->abort();
                             });
            timeout.start(30000);
            loop.exec();
            timeout.stop();
            const QByteArray remaining = reply->readAll();
            if (!tooLarge && body.size() + remaining.size() <= maxBytes)
            {
                body.append(remaining);
            }
            else if (!remaining.isEmpty())
            {
                tooLarge = true;
            }

            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto networkError = reply->error();
            const QString error = reply->errorString();
            reply->deleteLater();
            if (timedOut)
            {
                fail(QStringLiteral("Timed out fetching loader metadata from %1.").arg(url.host()));
            }
            if (tooLarge)
            {
                fail(QStringLiteral("Loader metadata from %1 exceeds the size limit.")
                         .arg(url.host()));
            }
            if (networkError != QNetworkReply::NoError || status < 200 || status >= 300)
            {
                fail(QStringLiteral("Could not fetch loader metadata from %1 (HTTP %2): %3")
                         .arg(url.host())
                         .arg(status)
                         .arg(error));
            }
            return body;
        }

        QJsonObject fetchJson(const QUrl& url)
        {
            QJsonParseError error{};
            const QByteArray bytes = fetchBytes(url, kMetadataLimit);
            const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
            if (error.error != QJsonParseError::NoError || !document.isObject())
            {
                fail(QStringLiteral("Loader metadata from %1 is not a valid JSON object.")
                         .arg(url.host()));
            }
            return document.object();
        }

        QString requiredString(const QJsonObject& object, const QString& key)
        {
            const QJsonValue value = object.value(key);
            if (!value.isString() || value.toString().trimmed().isEmpty())
            {
                fail(QStringLiteral("Loader profile is missing %1.").arg(key));
            }
            return value.toString();
        }

        QString profileFile(const QString& gameDirectory, const QString& profileId)
        {
            const QString versionsRoot = QDir(gameDirectory).filePath(QStringLiteral("versions"));
            const QString versionDirectory = QDir(versionsRoot).filePath(profileId);
            return QDir(versionDirectory).filePath(profileId + QStringLiteral(".json"));
        }

        QJsonObject readJsonObject(const QString& path)
        {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                fail(QStringLiteral("Could not read loader profile: %1").arg(path));
            }
            if (file.size() > kMetadataLimit)
            {
                fail(QStringLiteral("Loader profile exceeds the size limit: %1").arg(path));
            }
            QJsonParseError error{};
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject())
            {
                fail(QStringLiteral("Loader profile is invalid JSON: %1").arg(path));
            }
            return document.object();
        }

        void saveJsonAtomically(const QString& path, const QJsonObject& value,
                                const QString& errorMessage)
        {
            const QByteArray bytes = QJsonDocument(value).toJson(QJsonDocument::Indented);
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
                !file.commit())
            {
                fail(errorMessage);
            }
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

        QString mavenRepository(LoaderType type)
        {
            switch (type)
            {
            case LoaderType::Fabric:
                return QStringLiteral("https://maven.fabricmc.net/");
            case LoaderType::Quilt:
                return QStringLiteral("https://maven.quiltmc.org/repository/release/");
            default:
                fail(QStringLiteral("Unsupported profile-based loader."));
            }
        }

        QString mavenArtifactPath(const QString& coordinate)
        {
            const QStringList coordinateAndExtension = coordinate.split(QLatin1Char('@'));
            if (coordinateAndExtension.size() > 2)
            {
                fail(QStringLiteral("Loader library has an invalid Maven coordinate."));
            }
            const QStringList parts =
                coordinateAndExtension.front().split(QLatin1Char(':'), Qt::KeepEmptyParts);
            if (parts.size() < 3 || parts.size() > 4)
            {
                fail(QStringLiteral("Loader library has an invalid Maven coordinate."));
            }
            const QString group = parts.at(0);
            const QString artifact = parts.at(1);
            const QString version = parts.at(2);
            const QString classifier = parts.size() == 4 ? parts.at(3) : QString();
            if (group.isEmpty() || artifact.isEmpty() || !isVersion(version) ||
                group.contains(QStringLiteral("..")) || artifact.contains(QLatin1Char('/')) ||
                group.contains(QLatin1Char('/')) ||
                (!classifier.isEmpty() && !isVersion(classifier)))
            {
                fail(QStringLiteral("Loader library has an unsafe Maven coordinate."));
            }
            QString extension = coordinateAndExtension.size() == 2 ? coordinateAndExtension.at(1)
                                                                   : QStringLiteral("jar");
            if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9]{1,10}$"))
                     .match(extension)
                     .hasMatch())
            {
                fail(QStringLiteral("Loader library has an invalid Maven extension."));
            }
            const QString base =
                QStringLiteral("%1/%2/%3/%2-%3")
                    .arg(group.split(QLatin1Char('.')).join(QLatin1Char('/')), artifact, version);
            return base + (classifier.isEmpty() ? QString() : QLatin1Char('-') + classifier) +
                   QLatin1Char('.') + extension;
        }

        int compareMavenVersions(const QString& left, const QString& right)
        {
            static const QRegularExpression tokenPattern(QStringLiteral("[0-9]+|[A-Za-z]+"));
            const auto leftMatches = tokenPattern.globalMatch(left);
            const auto rightMatches = tokenPattern.globalMatch(right);
            QStringList leftTokens;
            QStringList rightTokens;
            for (auto match = leftMatches; match.hasNext();)
            {
                leftTokens.append(match.next().captured());
            }
            for (auto match = rightMatches; match.hasNext();)
            {
                rightTokens.append(match.next().captured());
            }
            const qsizetype commonCount = std::min(leftTokens.size(), rightTokens.size());
            for (qsizetype index = 0; index < commonCount; ++index)
            {
                const QString& leftToken = leftTokens.at(index);
                const QString& rightToken = rightTokens.at(index);
                const bool leftNumeric = leftToken.front().isDigit();
                const bool rightNumeric = rightToken.front().isDigit();
                if (leftNumeric && rightNumeric)
                {
                    const auto normalizeNumber = [](const QString& number)
                    {
                        qsizetype start = 0;
                        while (start + 1 < number.size() && number.at(start) == QLatin1Char('0'))
                        {
                            ++start;
                        }
                        return number.mid(start);
                    };
                    const QString normalizedLeft = normalizeNumber(leftToken);
                    const QString normalizedRight = normalizeNumber(rightToken);
                    const QString& numberLeft =
                        normalizedLeft.isEmpty() ? QStringLiteral("0") : normalizedLeft;
                    const QString& numberRight =
                        normalizedRight.isEmpty() ? QStringLiteral("0") : normalizedRight;
                    if (numberLeft.size() != numberRight.size())
                    {
                        return numberLeft.size() < numberRight.size() ? -1 : 1;
                    }
                    const int numericOrder = QString::compare(numberLeft, numberRight);
                    if (numericOrder != 0)
                    {
                        return numericOrder;
                    }
                }
                else if (leftNumeric != rightNumeric)
                {
                    return leftNumeric ? 1 : -1;
                }
                else
                {
                    const int textOrder =
                        QString::compare(leftToken, rightToken, Qt::CaseInsensitive);
                    if (textOrder != 0)
                    {
                        return textOrder;
                    }
                }
            }
            if (leftTokens.size() == rightTokens.size())
            {
                return 0;
            }
            const QStringList& longer =
                leftTokens.size() > rightTokens.size() ? leftTokens : rightTokens;
            const bool extraIsNumeric = longer.at(commonCount).front().isDigit();
            const int result = extraIsNumeric ? 1 : -1;
            return leftTokens.size() > rightTokens.size() ? result : -result;
        }

        QString expectedLoaderType(const QJsonObject& state)
        {
            return requiredString(state, QStringLiteral("type")).toUpper();
        }
    } // namespace

    LoaderSelection LoaderSelection::vanilla()
    {
        return {};
    }

    LoaderType LoaderSelection::parseType(const QString& name)
    {
        const QString normalized = name.trimmed().toUpper();
        if (normalized == QStringLiteral("VANILLA"))
        {
            return LoaderType::Vanilla;
        }
        if (normalized == QStringLiteral("FABRIC"))
        {
            return LoaderType::Fabric;
        }
        if (normalized == QStringLiteral("QUILT"))
        {
            return LoaderType::Quilt;
        }
        if (normalized == QStringLiteral("FORGE"))
        {
            return LoaderType::Forge;
        }
        if (normalized == QStringLiteral("NEOFORGE"))
        {
            return LoaderType::NeoForge;
        }
        fail(QStringLiteral("Unknown Minecraft loader: %1").arg(name));
    }

    QString LoaderSelection::typeName(LoaderType type)
    {
        switch (type)
        {
        case LoaderType::Vanilla:
            return QStringLiteral("VANILLA");
        case LoaderType::Fabric:
            return QStringLiteral("FABRIC");
        case LoaderType::Quilt:
            return QStringLiteral("QUILT");
        case LoaderType::Forge:
            return QStringLiteral("FORGE");
        case LoaderType::NeoForge:
            return QStringLiteral("NEOFORGE");
        }
        fail(QStringLiteral("Unknown Minecraft loader type."));
    }

    LoaderInstaller::LoaderInstaller(const download::DownloadManager& downloadManager,
                                     const runtime::JavaRuntimeManager& runtimeManager,
                                     QString dataDirectory)
        : m_downloadManager(downloadManager), m_runtimeManager(runtimeManager),
          m_dataDirectory(QDir::cleanPath(QFileInfo(dataDirectory).absoluteFilePath())),
          m_installerCache(QDir(m_dataDirectory).filePath(QStringLiteral("loader-installers"))),
          m_librariesDirectory(QDir(m_dataDirectory).filePath(QStringLiteral("libraries")))
    {
    }

    QStringList LoaderInstaller::availableVersions(LoaderType type,
                                                   const QString& minecraftVersion) const
    {
        if (!isVersion(minecraftVersion))
        {
            fail(QStringLiteral("Invalid Minecraft version for loader selection."));
        }
        if (type == LoaderType::Vanilla)
        {
            return {};
        }

        if (type == LoaderType::Fabric || type == LoaderType::Quilt)
        {
            const QString host = type == LoaderType::Fabric ? QStringLiteral("meta.fabricmc.net")
                                                            : QStringLiteral("meta.quiltmc.org");
            const QString apiVersion =
                type == LoaderType::Fabric ? QStringLiteral("v2") : QStringLiteral("v3");
            const QUrl url(QStringLiteral("https://%1/%2/versions/loader/%3")
                               .arg(host, apiVersion, minecraftVersion));
            const QByteArray bytes = fetchBytes(url, kMetadataLimit);
            QJsonParseError error{};
            const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
            if (error.error != QJsonParseError::NoError || !document.isArray())
            {
                fail(QStringLiteral("%1 version metadata is invalid.")
                         .arg(LoaderSelection::typeName(type)));
            }
            QStringList result;
            for (const QJsonValue& item : document.array())
            {
                if (!item.isObject())
                {
                    fail(QStringLiteral("%1 version metadata contains an invalid entry.")
                             .arg(LoaderSelection::typeName(type)));
                }
                const QJsonValue loader = item.toObject().value(QStringLiteral("loader"));
                const QJsonValue version = loader.isObject()
                                               ? loader.toObject().value(QStringLiteral("version"))
                                               : QJsonValue{};
                if (!version.isString() || !isVersion(version.toString()))
                {
                    fail(QStringLiteral("%1 version metadata contains an invalid version.")
                             .arg(LoaderSelection::typeName(type)));
                }
                if (!result.contains(version.toString()))
                {
                    result.append(version.toString());
                }
            }
            return result;
        }

        const QUrl metadataUrl(
            type == LoaderType::Forge
                ? QStringLiteral("https://maven.minecraftforge.net/net/minecraftforge/forge/"
                                 "maven-metadata.xml")
                : QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/"
                                 "maven-metadata.xml"));
        const QString prefix = type == LoaderType::Forge
                                   ? minecraftVersion + QLatin1Char('-')
                                   : neoForgeCompatibilityPrefix(minecraftVersion);
        if (prefix.isEmpty())
        {
            return {};
        }
        const QStringList fullVersions =
            parseMavenVersions(fetchBytes(metadataUrl, kMetadataLimit), prefix);
        if (type == LoaderType::Forge)
        {
            QStringList result;
            result.reserve(fullVersions.size());
            for (const QString& version : fullVersions)
            {
                result.append(version.mid(prefix.size()));
            }
            return result;
        }
        return fullVersions;
    }

    QStringList LoaderInstaller::parseMavenVersions(const QByteArray& xml, const QString& prefix)
    {
        if (xml.size() > kMetadataLimit)
        {
            fail(QStringLiteral("Maven metadata exceeds the size limit."));
        }
        QXmlStreamReader reader(xml);
        QStringList versions;
        while (!reader.atEnd())
        {
            const QXmlStreamReader::TokenType token = reader.readNext();
            if (token == QXmlStreamReader::DTD)
            {
                fail(QStringLiteral("Maven metadata must not contain a DTD."));
            }
            if (token == QXmlStreamReader::StartElement &&
                reader.name() == QStringLiteral("version"))
            {
                const QString version =
                    reader.readElementText(QXmlStreamReader::ErrorOnUnexpectedElement);
                if (version.startsWith(prefix) && isVersion(version) && !versions.contains(version))
                {
                    versions.append(version);
                }
            }
        }
        if (reader.hasError())
        {
            fail(QStringLiteral("Maven metadata XML is invalid."));
        }
        std::sort(versions.begin(), versions.end(), [](const QString& left, const QString& right)
                  { return compareMavenVersions(left, right) > 0; });
        return versions;
    }

    QString LoaderInstaller::neoForgeCompatibilityPrefix(const QString& minecraftVersion)
    {
        const QVersionNumber version = QVersionNumber::fromString(minecraftVersion);
        if (version.segmentCount() != 3 || version.segmentAt(0) != 1 || version.segmentAt(1) < 20)
        {
            return {};
        }
        return QStringLiteral("%1.%2.").arg(version.segmentAt(1)).arg(version.segmentAt(2));
    }

    LoaderProfile LoaderInstaller::parseProfile(LoaderType type, const QString& minecraftVersion,
                                                const QString& loaderVersion,
                                                const QJsonObject& profile)
    {
        if (type == LoaderType::Vanilla || !isVersion(minecraftVersion) ||
            !isVersion(loaderVersion))
        {
            fail(QStringLiteral("Loader profile selection is invalid."));
        }
        const QString inherited = requiredString(profile, QStringLiteral("inheritsFrom"));
        if (inherited != minecraftVersion)
        {
            fail(QStringLiteral("Loader profile targets Minecraft %1, not %2.")
                     .arg(inherited, minecraftVersion));
        }
        requiredString(profile, QStringLiteral("id"));
        requiredString(profile, QStringLiteral("mainClass"));
        const QJsonValue libraries = profile.value(QStringLiteral("libraries"));
        if (!libraries.isUndefined() && !libraries.isArray())
        {
            fail(QStringLiteral("Loader profile libraries must be an array."));
        }
        const QJsonValue arguments = profile.value(QStringLiteral("arguments"));
        if (!arguments.isUndefined())
        {
            if (!arguments.isObject())
            {
                fail(QStringLiteral("Loader profile arguments must be an object."));
            }
            const QJsonObject argumentObject = arguments.toObject();
            for (const QString& key : {QStringLiteral("game"), QStringLiteral("jvm")})
            {
                const QJsonValue value = argumentObject.value(key);
                if (!value.isUndefined() && !value.isArray())
                {
                    fail(QStringLiteral("Loader profile %1 arguments must be an array.").arg(key));
                }
            }
        }
        return {type, minecraftVersion, loaderVersion,
                profile.value(QStringLiteral("id")).toString(), profile};
    }

    QJsonObject LoaderInstaller::mergeMetadata(const QJsonObject& minecraftMetadata,
                                               const LoaderProfile& profile)
    {
        if (profile.type == LoaderType::Vanilla ||
            profile.launchMetadata.value(QStringLiteral("inheritsFrom")).toString() !=
                profile.minecraftVersion)
        {
            fail(QStringLiteral("Loader profile cannot be merged with this Minecraft version."));
        }
        QJsonObject merged = minecraftMetadata;
        const QJsonObject loader = profile.launchMetadata;
        for (const QString& key : {QStringLiteral("mainClass"), QStringLiteral("jar"),
                                   QStringLiteral("minecraftArguments")})
        {
            if (loader.contains(key))
            {
                merged.insert(key, loader.value(key));
            }
        }

        QJsonArray libraries = merged.value(QStringLiteral("libraries")).toArray();
        QJsonArray loaderLibraries = loader.value(QStringLiteral("libraries")).toArray();
        QSet<QString> names;
        for (const QJsonValue& value : libraries)
        {
            if (value.isObject())
            {
                const QString name = value.toObject().value(QStringLiteral("name")).toString();
                if (!name.isEmpty())
                {
                    names.insert(name);
                }
            }
        }
        for (const QJsonValue& value : loaderLibraries)
        {
            if (!value.isObject())
            {
                fail(QStringLiteral("Loader profile contains an invalid library entry."));
            }
            const QString name = value.toObject().value(QStringLiteral("name")).toString();
            if (name.isEmpty() || !names.contains(name))
            {
                libraries.append(value);
                if (!name.isEmpty())
                {
                    names.insert(name);
                }
            }
        }
        merged.insert(QStringLiteral("libraries"), libraries);

        QJsonObject arguments = merged.value(QStringLiteral("arguments")).toObject();
        const QJsonObject loaderArguments = loader.value(QStringLiteral("arguments")).toObject();
        for (const QString& key : {QStringLiteral("game"), QStringLiteral("jvm")})
        {
            const QJsonValue loaderValues = loaderArguments.value(key);
            if (loaderValues.isUndefined())
            {
                continue;
            }
            QJsonArray values = arguments.value(key).toArray();
            for (const QJsonValue& value : loaderValues.toArray())
            {
                values.append(value);
            }
            arguments.insert(key, values);
        }
        merged.insert(QStringLiteral("arguments"), arguments);
        return merged;
    }

    QString LoaderInstaller::resolveLibraryPath(const QString& root, const QString& relativePath)
    {
        QString normalizedRelative = QDir::fromNativeSeparators(relativePath);
        normalizedRelative.replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (normalizedRelative.isEmpty() || QDir::isAbsolutePath(normalizedRelative) ||
            normalizedRelative.startsWith(QLatin1Char('/')) ||
            normalizedRelative.contains(QChar::Null))
        {
            fail(QStringLiteral("Loader library path is invalid."));
        }
        const QStringList segments = normalizedRelative.split(QLatin1Char('/'));
        for (const QString& segment : segments)
        {
            if (segment == QStringLiteral("..") || segment.contains(QLatin1Char(':')))
            {
                fail(QStringLiteral("Loader library path escapes the library directory."));
            }
        }
        const QString rootPath = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
        const QString candidate = QDir::cleanPath(QDir(rootPath).filePath(normalizedRelative));
        const QString prefix =
            rootPath.endsWith(QDir::separator()) ? rootPath : rootPath + QDir::separator();
        if (!candidate.startsWith(prefix))
        {
            fail(QStringLiteral("Loader library path escapes the library directory."));
        }
        return candidate;
    }

    LoaderSelection LoaderInstaller::installedSelection(const Instance& instance) const
    {
        const QString statePath =
            QDir(instance.gameDirectory()).filePath(QStringLiteral(".stratalauncher-loader.json"));
        if (!QFileInfo::exists(statePath))
        {
            return LoaderSelection::vanilla();
        }
        const QJsonObject state = readJsonObject(statePath);
        const LoaderType type = LoaderSelection::parseType(expectedLoaderType(state));
        if (type == LoaderType::Vanilla)
        {
            return LoaderSelection::vanilla();
        }
        if (state.value(QStringLiteral("installed")).toBool() != true)
        {
            fail(QStringLiteral("Loader installation is incomplete. Reinstall the loader."));
        }
        const QString minecraftVersion = requiredString(state, QStringLiteral("minecraftVersion"));
        if (minecraftVersion != instance.versionId)
        {
            fail(QStringLiteral("Installed loader state does not match the Minecraft instance."));
        }
        return {type, requiredString(state, QStringLiteral("loaderVersion"))};
    }

    std::optional<LoaderProfile> LoaderInstaller::installedProfile(const Instance& instance)
    {
        const QString statePath =
            QDir(instance.gameDirectory()).filePath(QStringLiteral(".stratalauncher-loader.json"));
        if (!QFileInfo::exists(statePath))
        {
            return std::nullopt;
        }
        const QJsonObject state = readJsonObject(statePath);
        const LoaderType type =
            LoaderSelection::parseType(requiredString(state, QStringLiteral("type")).toUpper());
        if (type == LoaderType::Vanilla)
        {
            return std::nullopt;
        }
        if (state.value(QStringLiteral("installed")).toBool() != true)
        {
            fail(QStringLiteral("Loader installation is incomplete. Reinstall the loader."));
        }
        const QString minecraftVersion = requiredString(state, QStringLiteral("minecraftVersion"));
        const QString loaderVersion = requiredString(state, QStringLiteral("loaderVersion"));
        if (minecraftVersion != instance.versionId)
        {
            fail(QStringLiteral("Installed loader state does not match the Minecraft instance."));
        }
        const QString profileId = requiredString(state, QStringLiteral("profileId"));
        const QJsonObject profile = readJsonObject(
            QDir(instance.gameDirectory()).filePath(QStringLiteral("loader-profile.json")));
        LoaderProfile parsed = parseProfile(type, minecraftVersion, loaderVersion, profile);
        if (parsed.profileVersionId != profileId)
        {
            fail(QStringLiteral("Installed loader profile ID does not match its saved state."));
        }
        return parsed;
    }

    void LoaderInstaller::prepare(const Instance& instance, const LoaderSelection& selection) const
    {
        const QString statePath =
            QDir(instance.gameDirectory()).filePath(QStringLiteral(".stratalauncher-loader.json"));
        const QString profilePath =
            QDir(instance.gameDirectory()).filePath(QStringLiteral("loader-profile.json"));
        if (selection.type == LoaderType::Vanilla)
        {
            if ((QFileInfo::exists(statePath) && !QFile::remove(statePath)) ||
                (QFileInfo::exists(profilePath) && !QFile::remove(profilePath)))
            {
                fail(QStringLiteral("Could not clear previous loader installation state."));
            }
            return;
        }
        if (!isVersion(instance.versionId) || !isVersion(selection.version))
        {
            fail(QStringLiteral("Loader selection contains an invalid version."));
        }
        const QJsonObject state{{QStringLiteral("type"), LoaderSelection::typeName(selection.type)},
                                {QStringLiteral("minecraftVersion"), instance.versionId},
                                {QStringLiteral("loaderVersion"), selection.version},
                                {QStringLiteral("profileId"), QString()},
                                {QStringLiteral("installed"), false}};
        saveJsonAtomically(statePath, state,
                           QStringLiteral("Could not save pending loader installation state."));
        if (QFileInfo::exists(profilePath) && !QFile::remove(profilePath))
        {
            fail(QStringLiteral("Could not clear the previous loader profile."));
        }
    }

    void LoaderInstaller::install(const Instance& instance, const LoaderSelection& selection,
                                  ProgressCallback progress) const
    {
        prepare(instance, selection);
        if (selection.type == LoaderType::Vanilla)
        {
            return;
        }
        const QStringList versions = availableVersions(selection.type, instance.versionId);
        if (!versions.contains(selection.version))
        {
            fail(QStringLiteral("Loader version %1 is not compatible with Minecraft %2.")
                     .arg(selection.version, instance.versionId));
        }

        LoaderProfile profile;
        if (selection.type == LoaderType::Fabric || selection.type == LoaderType::Quilt)
        {
            profile = installProfileBased(selection.type, instance.versionId, selection.version,
                                          progress);
        }
        else
        {
            profile = installOfficial(selection.type, instance, selection.version, progress);
        }

        const QString savedProfilePath =
            QDir(instance.gameDirectory()).filePath(QStringLiteral("loader-profile.json"));
        saveJsonAtomically(savedProfilePath, profile.launchMetadata,
                           QStringLiteral("Could not save installed loader profile."));
        const QString statePath =
            QDir(instance.gameDirectory()).filePath(QStringLiteral(".stratalauncher-loader.json"));
        const QJsonObject state{{QStringLiteral("type"), LoaderSelection::typeName(profile.type)},
                                {QStringLiteral("minecraftVersion"), profile.minecraftVersion},
                                {QStringLiteral("loaderVersion"), profile.loaderVersion},
                                {QStringLiteral("profileId"), profile.profileVersionId},
                                {QStringLiteral("installed"), true}};
        saveJsonAtomically(statePath, state,
                           QStringLiteral("Could not save installed loader state."));
    }

    LoaderProfile LoaderInstaller::installProfileBased(LoaderType type,
                                                       const QString& minecraftVersion,
                                                       const QString& loaderVersion,
                                                       const ProgressCallback& progress) const
    {
        const QString host = type == LoaderType::Fabric ? QStringLiteral("meta.fabricmc.net")
                                                        : QStringLiteral("meta.quiltmc.org");
        const QString apiVersion =
            type == LoaderType::Fabric ? QStringLiteral("v2") : QStringLiteral("v3");
        const QUrl url(QStringLiteral("https://%1/%2/versions/loader/%3/%4/profile/json")
                           .arg(host, apiVersion, minecraftVersion, loaderVersion));
        const QJsonObject json = fetchJson(url);
        LoaderProfile profile = parseProfile(type, minecraftVersion, loaderVersion, json);
        downloadLibraries(type, json, progress);
        return profile;
    }

    LoaderProfile LoaderInstaller::installOfficial(LoaderType type, const Instance& instance,
                                                   const QString& loaderVersion,
                                                   const ProgressCallback& progress) const
    {
        const QString minecraftVersion = instance.versionId;
        QString artifact;
        QUrl repository;
        QString installerName;
        if (type == LoaderType::Forge)
        {
            const QString combined = minecraftVersion + QLatin1Char('-') + loaderVersion;
            artifact =
                QStringLiteral("net/minecraftforge/forge/%1/forge-%1-installer.jar").arg(combined);
            repository = QUrl(QStringLiteral("https://maven.minecraftforge.net/"));
            installerName = QStringLiteral("Forge");
        }
        else if (type == LoaderType::NeoForge)
        {
            artifact = QStringLiteral("net/neoforged/neoforge/%1/neoforge-%1-installer.jar")
                           .arg(loaderVersion);
            repository = QUrl(QStringLiteral("https://maven.neoforged.net/releases/"));
            installerName = QStringLiteral("NeoForge");
        }
        else
        {
            fail(QStringLiteral("Unsupported official loader."));
        }
        const QUrl installerUrl = repository.resolved(QUrl(artifact));
        const QByteArray sha1Bytes =
            fetchBytes(QUrl(installerUrl.toString() + QStringLiteral(".sha1")), 256);
        const QString sha1 =
            QString::fromLatin1(sha1Bytes).trimmed().section(QLatin1Char(' '), 0, 0);
        if (!QRegularExpression(QStringLiteral("(?i)^[0-9a-f]{40}$")).match(sha1).hasMatch())
        {
            fail(QStringLiteral("%1 installer SHA-1 metadata is invalid.").arg(installerName));
        }

        const QString relativeInstallerPath =
            QStringLiteral("%1/%2/%3-installer.jar")
                .arg(LoaderSelection::typeName(type).toLower(), minecraftVersion, loaderVersion);
        const QString installerPath = resolveLibraryPath(m_installerCache, relativeInstallerPath);
        const download::DownloadTask task{installerUrl, installerPath, sha1,
                                          installerName + QStringLiteral(" installer")};
        m_downloadManager.downloadAll({task}, progress);

        const QString vanillaMetadataPath = profileFile(instance.gameDirectory(), minecraftVersion);
        const QJsonObject minecraftMetadata = readJsonObject(vanillaMetadataPath);
        const QString java =
            m_runtimeManager.javaExecutableForInstaller(instance, minecraftMetadata, progress);
        QProcess process;
        process.setProgram(java);
        process.setArguments({QStringLiteral("-jar"), installerPath,
                              type == LoaderType::Forge ? QStringLiteral("--installClient")
                                                        : QStringLiteral("--install-client")});
        process.setWorkingDirectory(instance.gameDirectory());
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start();
        if (!process.waitForStarted(10000))
        {
            fail(QStringLiteral("Could not start the %1 installer: %2")
                     .arg(installerName, process.errorString()));
        }
        if (!process.waitForFinished(10 * 60 * 1000))
        {
            process.kill();
            process.waitForFinished();
            fail(QStringLiteral("%1 installer timed out.").arg(installerName));
        }
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        {
            QString output = QString::fromLocal8Bit(process.readAll()).trimmed();
            if (output.size() > 2000)
            {
                output = output.right(2000);
            }
            fail(QStringLiteral("%1 installer failed with exit code %2: %3")
                     .arg(installerName)
                     .arg(process.exitCode())
                     .arg(output));
        }

        const QDir versionsDirectory(
            QDir(instance.gameDirectory()).filePath(QStringLiteral("versions")));
        const QStringList directories =
            versionsDirectory.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
        for (const QString& profileId : directories)
        {
            const QString expectedMarker =
                type == LoaderType::Forge ? QStringLiteral("forge") : QStringLiteral("neoforge");
            if (!profileId.contains(expectedMarker, Qt::CaseInsensitive) ||
                !profileId.contains(loaderVersion, Qt::CaseInsensitive))
            {
                continue;
            }
            const QString path = profileFile(instance.gameDirectory(), profileId);
            if (!QFileInfo(path).isFile())
            {
                continue;
            }
            const QJsonObject json = readJsonObject(path);
            if (json.value(QStringLiteral("inheritsFrom")).toString() != minecraftVersion)
            {
                continue;
            }
            return parseProfile(type, minecraftVersion, loaderVersion, json);
        }
        fail(
            QStringLiteral("%1 installer completed but did not create a compatible launch profile.")
                .arg(installerName));
    }

    void LoaderInstaller::downloadLibraries(LoaderType type, const QJsonObject& profile,
                                            const ProgressCallback& progress) const
    {
        const QJsonArray libraries = profile.value(QStringLiteral("libraries")).toArray();
        QList<download::DownloadTask> tasks;
        for (const QJsonValue& value : libraries)
        {
            if (!value.isObject())
            {
                fail(QStringLiteral("Loader profile contains an invalid library entry."));
            }
            const QJsonObject library = value.toObject();
            if (!strata::launch::GameLauncher::isAllowedByRules(library, osName(),
                                                                QSysInfo::currentCpuArchitecture()))
            {
                continue;
            }
            const QString name = requiredString(library, QStringLiteral("name"));
            const QJsonObject downloads = library.value(QStringLiteral("downloads")).toObject();
            const QJsonObject artifact = downloads.value(QStringLiteral("artifact")).toObject();
            QString relativePath;
            if (!artifact.isEmpty())
            {
                relativePath = requiredString(artifact, QStringLiteral("path"));
            }
            else
            {
                relativePath = mavenArtifactPath(name);
            }
            const QString destination = resolveLibraryPath(m_librariesDirectory, relativePath);
            QString sha1 = artifact.value(QStringLiteral("sha1")).toString();
            if (!sha1.isEmpty() &&
                !QRegularExpression(QStringLiteral("(?i)^[0-9a-f]{40}$")).match(sha1).hasMatch())
            {
                fail(QStringLiteral("Loader library %1 has an invalid SHA-1.").arg(name));
            }
            QUrl source;
            const QJsonValue artifactUrl = artifact.value(QStringLiteral("url"));
            if (artifactUrl.isString() && !artifactUrl.toString().isEmpty())
            {
                source = QUrl(artifactUrl.toString());
            }
            else
            {
                QString repository =
                    library.value(QStringLiteral("url")).toString(mavenRepository(type));
                if (!repository.endsWith(QLatin1Char('/')))
                {
                    repository.append(QLatin1Char('/'));
                }
                source = QUrl(repository + relativePath);
            }
            if (!source.isValid() || source.scheme() != QStringLiteral("https") ||
                source.host().isEmpty())
            {
                fail(QStringLiteral("Loader library %1 does not have a safe HTTPS URL.").arg(name));
            }
            if (sha1.isEmpty())
            {
                QUrl checksumUrl = source;
                checksumUrl.setQuery(QString());
                checksumUrl.setFragment(QString());
                checksumUrl.setPath(checksumUrl.path() + QStringLiteral(".sha1"));
                sha1 = QString::fromLatin1(fetchBytes(checksumUrl, 256))
                           .trimmed()
                           .section(QLatin1Char(' '), 0, 0);
                if (!QRegularExpression(QStringLiteral("(?i)^[0-9a-f]{40}$"))
                         .match(sha1)
                         .hasMatch())
                {
                    fail(QStringLiteral("Loader library %1 SHA-1 metadata is invalid.").arg(name));
                }
            }
            tasks.append({source, destination, sha1, name});
        }
        m_downloadManager.downloadAll(tasks, progress);
    }
} // namespace strata::loader
