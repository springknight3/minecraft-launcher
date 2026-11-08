#pragma once

#include "../core/auth/AccountManager.h"
#include "../core/auth/CredentialStore.h"
#include "../core/auth/MicrosoftAuth.h"
#include "../core/auth/OfflineAccountStore.h"
#include "../core/download/DownloadManager.h"
#include "../core/instance/InstanceManager.h"
#include "../core/launch/GameLauncher.h"
#include "../core/launch/GameProcess.h"
#include "../core/loader/LoaderInstaller.h"
#include "../core/modrinth/ModrinthClient.h"
#include "../core/mojang/MinecraftInstaller.h"
#include "../core/mojang/MojangManifestClient.h"
#include "../core/runtime/JavaRuntimeManager.h"
#include "../core/server/DevServerManager.h"
#include "../core/server/ServerProcess.h"
#include "../core/settings/LauncherSettings.h"
#include "content/ContentController.h"
#include "settings/InstanceSettingsController.h"

#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QThreadPool>
#include <QVariantList>

#include <functional>

namespace strata::ui
{
    class LauncherController : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(QVariantList versions READ versions NOTIFY versionsChanged)
        Q_PROPERTY(QObject* contentController READ contentController CONSTANT)
        Q_PROPERTY(QStringList loaderVersions READ loaderVersions NOTIFY loaderVersionsChanged)
        Q_PROPERTY(
            bool loadingLoaderVersions READ loadingLoaderVersions NOTIFY loaderVersionsChanged)
        Q_PROPERTY(
            QString loaderVersionsError READ loaderVersionsError NOTIFY loaderVersionsChanged)
        Q_PROPERTY(QVariantList instances READ instances NOTIFY instancesChanged)
        Q_PROPERTY(
            QString lastLaunchedInstanceId READ lastLaunchedInstanceId NOTIFY lastLaunchedChanged)
        Q_PROPERTY(
            QString lastLaunchedInstanceName READ lastLaunchedInstanceName NOTIFY instancesChanged)
        Q_PROPERTY(QVariantList recentLaunchedInstances READ recentLaunchedInstances NOTIFY
                       lastLaunchedChanged)
        Q_PROPERTY(
            bool trackInstanceLaunches READ trackInstanceLaunches NOTIFY launchTrackingChanged)
        Q_PROPERTY(QString latestRelease READ latestRelease NOTIFY versionsChanged)
        Q_PROPERTY(bool installing READ installing NOTIFY installationChanged)
        Q_PROPERTY(bool initializing READ initializing NOTIFY initializationChanged)
        Q_PROPERTY(QString currentInstallName READ currentInstallName NOTIFY installationChanged)
        Q_PROPERTY(double installationProgress READ installationProgress NOTIFY installationChanged)
        Q_PROPERTY(QString currentInstallItem READ currentInstallItem NOTIFY installationChanged)
        Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
        Q_PROPERTY(bool usingCachedManifest READ usingCachedManifest NOTIFY versionsChanged)
        Q_PROPERTY(QString offlineUsername READ offlineUsername NOTIFY offlineAccountChanged)
        Q_PROPERTY(QVariantList accounts READ accounts NOTIFY accountsChanged)
        Q_PROPERTY(QString activeAccountName READ activeAccountName NOTIFY accountsChanged)
        Q_PROPERTY(QString activeAccountId READ activeAccountId NOTIFY accountsChanged)
        Q_PROPERTY(QString microsoftClientId READ microsoftClientId NOTIFY accountsChanged)
        Q_PROPERTY(QString microsoftUserCode READ microsoftUserCode NOTIFY microsoftAuthChanged)
        Q_PROPERTY(QString microsoftVerificationUrl READ microsoftVerificationUrl NOTIFY
                       microsoftAuthChanged)
        Q_PROPERTY(
            bool microsoftAuthInProgress READ microsoftAuthInProgress NOTIFY microsoftAuthChanged)
        Q_PROPERTY(QString consoleText READ consoleText NOTIFY consoleTextChanged)
        Q_PROPERTY(
            QString consoleInstanceName READ consoleInstanceName NOTIFY consoleInstanceChanged)
        Q_PROPERTY(QVariantList installedRuntimes READ installedRuntimes NOTIFY runtimesChanged)
        Q_PROPERTY(QObject* instanceSettingsController READ instanceSettingsController CONSTANT)
        Q_PROPERTY(QString runtimeVendor READ runtimeVendor NOTIFY runtimePreferencesChanged)
        Q_PROPERTY(bool redundantRuntimes READ redundantRuntimes NOTIFY runtimePreferencesChanged)
        Q_PROPERTY(QStringList globalJavaArguments READ globalJavaArguments NOTIFY
                       globalJavaArgumentsChanged)
        Q_PROPERTY(QStringList recommendedJavaArguments READ recommendedJavaArguments CONSTANT)
        Q_PROPERTY(bool installingRuntime READ installingRuntime NOTIFY runtimeInstallationChanged)
        Q_PROPERTY(bool deletingInstance READ deletingInstance NOTIFY deletingInstanceChanged)
        Q_PROPERTY(double runtimeInstallationProgress READ runtimeInstallationProgress NOTIFY
                       runtimeInstallationChanged)
        Q_PROPERTY(QString runtimeInstallationItem READ runtimeInstallationItem NOTIFY
                       runtimeInstallationChanged)
        Q_PROPERTY(QVariantList servers READ servers NOTIFY serversChanged)
        Q_PROPERTY(bool serverBusy READ serverBusy NOTIFY serversChanged)
        Q_PROPERTY(QString serverStatus READ serverStatus NOTIFY serversChanged)
        Q_PROPERTY(QString serverProperties READ serverProperties NOTIFY serversChanged)
        Q_PROPERTY(double serverProgress READ serverProgress NOTIFY serversChanged)
        Q_PROPERTY(QString serverInstallItem READ serverInstallItem NOTIFY serversChanged)
        Q_PROPERTY(QString serverConsoleText READ serverConsoleText NOTIFY serversChanged)
        Q_PROPERTY(QString serverProcessState READ serverProcessState NOTIFY serversChanged)
        Q_PROPERTY(QString serverResourceStatus READ serverResourceStatus NOTIFY serversChanged)
        Q_PROPERTY(QVariantList serverContentProjects READ serverContentProjects NOTIFY
                       serverContentChanged)
        Q_PROPERTY(QVariantList serverContentVersions READ serverContentVersions NOTIFY
                       serverContentChanged)
        Q_PROPERTY(QString serverContentStatus READ serverContentStatus NOTIFY serverContentChanged)
        Q_PROPERTY(bool serverContentBusy READ serverContentBusy NOTIFY serverContentChanged)

