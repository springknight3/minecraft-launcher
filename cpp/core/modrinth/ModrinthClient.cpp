#include "ModrinthClient.h"

#include "../loader/LoaderInstaller.h"

#include <QEventLoop>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace strata::modrinth
{
    namespace
    {
        constexpr qsizetype kMaxResponseBytes = 8 * 1024 * 1024;
        constexpr int kMaxPageSize = 100;
        const QRegularExpression kIdentifierPattern(QStringLiteral("^[A-Za-z0-9_-]{1,128}$"));
        const QRegularExpression
            kMinecraftVersionPattern(QStringLiteral("^[A-Za-z0-9._+-]{1,128}$"));

        [[noreturn]] void fail(const QString& message)
        {
            throw std::runtime_error(message.toStdString());
        }

        bool loopbackHost(const QString& host)
        {
            if (host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0)
            {
                return true;
            }
            QHostAddress address;
            return address.setAddress(host) && address.isLoopback();
        }

        void validateApiRoot(const QUrl& root)
        {
            if (!root.isValid() || root.host().isEmpty() ||
                (root.scheme() != QStringLiteral("https") &&
                 !(root.scheme() == QStringLiteral("http") && loopbackHost(root.host()))) ||
                !root.userInfo().isEmpty() ||
                (root.scheme() == QStringLiteral("https") && root.port(-1) != -1 &&
                 root.port(-1) != 443) ||
                !root.path().endsWith(QLatin1Char('/')))
            {
                fail(QStringLiteral("Modrinth API root must be HTTPS."));
            }
        }

        QString requiredString(const QJsonObject& object, const QString& key)
        {
            const QJsonValue value = object.value(key);
            if (!value.isString() || value.toString().trimmed().isEmpty())
            {
                fail(QStringLiteral("Modrinth response is missing %1.").arg(key));
            }
            return value.toString();
        }

        QString optionalString(const QJsonObject& object, const QString& key)
        {
            const QJsonValue value = object.value(key);
            return value.isString() ? value.toString() : QString();
        }

        QStringList stringList(const QJsonObject& object, const QString& key)
        {
            const QJsonValue value = object.value(key);
            if (value.isUndefined())
            {
                return {};
            }
            if (!value.isArray())
            {
                fail(QStringLiteral("Modrinth response contains an invalid %1 list.").arg(key));
            }
            QStringList result;
            for (const QJsonValue& item : value.toArray())
            {
                if (!item.isString())
                {
                    fail(QStringLiteral("Modrinth response contains an invalid %1 item.").arg(key));
                }
                result.append(item.toString());
            }
            return result;
        }

        QJsonArray oneFacet(const QString& value)
        {
            return {value};
        }

        void addFacet(QJsonArray& facets, const QString& value)
        {
            facets.append(oneFacet(value));
        }

        QUrl searchUrl(const QUrl& apiRoot, const QString& query, const QJsonArray& facets,
                       int offset, int limit, bool sortByDownloads)
        {
            QUrl url = apiRoot.resolved(QUrl(QStringLiteral("search")));
            QUrlQuery parameters;
            parameters.addQueryItem(QStringLiteral("query"), query.trimmed());
            parameters.addQueryItem(
                QStringLiteral("facets"),
                QString::fromUtf8(QJsonDocument(facets).toJson(QJsonDocument::Compact)));
            if (sortByDownloads)
            {
                parameters.addQueryItem(QStringLiteral("index"), QStringLiteral("downloads"));
            }
            parameters.addQueryItem(QStringLiteral("offset"), QString::number(offset));
            parameters.addQueryItem(QStringLiteral("limit"), QString::number(limit));
            url.setQuery(parameters);
            return url;
        }

        QList<Project> parseSearchProjects(const QJsonDocument& document, int* total)
        {
            if (!document.isObject())
            {
                fail(QStringLiteral("Modrinth returned an invalid search response."));
            }
            const QJsonObject response = document.object();
            const QJsonValue hitsValue = response.value(QStringLiteral("hits"));
            if (!hitsValue.isArray())
            {
                fail(QStringLiteral("Modrinth search response is missing hits."));
            }
            const QJsonValue totalValue = response.value(
                response.contains(QStringLiteral("total_hits")) ? QStringLiteral("total_hits")
                                                                : QStringLiteral("total"));
            if (!totalValue.isDouble() || totalValue.toDouble() < 0 ||
                totalValue.toDouble() > std::numeric_limits<int>::max())
            {
                fail(QStringLiteral("Modrinth search response contains an invalid result count."));
            }
            *total = static_cast<int>(totalValue.toDouble());
            QList<Project> projects;
            for (const QJsonValue& value : hitsValue.toArray())
            {
                if (value.isObject())
                {
                    projects.append(ModrinthClient::parseProject(value.toObject()));
                }
            }
            return projects;
        }

        void validatePagination(int offset, int limit)
        {
            if (offset < 0 || limit < 1 || limit > kMaxPageSize)
            {
                fail(QStringLiteral("Modrinth pagination requires offset >= 0 and limit 1-100."));
            }
        }

        void validateProjectId(const QString& projectId)
        {
            if (!kIdentifierPattern.match(projectId).hasMatch())
            {
                fail(QStringLiteral("Invalid Modrinth project identifier."));
            }
        }

        void validateMinecraftVersion(const QString& version)
        {
            if (!kMinecraftVersionPattern.match(version).hasMatch())
            {
                fail(QStringLiteral("Invalid Minecraft version for Modrinth lookup."));
            }
        }

        QString encodedStringList(const QString& value)
        {
            return QString::fromUtf8(
                QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact));
        }

        QByteArray fetchBody(const QUrl& url, const QString& userAgent)
        {
            QNetworkAccessManager manager;
            QNetworkRequest request(url);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);
            request.setTransferTimeout(30000);
            request.setRawHeader("Accept", "application/json");
            request.setRawHeader("User-Agent", userAgent.toUtf8());
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
                                 if (body.size() + chunk.size() > kMaxResponseBytes)
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
            if (!tooLarge && body.size() + remaining.size() <= kMaxResponseBytes)
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
                fail(QStringLiteral("Modrinth request timed out."));
            }
            if (tooLarge)
            {
                fail(QStringLiteral("Modrinth response exceeds the size limit."));
            }
            if (networkError != QNetworkReply::NoError || status < 200 || status >= 300)
            {
                fail(QStringLiteral("Modrinth API returned HTTP %1: %2").arg(status).arg(error));
            }
            return body;
        }

        QString loaderCategory(loader::LoaderType loader)
        {
            switch (loader)
            {
            case loader::LoaderType::Fabric:
                return QStringLiteral("fabric");
            case loader::LoaderType::Quilt:
                return QStringLiteral("quilt");
            case loader::LoaderType::Forge:
                return QStringLiteral("forge");
            case loader::LoaderType::NeoForge:
                return QStringLiteral("neoforge");
            case loader::LoaderType::Vanilla:
                fail(QStringLiteral("Mods require an installed mod loader."));
            }
            fail(QStringLiteral("Unknown mod loader."));
        }

        QStringList parseStringList(const QJsonObject& object, const QString& key)
        {
            return stringList(object, key);
        }
    } // namespace

    const VersionFile& ProjectVersion::primaryFile() const
    {
        if (files.isEmpty())
        {
            fail(QStringLiteral("Modrinth version has no downloadable files."));
        }
        for (const VersionFile& file : files)
        {
            if (file.primary)
            {
                return file;
            }
        }
        return files.constFirst();
    }

    ModrinthClient::ModrinthClient(QString projectName, QString version, QUrl apiRoot)
        : m_userAgent(QStringLiteral("%1/%2 (https://github.com/springknight3/minecraft-launcher)")
                          .arg(projectName, version)),
          m_apiRoot(std::move(apiRoot))
    {
        static const QRegularExpression projectPattern(QStringLiteral("^[A-Za-z0-9._-]+$"));
        static const QRegularExpression versionPattern(QStringLiteral("^[A-Za-z0-9._+-]+$"));
        if (!projectPattern.match(projectName).hasMatch() ||
            !versionPattern.match(version).hasMatch() || m_userAgent.contains(QLatin1Char('\n')) ||
            m_userAgent.contains(QLatin1Char('\r')))
        {
            fail(QStringLiteral("Invalid Modrinth application name or version."));
        }
        validateApiRoot(m_apiRoot);
    }

    SearchPage ModrinthClient::search(ContentType type, const QString& query,
                                      const QString& minecraftVersion, loader::LoaderType loader,
                                      int offset, int limit) const
    {
        validatePagination(offset, limit);
        if (minecraftVersion.trimmed().isEmpty())
        {
            fail(QStringLiteral("Choose a Minecraft version before searching Modrinth."));
        }
        validateMinecraftVersion(minecraftVersion);
        if (query.size() > 256 || query.contains(QLatin1Char('\n')) ||
            query.contains(QLatin1Char('\r')))
        {
            fail(QStringLiteral(
                "Modrinth search query must be a single line of at most 256 characters."));
        }
        QJsonArray facets;
        addFacet(facets, QStringLiteral("project_type:%1").arg(projectType(type)));
        addFacet(facets, QStringLiteral("versions:%1").arg(minecraftVersion));
        if (type == ContentType::Mod)
        {
            addFacet(facets, QStringLiteral("categories:%1").arg(loaderCategory(loader)));
        }
        const QJsonDocument response =
            getJson(searchUrl(m_apiRoot, query, facets, offset, limit, true));
        int total = 0;
        QList<Project> projects = parseSearchProjects(response, &total);
        return {projects, total, offset, limit};
    }

    SearchPage ModrinthClient::searchServer(ServerContentType type, const QString& query,
                                            const QString& minecraftVersion, int offset,
                                            int limit) const
    {
        validatePagination(offset, limit);
        if (minecraftVersion.trimmed().isEmpty())
        {
            fail(QStringLiteral("Choose a Minecraft version before searching Modrinth."));
        }
        validateMinecraftVersion(minecraftVersion);
        if (query.size() > 256 || query.contains(QLatin1Char('\n')) ||
            query.contains(QLatin1Char('\r')))
        {
            fail(QStringLiteral(
                "Modrinth search query must be a single line of at most 256 characters."));
        }
        QJsonArray facets;
        addFacet(facets, QStringLiteral("project_type:%1").arg(projectType(type)));
        addFacet(facets, QStringLiteral("versions:%1").arg(minecraftVersion));
        const QJsonDocument response =
            getJson(searchUrl(m_apiRoot, query, facets, offset, limit, false));
        int total = 0;
        QList<Project> projects = parseSearchProjects(response, &total);
        projects.erase(std::remove_if(projects.begin(), projects.end(),
                                      [type](const Project& project)
                                      { return project.projectType != projectType(type); }),
                       projects.end());
        return {projects, total, offset, limit};
    }

    QList<ProjectVersion> ModrinthClient::versions(const QString& projectId,
                                                   const QString& minecraftVersion,
                                                   loader::LoaderType loader,
                                                   ContentType type) const
    {
        validateProjectId(projectId);
        if (minecraftVersion.trimmed().isEmpty())
        {
            fail(QStringLiteral("Minecraft version is required."));
        }
        validateMinecraftVersion(minecraftVersion);
        QString requiredLoader;
        if (type == ContentType::Mod)
        {
            requiredLoader = loaderCategory(loader);
        }
        QUrl url = m_apiRoot.resolved(
            QUrl(QStringLiteral("project/%1/version")
                     .arg(QString::fromLatin1(QUrl::toPercentEncoding(projectId)))));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("game_versions"), encodedStringList(minecraftVersion));
        if (!requiredLoader.isEmpty())
        {
            query.addQueryItem(QStringLiteral("loaders"),
                               QStringLiteral("[\"%1\"]").arg(requiredLoader));
        }
        url.setQuery(query);
        const QJsonDocument response = getJson(url);
        if (!response.isArray())
        {
            fail(QStringLiteral("Modrinth returned an invalid project version list."));
        }
        QList<ProjectVersion> result;
        for (const QJsonValue& value : response.array())
        {
            if (!value.isObject())
            {
                continue;
            }
            ProjectVersion version = parseVersion(value.toObject(), projectType(type));
            if (version.projectId == projectId && version.gameVersions.contains(minecraftVersion) &&
                (requiredLoader.isEmpty() || version.loaders.contains(requiredLoader)))
            {
                result.append(std::move(version));
            }
        }
        return result;
    }

    Project ModrinthClient::project(const QString& projectId) const
    {
        validateProjectId(projectId);
        const QUrl url = m_apiRoot.resolved(
            QUrl(QStringLiteral("project/%1")
                     .arg(QString::fromLatin1(QUrl::toPercentEncoding(projectId)))));
        const QJsonDocument response = getJson(url);
        if (!response.isObject())
        {
            fail(QStringLiteral("Modrinth returned an invalid project response."));
        }
        Project result = parseProject(response.object());
        if (result.id != projectId)
        {
            fail(QStringLiteral("Modrinth returned a different project than requested."));
        }
        return result;
    }

    ProjectVersion ModrinthClient::version(const QString& versionId) const
    {
        if (!kIdentifierPattern.match(versionId).hasMatch())
        {
            fail(QStringLiteral("Invalid Modrinth version identifier."));
        }
        const QUrl url = m_apiRoot.resolved(
            QUrl(QStringLiteral("version/%1")
                     .arg(QString::fromLatin1(QUrl::toPercentEncoding(versionId)))));
        const QJsonDocument response = getJson(url);
        if (!response.isObject())
        {
            fail(QStringLiteral("Modrinth returned an invalid version response."));
        }
        const QString projectId = requiredString(response.object(), QStringLiteral("project_id"));
        const Project projectInfo = project(projectId);
        ProjectVersion result = parseVersion(response.object(), projectInfo.projectType);
        if (result.id != versionId)
        {
            fail(QStringLiteral("Modrinth returned a different version than requested."));
        }
        return result;
    }

    QList<ProjectVersion> ModrinthClient::serverVersions(const QString& projectId,
                                                         const QString& minecraftVersion,
                                                         ServerContentType type) const
    {
        validateProjectId(projectId);
        if (minecraftVersion.trimmed().isEmpty())
        {
            fail(QStringLiteral("Minecraft version is required."));
        }
        validateMinecraftVersion(minecraftVersion);
        QUrl url = m_apiRoot.resolved(
            QUrl(QStringLiteral("project/%1/version")
                     .arg(QString::fromLatin1(QUrl::toPercentEncoding(projectId)))));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("game_versions"), encodedStringList(minecraftVersion));
        url.setQuery(query);
        const QJsonDocument response = getJson(url);
        if (!response.isArray())
        {
            fail(QStringLiteral("Modrinth returned an invalid server-content version list."));
        }
        QList<ProjectVersion> result;
        const QString category = projectType(type);
        for (const QJsonValue& value : response.array())
        {
            if (!value.isObject())
            {
                continue;
            }
            ProjectVersion version = parseVersion(value.toObject(), category);
            if (version.projectId == projectId && version.projectType == category &&
                version.gameVersions.contains(minecraftVersion))
            {
                result.append(std::move(version));
            }
        }
        return result;
    }

    Project ModrinthClient::parseProject(const QJsonObject& json)
    {
        Project project;
        project.id = requiredString(json, json.contains(QStringLiteral("project_id"))
                                              ? QStringLiteral("project_id")
                                              : QStringLiteral("id"));
        if (!kIdentifierPattern.match(project.id).hasMatch())
        {
            fail(QStringLiteral("Modrinth returned an invalid project identifier."));
        }
        project.slug = optionalString(json, QStringLiteral("slug"));
        project.title = requiredString(json, QStringLiteral("title"));
        project.description = optionalString(json, QStringLiteral("description"));
        project.projectType = requiredString(json, QStringLiteral("project_type"));
        const bool contentTypeFound = project.projectType == QStringLiteral("mod") ||
                                      project.projectType == QStringLiteral("resourcepack") ||
                                      project.projectType == QStringLiteral("datapack") ||
                                      project.projectType == QStringLiteral("shader") ||
                                      project.projectType == QStringLiteral("plugin");
        if (!contentTypeFound)
        {
            fail(QStringLiteral("Modrinth returned an unsupported project type."));
        }
        const QJsonValue downloads = json.value(QStringLiteral("downloads"));
        if (downloads.isDouble() && downloads.toDouble() >= 0 &&
            downloads.toDouble() <= static_cast<double>(std::numeric_limits<qint64>::max()))
        {
            project.downloads = static_cast<qint64>(downloads.toDouble());
        }
        project.iconUrl = optionalString(json, QStringLiteral("icon_url"));
        project.categories = parseStringList(json, QStringLiteral("categories"));
        return project;
    }

    ProjectVersion ModrinthClient::parseVersion(const QJsonObject& json,
                                                const QString& expectedProjectType)
    {
        ProjectVersion version;
        version.id = requiredString(json, QStringLiteral("id"));
        version.projectId = requiredString(json, QStringLiteral("project_id"));
        if (!kIdentifierPattern.match(version.id).hasMatch() ||
            !kIdentifierPattern.match(version.projectId).hasMatch())
        {
            fail(QStringLiteral("Modrinth returned an invalid version identifier."));
        }
        version.versionNumber = requiredString(json, QStringLiteral("version_number"));
        version.projectType = json.contains(QStringLiteral("project_type"))
                                  ? requiredString(json, QStringLiteral("project_type"))
                                  : expectedProjectType;
        if (version.projectType != expectedProjectType)
        {
            fail(QStringLiteral("Modrinth returned a version for a different content category."));
        }
        version.gameVersions = parseStringList(json, QStringLiteral("game_versions"));
        version.loaders = parseStringList(json, QStringLiteral("loaders"));
        const QJsonValue filesValue = json.value(QStringLiteral("files"));
        if (!filesValue.isArray())
        {
            fail(QStringLiteral("Modrinth version is missing downloadable files."));
        }
        for (const QJsonValue& value : filesValue.toArray())
        {
            if (!value.isObject())
            {
                continue;
            }
            const QJsonObject file = value.toObject();
            const QJsonObject hashes = file.value(QStringLiteral("hashes")).toObject();
            const QJsonValue sizeValue = file.value(QStringLiteral("size"));
            qint64 size = -1;
            if (sizeValue.isDouble() && sizeValue.toDouble() >= 0 &&
                sizeValue.toDouble() <= static_cast<double>(std::numeric_limits<qint64>::max()))
            {
                size = static_cast<qint64>(sizeValue.toDouble());
            }
            version.files.append({requiredString(file, QStringLiteral("filename")),
                                  requiredString(file, QStringLiteral("url")),
                                  optionalString(hashes, QStringLiteral("sha1")),
                                  file.value(QStringLiteral("primary")).toBool(), size});
        }
        if (version.files.isEmpty())
        {
            fail(QStringLiteral("Modrinth version has no downloadable files."));
        }
        const QJsonValue dependencies = json.value(QStringLiteral("dependencies"));
        if (!dependencies.isUndefined())
        {
            if (!dependencies.isArray())
            {
                fail(QStringLiteral("Modrinth version dependencies are invalid."));
            }
            for (const QJsonValue& value : dependencies.toArray())
            {
                if (!value.isObject())
                {
                    fail(QStringLiteral("Modrinth version contains an invalid dependency."));
                }
                const QJsonObject dependency = value.toObject();
                VersionDependency parsed{
                    optionalString(dependency, QStringLiteral("project_id")),
                    optionalString(dependency, QStringLiteral("version_id")),
                    requiredString(dependency, QStringLiteral("dependency_type"))};
                if ((parsed.projectId.isEmpty() && parsed.versionId.isEmpty()) ||
                    (!parsed.projectId.isEmpty() &&
                     !kIdentifierPattern.match(parsed.projectId).hasMatch()) ||
                    (!parsed.versionId.isEmpty() &&
                     !kIdentifierPattern.match(parsed.versionId).hasMatch()))
                {
                    fail(QStringLiteral("Modrinth version contains an invalid dependency ID."));
                }
                version.dependencies.append(std::move(parsed));
            }
        }
        return version;
    }

    void ModrinthClient::validateFile(const VersionFile& file)
    {
        const QString filename = file.filename;
        if (filename.trimmed().isEmpty() || filename == QStringLiteral(".") ||
            filename == QStringLiteral("..") || filename.contains(QLatin1Char('/')) ||
            filename.contains(QLatin1Char('\\')) ||
            std::any_of(filename.cbegin(), filename.cend(),
                        [](QChar character) { return character.isNull() || !character.isPrint(); }))
        {
            fail(QStringLiteral("Modrinth returned an unsafe content filename."));
        }
        const QUrl url(file.url);
        const QString host = url.host().toLower();
        const bool allowedHost = host == QStringLiteral("modrinth.com") ||
                                 host.endsWith(QStringLiteral(".modrinth.com")) ||
                                 host == QStringLiteral("modrinth.gg") ||
                                 host.endsWith(QStringLiteral(".modrinth.gg"));
        if (!url.isValid() || url.scheme() != QStringLiteral("https") ||
            !url.userInfo().isEmpty() || (url.port(-1) != -1 && url.port(-1) != 443) ||
            !allowedHost)
        {
            fail(QStringLiteral("Modrinth file downloads must use the official HTTPS CDN."));
        }
        if (!QRegularExpression(QStringLiteral("(?i)^[0-9a-f]{40}$")).match(file.sha1).hasMatch())
        {
            fail(QStringLiteral("Modrinth file is missing a valid SHA-1 integrity hash."));
        }
    }

    QJsonDocument ModrinthClient::getJson(const QUrl& url) const
    {
        const QByteArray bytes = fetchBody(url, m_userAgent);
        QJsonParseError error{};
        const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
        if (error.error != QJsonParseError::NoError ||
            (!document.isObject() && !document.isArray()))
        {
            fail(QStringLiteral("Modrinth returned malformed JSON."));
        }
        return document;
    }
} // namespace strata::modrinth
