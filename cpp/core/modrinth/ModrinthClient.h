#pragma once

#include "ContentType.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QUrl>

namespace strata::loader
{
    enum class LoaderType;
}

namespace strata::modrinth
{
    struct Project
    {
        QString id;
        QString slug;
        QString title;
        QString description;
        QString projectType;
        qint64 downloads = 0;
        QString iconUrl;
        QStringList categories;
    };

    struct VersionFile
    {
        QString filename;
        QString url;
        QString sha1;
        bool primary = false;
        qint64 size = -1;
    };

    struct VersionDependency
    {
        QString projectId;
        QString versionId;
        QString dependencyType;
    };

    struct ProjectVersion
    {
        QString id;
        QString projectId;
        QString versionNumber;
        QString projectType;
        QStringList gameVersions;
        QStringList loaders;
        QList<VersionFile> files;
        QList<VersionDependency> dependencies;

        const VersionFile& primaryFile() const;
    };

    struct SearchPage
    {
        QList<Project> projects;
        int total = 0;
        int offset = 0;
        int limit = 0;
    };

    class ModrinthClient
    {
    public:
        explicit ModrinthClient(
            QString projectName = QStringLiteral("StrataLauncher"),
            QString version = QStringLiteral("0.1.0"),
            QUrl apiRoot = QUrl(QStringLiteral("https://api.modrinth.com/v2/")));

        SearchPage search(ContentType type, const QString& query, const QString& minecraftVersion,
                          loader::LoaderType loader, int offset, int limit) const;
        SearchPage searchServer(ServerContentType type, const QString& query,
                                const QString& minecraftVersion, int offset, int limit) const;
        QList<ProjectVersion> versions(const QString& projectId, const QString& minecraftVersion,
                                       loader::LoaderType loader, ContentType type) const;
        Project project(const QString& projectId) const;
        ProjectVersion version(const QString& versionId) const;
        QList<ProjectVersion> serverVersions(const QString& projectId,
                                             const QString& minecraftVersion,
                                             ServerContentType type) const;

        static Project parseProject(const QJsonObject& json);
        static ProjectVersion parseVersion(const QJsonObject& json,
                                           const QString& expectedProjectType);
        static void validateFile(const VersionFile& file);

    private:
        QString m_userAgent;
        QUrl m_apiRoot;

        QJsonDocument getJson(const QUrl& url) const;
    };
} // namespace strata::modrinth