    public:
        explicit LauncherController(QObject* parent = nullptr);
        ~LauncherController() override;

        QVariantList versions() const;
        QObject* contentController();
        QStringList loaderVersions() const;
        bool loadingLoaderVersions() const;
        QString loaderVersionsError() const;
        QVariantList instances() const;
        QString lastLaunchedInstanceId() const;
        QString lastLaunchedInstanceName() const;
        QVariantList recentLaunchedInstances() const;
        bool trackInstanceLaunches() const;
        QString latestRelease() const;
        bool installing() const;
        bool initializing() const;
        QString currentInstallName() const;
        double installationProgress() const;
        QString currentInstallItem() const;
        QString errorMessage() const;
        bool usingCachedManifest() const;
        QString offlineUsername() const;
        QVariantList accounts() const;
        QString activeAccountName() const;
        QString activeAccountId() const;
        QString microsoftClientId() const;
        QString microsoftUserCode() const;
        QString microsoftVerificationUrl() const;
        bool microsoftAuthInProgress() const;
        QString consoleText() const;
        QString consoleInstanceName() const;
        QVariantList installedRuntimes() const;
        QObject* instanceSettingsController();
        QString runtimeVendor() const;
        bool redundantRuntimes() const;
        QStringList globalJavaArguments() const;
        QStringList recommendedJavaArguments() const;
        bool installingRuntime() const;
        bool deletingInstance() const;
        double runtimeInstallationProgress() const;
        QString runtimeInstallationItem() const;
        QVariantList servers() const;
        bool serverBusy() const;
        QString serverStatus() const;
        QString serverProperties() const;
        double serverProgress() const;
        QString serverInstallItem() const;
        QString serverConsoleText() const;
        QString serverProcessState() const;
        QString serverResourceStatus() const;
        QVariantList serverContentProjects() const;
        QVariantList serverContentVersions() const;
        QString serverContentStatus() const;
        bool serverContentBusy() const;

