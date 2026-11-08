#pragma once

#include "ModrinthClient.h"

#include "../download/DownloadManager.h"
#include "../instance/Instance.h"

#include <QList>
#include <QString>

namespace strata::modrinth
{
    struct InstalledContent
    {
        ContentType type = ContentType::Mod;
        QString projectId;
        QString slug;
        QString title;
        QString versionId;
        QString versionNumber;
        QString worldName;
        QString filename;
        QString installedAt;
        bool enabled = true;
    };

    struct DependencyInstall
    {
        ContentType type = ContentType::Mod;
        Project project;
        ProjectVersion version;
    };

    class ContentManager
    {
    public:
        using ProgressCallback = download::DownloadManager::ProgressCallback;

        explicit ContentManager(const ModrinthClient& client,
                                const download::DownloadManager& downloadManager);

        QList<InstalledContent> installed(const Instance& instance, ContentType type) const;
        QStringList worlds(const Instance& instance) const;
        QList<DependencyInstall> resolveRequiredDependencies(const Instance& instance,
                                                             loader::LoaderType loader,
                                                             const Project& project,
                                                             const ProjectVersion& version) const;
        InstalledContent install(const Instance& instance, ContentType type,
                                 loader::LoaderType loader, const QString& worldName,
                                 const Project& project, const ProjectVersion& version,
                                 ProgressCallback progress = {}) const;
        void setEnabled(const Instance& instance, const InstalledContent& content,
                        bool enabled) const;
        void remove(const Instance& instance, const InstalledContent& content) const;

        static void validateFile(const VersionFile& file);
        static QString targetDirectory(const Instance& instance, ContentType type,
                                       const QString& worldName = {});

    private:
        const ModrinthClient& m_client;
        const download::DownloadManager& m_downloadManager;

        QList<InstalledContent> readState(const Instance& instance) const;
        void writeState(const Instance& instance, const QList<InstalledContent>& entries) const;
    };
} // namespace strata::modrinth
