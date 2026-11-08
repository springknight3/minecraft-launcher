#pragma once

#include "../download/DownloadManager.h"
#include "../mojang/MojangManifestClient.h"
#include "../runtime/JavaRuntimeManager.h"
#include "ServerDistribution.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

namespace strata::server
{
    struct ServerLaunchPlan
    {
        QString program;
        QStringList arguments;
        QString workingDirectory;
        QStringList javaFlags;
    };

    struct PaperBuild
    {
        int id = 0;
        QString filename;
        QUrl url;
        QString sha256;
    };

    struct ServerConfig
    {
        QString id;
        QString name;
        QString minecraftVersion;
        ServerDistribution distribution = ServerDistribution::Vanilla;
        bool installed = false;
        bool eulaAccepted = false;
        QStringList javaFlags;
        QString createdAt;
        int viewDistance = 10;
        bool onlineMode = true;
        bool useManagedJavaRuntime = false;
        QString javaRuntimeComponent;
    };

    class DevServerManager
    {
    public:
        DevServerManager(QString dataDirectory, const download::DownloadManager& downloadManager,
                         const mojang::MojangManifestClient& manifestClient,
                         runtime::JavaRuntimeManager* javaRuntimeManager = nullptr);

        QVector<ServerConfig> listServers() const;
        ServerConfig create(const QString& name, const QString& minecraftVersion,
                            ServerDistribution distribution) const;
        ServerConfig updateSettings(const ServerConfig& server, const QString& name,
                                    bool eulaAccepted, const QStringList& javaFlags) const;
        ServerConfig updateSettings(const ServerConfig& server, const QString& name,
                                    bool eulaAccepted, const QStringList& javaFlags,
                                    int viewDistance, bool onlineMode,
                                    bool useManagedJavaRuntime = false) const;
        ServerConfig setEulaAccepted(const ServerConfig& server, bool accepted) const;
        ServerConfig install(const ServerConfig& server,
                             download::DownloadManager::ProgressCallback progress = {}) const;
        ServerConfig
        installManagedJavaRuntime(const ServerConfig& server,
                                  download::DownloadManager::ProgressCallback progress = {}) const;
        ServerLaunchPlan launchPlan(const ServerConfig& server) const;
        QString serverDirectoryPath(const ServerConfig& server) const;
        QString readServerProperties(const ServerConfig& server) const;
        ServerConfig saveServerProperties(const ServerConfig& server,
                                          const QString& contents) const;
        void deleteServer(const ServerConfig& server) const;
        static PaperBuild latestStablePaperBuild(const QJsonArray& builds);

    private:
        QString m_dataDirectory;
        QString m_serversDirectory;
        QString m_installerCache;
        const download::DownloadManager& m_downloadManager;
        const mojang::MojangManifestClient& m_manifestClient;
        runtime::JavaRuntimeManager* m_javaRuntimeManager;

        ServerConfig requireServer(const QString& id) const;
        QJsonObject
        fetchVersionMetadata(const QString& versionId,
                             const download::DownloadManager::ProgressCallback& progress) const;
        QString serverRoot(const QString& id) const;
        QString serverDirectory(const ServerConfig& server) const;
        void writeServerProperties(const ServerConfig& server) const;
        void writeConfig(const ServerConfig& server, const QString& root) const;
        static ServerConfig readConfig(const QString& path);
        static QString requireName(const QString& value);
        static QString requireVersion(const QString& value);
        static int requireViewDistance(int value);
        static QString distributionName(ServerDistribution distribution);
        static ServerDistribution parseDistribution(const QString& value);
        static void writeTextAtomically(const QString& path, const QString& contents);
    };
} // namespace strata::server