        Q_INVOKABLE void refresh();
        Q_INVOKABLE void loadLoaderVersions(const QString& loaderType,
                                            const QString& minecraftVersion);
        Q_INVOKABLE void installInstance(const QString& name, const QString& versionId);
        Q_INVOKABLE void installInstanceWithLoader(const QString& name, const QString& versionId,
                                                   const QString& loaderType,
                                                   const QString& loaderVersion);
        Q_INVOKABLE void saveOfflineUsername(const QString& name);
        Q_INVOKABLE void addOfflineAccount(const QString& name);
        Q_INVOKABLE void selectAccount(const QString& accountId);
        Q_INVOKABLE void removeAccount(const QString& accountId);
        Q_INVOKABLE void setMicrosoftClientId(const QString& clientId);
        Q_INVOKABLE void signInMicrosoft(const QString& clientId);
        Q_INVOKABLE void cancelMicrosoftSignIn();
        Q_INVOKABLE void launchInstance(const QString& instanceId);
        Q_INVOKABLE void deleteInstance(const QString& instanceId);
        Q_INVOKABLE void setTrackInstanceLaunches(bool track);
        Q_INVOKABLE void stopInstance(const QString& instanceId);
        Q_INVOKABLE void stopConsoleInstance();
        Q_INVOKABLE void openConsole(const QString& instanceId);
        Q_INVOKABLE void closeConsole();
        Q_INVOKABLE void setRuntimePreferences(const QString& vendor, bool redundant);
        Q_INVOKABLE void setGlobalJavaArguments(const QString& flags);
        Q_INVOKABLE void installInstanceRuntime(const QString& instanceId);
        Q_INVOKABLE void useSystemJava(const QString& instanceId);
        Q_INVOKABLE void refreshRuntimes();
        Q_INVOKABLE void deleteRuntime(const QString& directory);
        Q_INVOKABLE void refreshServers();
        Q_INVOKABLE void createServer(const QString& name, const QString& version,
                                      const QString& distribution);
        Q_INVOKABLE void updateServerSettings(const QString& id, const QString& name,
                                              bool eulaAccepted, const QString& javaFlags,
                                              int viewDistance, bool onlineMode,
                                              bool useManagedJavaRuntime);
        Q_INVOKABLE void setServerEulaAccepted(const QString& id, bool accepted);
        Q_INVOKABLE void loadServerProperties(const QString& id);
        Q_INVOKABLE void saveServerProperties(const QString& id, const QString& contents);
        Q_INVOKABLE void deleteServer(const QString& id);
        Q_INVOKABLE void selectServer(const QString& id);
        Q_INVOKABLE void installServer(const QString& id);
        Q_INVOKABLE void installServerRuntime(const QString& id);
        Q_INVOKABLE void startServer(const QString& id);
        Q_INVOKABLE void stopServer(const QString& id);
        Q_INVOKABLE void terminateServer(const QString& id);
        Q_INVOKABLE void sendServerCommand(const QString& id, const QString& command);
        Q_INVOKABLE void searchServerContent(const QString& id, const QString& query);
        Q_INVOKABLE void loadServerContentVersions(const QString& id, const QString& projectId);
        Q_INVOKABLE void installServerContent(const QString& id, const QString& projectId,
                                              const QString& versionId);

    signals:
        void versionsChanged();
        void loaderVersionsChanged();
        void instancesChanged();
        void lastLaunchedChanged();
        void launchTrackingChanged();
        void deletingInstanceChanged();
        void installationChanged();
        void initializationChanged();
        void errorMessageChanged();
        void installFinished(bool success, const QString& instanceName, const QString& error);
        void offlineAccountChanged();
        void accountsChanged();
        void microsoftAuthChanged();
        void consoleTextChanged();
        void consoleInstanceChanged();
        void processStateChanged(const QString& instanceId, const QString& state);
        void runtimesChanged();
        void runtimePreferencesChanged();
        void globalJavaArgumentsChanged();
        void runtimeInstallationChanged();
        void serversChanged();
        void serverContentChanged();

