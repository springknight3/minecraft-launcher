#pragma once

#include "../core/auth/OfflineAccountStore.h"
#include "../core/download/DownloadManager.h"
#include "../core/instance/InstanceManager.h"
#include "../core/launch/GameLauncher.h"
#include "../core/launch/GameProcess.h"
#include "../core/mojang/MinecraftInstaller.h"
#include "../core/mojang/MojangManifestClient.h"
#include "../core/runtime/JavaRuntimeManager.h"

#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QThreadPool>
#include <QVariantList>

namespace strata::ui
{
    class LauncherController : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(QVariantList versions READ versions NOTIFY versionsChanged)
        Q_PROPERTY(QVariantList instances READ instances NOTIFY instancesChanged)
        Q_PROPERTY(QString latestRelease READ latestRelease NOTIFY versionsChanged)
        Q_PROPERTY(bool installing READ installing NOTIFY installationChanged)
        Q_PROPERTY(QString currentInstallName READ currentInstallName NOTIFY installationChanged)
        Q_PROPERTY(double installationProgress READ installationProgress NOTIFY installationChanged)
        Q_PROPERTY(QString currentInstallItem READ currentInstallItem NOTIFY installationChanged)
        Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
        Q_PROPERTY(bool usingCachedManifest READ usingCachedManifest NOTIFY versionsChanged)
        Q_PROPERTY(QString offlineUsername READ offlineUsername NOTIFY offlineAccountChanged)
        Q_PROPERTY(QString consoleText READ consoleText NOTIFY consoleTextChanged)
        Q_PROPERTY(
            QString consoleInstanceName READ consoleInstanceName NOTIFY consoleInstanceChanged)
        Q_PROPERTY(QVariantList installedRuntimes READ installedRuntimes NOTIFY runtimesChanged)
        Q_PROPERTY(QString runtimeVendor READ runtimeVendor NOTIFY runtimePreferencesChanged)
        Q_PROPERTY(bool redundantRuntimes READ redundantRuntimes NOTIFY runtimePreferencesChanged)
        Q_PROPERTY(bool installingRuntime READ installingRuntime NOTIFY runtimeInstallationChanged)
        Q_PROPERTY(double runtimeInstallationProgress READ runtimeInstallationProgress NOTIFY
                       runtimeInstallationChanged)
        Q_PROPERTY(QString runtimeInstallationItem READ runtimeInstallationItem NOTIFY
                       runtimeInstallationChanged)

    public:
        explicit LauncherController(QObject* parent = nullptr);
        ~LauncherController() override;

        QVariantList versions() const;
        QVariantList instances() const;
        QString latestRelease() const;
        bool installing() const;
        QString currentInstallName() const;
        double installationProgress() const;
        QString currentInstallItem() const;
        QString errorMessage() const;
        bool usingCachedManifest() const;
        QString offlineUsername() const;
        QString consoleText() const;
        QString consoleInstanceName() const;
        QVariantList installedRuntimes() const;
        QString runtimeVendor() const;
        bool redundantRuntimes() const;
        bool installingRuntime() const;
        double runtimeInstallationProgress() const;
        QString runtimeInstallationItem() const;

        Q_INVOKABLE void refresh();
        Q_INVOKABLE void installInstance(const QString& name, const QString& versionId);
        Q_INVOKABLE void saveOfflineUsername(const QString& name);
        Q_INVOKABLE void launchInstance(const QString& instanceId);
        Q_INVOKABLE void stopInstance(const QString& instanceId);
        Q_INVOKABLE void stopConsoleInstance();
        Q_INVOKABLE void openConsole(const QString& instanceId);
        Q_INVOKABLE void closeConsole();
        Q_INVOKABLE void setRuntimePreferences(const QString& vendor, bool redundant);
        Q_INVOKABLE void installInstanceRuntime(const QString& instanceId);
        Q_INVOKABLE void useSystemJava(const QString& instanceId);
        Q_INVOKABLE void refreshRuntimes();
        Q_INVOKABLE void deleteRuntime(const QString& directory);

    signals:
        void versionsChanged();
        void instancesChanged();
        void installationChanged();
        void errorMessageChanged();
        void installFinished(bool success, const QString& instanceName, const QString& error);
        void offlineAccountChanged();
        void consoleTextChanged();
        void consoleInstanceChanged();
        void processStateChanged(const QString& instanceId, const QString& state);
        void runtimesChanged();
        void runtimePreferencesChanged();
        void runtimeInstallationChanged();

    private:
        strata::InstanceManager m_instanceManager;
        strata::mojang::MojangManifestClient m_manifestClient;
        strata::auth::OfflineAccountStore m_offlineAccountStore;
        QMutex m_instanceMutex;
        strata::download::DownloadManager m_downloadManager;
        strata::runtime::JavaRuntimeManager m_runtimeManager;
        strata::mojang::MinecraftInstaller m_installer;
        QThreadPool m_workers;
        QThreadPool m_logWorkers;
        QList<strata::Instance> m_instanceRecords;
        QList<strata::runtime::RuntimeOwner> m_runtimeOwners;
        QHash<QString, QString> m_processStates;
        QHash<QString, QString> m_processOutput;
        QHash<QString, QString> m_runtimeModes;
        QHash<QString, QString> m_runtimeComponents;
        QHash<QString, QPointer<strata::launch::GameProcess>> m_processes;
        strata::mojang::VersionManifest m_manifest;
        QVariantList m_versions;
        QVariantList m_instances;
        QString m_errorMessage;
        QString m_currentInstallItem;
        QString m_currentInstallName;
        QString m_offlineUsername;
        QString m_consoleText;
        QString m_consoleInstanceId;
        QVariantList m_installedRuntimes;
        QString m_runtimeVendor = QStringLiteral("MOJANG");
        QString m_runtimeInstallationItem;
        int m_installCompleted = 0;
        int m_installTotal = 0;
        int m_runtimeInstallCompleted = 0;
        int m_runtimeInstallTotal = 0;
        bool m_installing = false;
        bool m_redundantRuntimes = false;
        bool m_installingRuntime = false;

        void publishInstances();
        void updateProcessState(const QString& instanceId, const QString& state);
        void publishRuntimes(const QList<strata::runtime::InstalledRuntime>& runtimes);
    };
} // namespace strata::ui
