#pragma once

#include "../download/DownloadManager.h"
#include "../instance/Instance.h"
#include "../runtime/JavaRuntimeManager.h"

#include <QJsonObject>
#include <QStringList>

#include <optional>

namespace strata::loader
{
    enum class LoaderType
    {
        Vanilla,
        Fabric,
        Quilt,
        Forge,
        NeoForge
    };

    struct LoaderSelection
    {
        LoaderType type = LoaderType::Vanilla;
        QString version;

        static LoaderSelection vanilla();
        static LoaderType parseType(const QString& name);
        static QString typeName(LoaderType type);
    };

    struct LoaderProfile
    {
        LoaderType type = LoaderType::Vanilla;
        QString minecraftVersion;
        QString loaderVersion;
        QString profileVersionId;
        QJsonObject launchMetadata;
    };

    class LoaderInstaller
    {
    public:
        using ProgressCallback = download::DownloadManager::ProgressCallback;

        LoaderInstaller(const download::DownloadManager& downloadManager,
                        const runtime::JavaRuntimeManager& runtimeManager, QString dataDirectory);

        QStringList availableVersions(LoaderType type, const QString& minecraftVersion) const;
        LoaderSelection installedSelection(const Instance& instance) const;
        static std::optional<LoaderProfile> installedProfile(const Instance& instance);
        void prepare(const Instance& instance, const LoaderSelection& selection) const;
        void install(const Instance& instance, const LoaderSelection& selection,
                     ProgressCallback progress = {}) const;

        static LoaderProfile parseProfile(LoaderType type, const QString& minecraftVersion,
                                          const QString& loaderVersion, const QJsonObject& profile);
        static QStringList parseMavenVersions(const QByteArray& xml, const QString& prefix);
        static QString neoForgeCompatibilityPrefix(const QString& minecraftVersion);
        static QJsonObject mergeMetadata(const QJsonObject& minecraftMetadata,
                                         const LoaderProfile& profile);
        static QString resolveLibraryPath(const QString& root, const QString& relativePath);

    private:
        const download::DownloadManager& m_downloadManager;
        const runtime::JavaRuntimeManager& m_runtimeManager;
        QString m_dataDirectory;
        QString m_installerCache;
        QString m_librariesDirectory;

        LoaderProfile installProfileBased(LoaderType type, const QString& minecraftVersion,
                                          const QString& loaderVersion,
                                          const ProgressCallback& progress) const;
        LoaderProfile installOfficial(LoaderType type, const Instance& instance,
                                      const QString& loaderVersion,
                                      const ProgressCallback& progress) const;
        void downloadLibraries(LoaderType type, const QJsonObject& profile,
                               const ProgressCallback& progress) const;
    };
} // namespace strata::loader