    private:
        strata::InstanceManager m_instanceManager;
        strata::mojang::MojangManifestClient m_manifestClient;
        strata::settings::LauncherSettings m_launcherSettings;
        strata::auth::OfflineAccountStore m_offlineAccountStore;
        strata::auth::NativeCredentialStore m_credentialStore;
        strata::auth::AccountManager m_accountManager;
        strata::auth::MicrosoftAuth m_microsoftAuth;
        QMutex m_instanceMutex;
        strata::download::DownloadManager m_downloadManager;
        strata::runtime::JavaRuntimeManager m_runtimeManager;
        strata::mojang::MinecraftInstaller m_installer;
        strata::loader::LoaderInstaller m_loaderInstaller;
        strata::server::DevServerManager m_serverManager;
        ContentController m_contentController;
        QThreadPool m_workers;
        QThreadPool m_logWorkers;
        QList<strata::Instance> m_instanceRecords;
        InstanceSettingsController m_instanceSettingsController;
        QList<strata::runtime::RuntimeOwner> m_runtimeOwners;
        QHash<QString, QString> m_processStates;
        QHash<QString, QString> m_processOutput;
        QHash<QString, QString> m_runtimeModes;
        QHash<QString, QString> m_runtimeComponents;
        QHash<QString, QPointer<strata::launch::GameProcess>> m_processes;
        strata::mojang::VersionManifest m_manifest;
        QVariantList m_versions;
        QStringList m_loaderVersions;
        QString m_loaderVersionsError;
        QVariantList m_instances;
        QString m_lastLaunchedInstanceId;
        QStringList m_recentLaunchedInstanceIds;
        QString m_errorMessage;
        QString m_currentInstallItem;
        QString m_currentInstallName;
        QString m_offlineUsername;
        QString m_microsoftUserCode;
        QString m_microsoftVerificationUrl;
        QString m_consoleText;
        QString m_consoleInstanceId;
        QVariantList m_installedRuntimes;
        QString m_runtimeVendor = QStringLiteral("MOJANG");
        QString m_runtimeInstallationItem;
        QVariantList m_servers;
        QString m_serverProperties;
        QString m_serverStatus;
        QString m_serverInstallItem;
        QString m_serverConsoleText;
        QString m_serverProcessState;
        QString m_serverResourceStatus;
        QVariantList m_serverContentProjects;
        QVariantList m_serverContentVersions;
        QString m_serverContentStatus;
        QString m_selectedServerId;
        QHash<QString, QStringList> m_serverConsoleLines;
        QHash<QString, qsizetype> m_serverConsoleCharacters;
        QHash<QString, QString> m_serverStates;
        QHash<QString, QString> m_serverResources;
        QHash<QString, QPointer<strata::server::ServerProcess>> m_serverProcesses;
        double m_serverProgress = 0.0;
        int m_installCompleted = 0;
        int m_installTotal = 0;
        int m_runtimeInstallCompleted = 0;
        int m_runtimeInstallTotal = 0;
        quint64 m_loaderVersionRequestId = 0;
        bool m_installing = false;
        bool m_initializing = false;
        bool m_loadingLoaderVersions = false;
        bool m_redundantRuntimes = false;
        bool m_installingRuntime = false;
        bool m_deletingInstance = false;
        bool m_microsoftAuthInProgress = false;
        bool m_trackInstanceLaunches = true;
        bool m_serverBusy = false;
        bool m_serverContentBusy = false;
        quint64 m_serverContentRevision = 0;
        int m_initializationTasks = 0;

        void publishInstances();
        void finishInitializationTask();
        void updateProcessState(const QString& instanceId, const QString& state);
        void publishRuntimes(const QList<strata::runtime::InstalledRuntime>& runtimes);
        void runServerOperation(const QString& status, std::function<QVariant()> operation,
                                std::function<void(LauncherController&, const QVariant&)> success);
        void runServerInstall(const QString& id, bool installRuntime);
        void publishServers(QVariantList servers);
        void appendServerConsoleLine(const QString& serverId, const QString& line);
        void updateServerProcessState(const QString& serverId, const QString& state);
    };
} // namespace strata::ui
