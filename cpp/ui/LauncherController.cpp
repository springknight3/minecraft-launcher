#include "LauncherController.h"

#include "../core/settings/InstanceSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMutexLocker>
#include <QPointer>
#include <QRunnable>

#include <algorithm>
#include <stdexcept>

namespace strata::ui
{
    namespace
    {
        QString exceptionMessage()
        {
            try
            {
                throw;
            }
            catch (const std::exception& exception)
            {
                return QString::fromUtf8(exception.what());
            }
            catch (...)
            {
                return QStringLiteral("An unknown error occurred");
            }
        }

        QVariantList toVariantInstances(const QList<strata::Instance>& instances,
                                        const QHash<QString, QString>& processStates,
                                        const QHash<QString, QString>& runtimeModes,
                                        const QHash<QString, QString>& runtimeComponents)
        {
            QVariantList values;
            values.reserve(instances.size());
            for (const strata::Instance& instance : instances)
            {
                const QString state = processStates.value(instance.id, QStringLiteral("stopped"));
                values.append(QVariantMap{
                    {QStringLiteral("id"), instance.id},
                    {QStringLiteral("name"), instance.name},
                    {QStringLiteral("version"), instance.versionId},
                    {QStringLiteral("favorite"), instance.favorite},
                    {QStringLiteral("runtimeMode"),
                     runtimeModes.value(instance.id, QStringLiteral("SYSTEM"))},
                    {QStringLiteral("runtimeComponent"), runtimeComponents.value(instance.id)},
                    {QStringLiteral("status"), state}});
            }
            return values;
        }

        void readRuntimeOwners(const QList<strata::Instance>& instances,
                               QHash<QString, QString>& modes, QHash<QString, QString>& components,
                               QList<strata::runtime::RuntimeOwner>& owners,
                               const strata::server::DevServerManager* serverManager = nullptr)
        {
            modes.clear();
            components.clear();
            owners.clear();
            for (const strata::Instance& instance : instances)
            {
                const strata::InstanceSettings settings(instance.directory);
                const bool managed = settings.javaRuntimeMode() ==
                                     strata::InstanceSettings::JavaRuntimeMode::Managed;
                const QString mode = managed ? QStringLiteral("MANAGED") : QStringLiteral("SYSTEM");
                const QString component = settings.javaRuntimeComponent();
                const bool redundant = settings.javaRuntimeRedundant();
                modes.insert(instance.id, mode);
                components.insert(instance.id, component);
                owners.append({QStringLiteral("Instance: %1").arg(instance.name),
                               instance.directory, component, managed, redundant});
            }
            if (serverManager != nullptr)
            {
                for (const strata::server::ServerConfig& server : serverManager->listServers())
                {
                    if (!server.useManagedJavaRuntime || server.javaRuntimeComponent.isEmpty())
                    {
                        continue;
                    }
                    const QString directory = serverManager->serverDirectoryPath(server);
                    const strata::InstanceSettings settings(directory);
                    owners.append({QStringLiteral("Server: %1").arg(server.name), directory,
                                   server.javaRuntimeComponent, true,
                                   settings.javaRuntimeRedundant()});
                }
            }
        }

        QVariantMap serverVariant(const strata::server::ServerConfig& server)
        {
            QString distribution;
            switch (server.distribution)
            {
            case strata::server::ServerDistribution::Vanilla:
                distribution = QStringLiteral("VANILLA");
                break;
            case strata::server::ServerDistribution::Fabric:
                distribution = QStringLiteral("FABRIC");
                break;
            case strata::server::ServerDistribution::Paper:
                distribution = QStringLiteral("PAPER");
                break;
            }
            return {{QStringLiteral("id"), server.id},
                    {QStringLiteral("name"), server.name},
                    {QStringLiteral("version"), server.minecraftVersion},
                    {QStringLiteral("distribution"), distribution},
                    {QStringLiteral("installed"), server.installed},
                    {QStringLiteral("eulaAccepted"), server.eulaAccepted},
                    {QStringLiteral("javaFlags"), server.javaFlags},
                    {QStringLiteral("viewDistance"), server.viewDistance},
                    {QStringLiteral("onlineMode"), server.onlineMode},
                    {QStringLiteral("useManagedJavaRuntime"), server.useManagedJavaRuntime},
                    {QStringLiteral("javaRuntimeComponent"), server.javaRuntimeComponent}};
        }

        strata::server::ServerDistribution serverDistribution(const QString& value)
        {
            if (value == QStringLiteral("VANILLA"))
            {
                return strata::server::ServerDistribution::Vanilla;
            }
            if (value == QStringLiteral("FABRIC"))
            {
                return strata::server::ServerDistribution::Fabric;
            }
            if (value == QStringLiteral("PAPER"))
            {
                return strata::server::ServerDistribution::Paper;
            }
            throw std::invalid_argument("Unsupported server distribution.");
        }

        strata::modrinth::ServerContentType
        serverContentType(strata::server::ServerDistribution distribution)
        {
            if (distribution == strata::server::ServerDistribution::Paper)
            {
                return strata::modrinth::ServerContentType::Plugin;
            }
            if (distribution == strata::server::ServerDistribution::Fabric)
            {
                return strata::modrinth::ServerContentType::Mod;
            }
            throw std::invalid_argument("Modrinth server content requires Paper or Fabric.");
        }
    } // namespace

    LauncherController::LauncherController(QObject* parent)
        : QObject(parent), m_instanceManager(), m_manifestClient(),
          m_launcherSettings(m_instanceManager.dataDirectory()),
          m_offlineAccountStore(m_instanceManager.dataDirectory()), m_credentialStore(),
          m_accountManager(m_instanceManager.dataDirectory(), m_credentialStore), m_microsoftAuth(),
          m_downloadManager(),
          m_runtimeManager(m_downloadManager, m_instanceManager.dataDirectory()),
          m_installer(m_downloadManager, m_instanceManager.dataDirectory()),
          m_loaderInstaller(m_downloadManager, m_runtimeManager, m_instanceManager.dataDirectory()),
          m_serverManager(m_instanceManager.dataDirectory(), m_downloadManager, m_manifestClient,
                          &m_runtimeManager),
          m_contentController(m_instanceManager, m_loaderInstaller, m_downloadManager),
          m_instanceSettingsController(m_instanceMutex, m_instanceRecords, this)
    {
        m_workers.setMaxThreadCount(3);
        m_logWorkers.setMaxThreadCount(1);
        m_lastLaunchedInstanceId = m_launcherSettings.lastLaunchedInstanceId();
        m_recentLaunchedInstanceIds = m_launcherSettings.recentLaunchedInstanceIds();
        m_trackInstanceLaunches = m_launcherSettings.trackInstanceLaunches();
        try
        {
            const auto preferences = m_runtimeManager.preferences();
            const auto vendor = strata::runtime::JavaRuntimeManager::parseVendor(
                m_launcherSettings.runtimeVendor());
            m_runtimeVendor = strata::runtime::JavaRuntimeManager::vendorName(vendor).toUpper();
            m_redundantRuntimes = m_launcherSettings.redundantJavaRuntimes();
            if (preferences.vendor != vendor ||
                preferences.redundantRuntimes != m_redundantRuntimes)
            {
                m_runtimeManager.setPreferences({vendor, m_redundantRuntimes});
            }
        }
        catch (const std::exception& exception)
        {
            m_errorMessage = QString::fromUtf8(exception.what());
        }
        try
        {
            m_offlineUsername = m_offlineAccountStore.username();
            if (m_accountManager.accounts().isEmpty() && !m_offlineUsername.isEmpty())
            {
                m_accountManager.addOffline(m_offlineUsername);
            }
            const auto activeAccount = m_accountManager.activeAccount();
            const QString accountClientId = m_accountManager.clientId();
            if (m_launcherSettings.azureClientId().isEmpty() && !accountClientId.isEmpty())
            {
                m_launcherSettings.setAzureClientId(accountClientId);
            }
            else if (!m_launcherSettings.azureClientId().isEmpty() &&
                     accountClientId != m_launcherSettings.azureClientId())
            {
                m_accountManager.setClientId(m_launcherSettings.azureClientId());
            }
            m_offlineUsername =
                activeAccount && activeAccount->type == strata::auth::Account::Type::Offline
                    ? activeAccount->name
                    : QString();
        }
        catch (const std::exception& exception)
        {
            m_errorMessage = QString::fromUtf8(exception.what());
        }
        connect(&m_microsoftAuth, &strata::auth::MicrosoftAuth::userCodeReady, this,
                [this](const QString& code, const QUrl& uri, const QString&)
                {
                    m_microsoftUserCode = code;
                    m_microsoftVerificationUrl = uri.toString();
                    emit microsoftAuthChanged();
                });
    }

    LauncherController::~LauncherController()
    {
        m_microsoftAuth.cancel();
        for (const auto& process : m_processes)
        {
            if (process && process->isRunning())
            {
                process->stop();
            }
        }
        for (const auto& process : m_serverProcesses)
        {
            if (process && process->isRunning())
            {
                process->terminate();
            }
        }
        m_workers.waitForDone();
        m_logWorkers.waitForDone();
    }

    QVariantList LauncherController::versions() const
    {
        return m_versions;
    }

    QObject* LauncherController::contentController()
    {
        return &m_contentController;
    }

    QObject* LauncherController::instanceSettingsController()
    {
        return &m_instanceSettingsController;
    }

    QStringList LauncherController::loaderVersions() const
    {
        return m_loaderVersions;
    }

    bool LauncherController::loadingLoaderVersions() const
    {
        return m_loadingLoaderVersions;
    }

    QString LauncherController::loaderVersionsError() const
    {
        return m_loaderVersionsError;
    }

    QVariantList LauncherController::instances() const
    {
        return m_instances;
    }

    QString LauncherController::lastLaunchedInstanceId() const
    {
        return m_lastLaunchedInstanceId;
    }

    QString LauncherController::lastLaunchedInstanceName() const
    {
        for (const strata::Instance& instance : m_instanceRecords)
        {
            if (instance.id == m_lastLaunchedInstanceId)
            {
                return instance.name;
            }
        }
        return {};
    }

    QVariantList LauncherController::recentLaunchedInstances() const
    {
        QVariantList result;
        for (const QString& id : m_recentLaunchedInstanceIds)
        {
            for (const strata::Instance& instance : m_instanceRecords)
            {
                if (instance.id == id)
                {
                    result.append(QVariantMap{{QStringLiteral("id"), instance.id},
                                              {QStringLiteral("name"), instance.name}});
                    break;
                }
            }
        }
        return result;
    }

    bool LauncherController::trackInstanceLaunches() const
    {
        return m_trackInstanceLaunches;
    }

    QString LauncherController::latestRelease() const
    {
        return m_manifest.latestRelease;
    }

    bool LauncherController::installing() const
    {
        return m_installing;
    }

    bool LauncherController::initializing() const
    {
        return m_initializing;
    }

    QString LauncherController::currentInstallName() const
    {
        return m_currentInstallName;
    }

    double LauncherController::installationProgress() const
    {
        return m_installTotal == 0 ? 0.0 : static_cast<double>(m_installCompleted) / m_installTotal;
    }

    QString LauncherController::currentInstallItem() const
    {
        return m_currentInstallItem;
    }

    QString LauncherController::errorMessage() const
    {
        return m_errorMessage;
    }

    bool LauncherController::usingCachedManifest() const
    {
        return m_manifest.usedCache;
    }

    QString LauncherController::offlineUsername() const
    {
        return m_offlineUsername;
    }

    QVariantList LauncherController::accounts() const
    {
        QVariantList result;
        const auto active = m_accountManager.activeAccount();
        for (const strata::auth::Account& account : m_accountManager.accounts())
        {
            result.append(QVariantMap{
                {QStringLiteral("accountId"), account.id},
                {QStringLiteral("name"), account.name},
                {QStringLiteral("type"), account.type == strata::auth::Account::Type::Microsoft
                                             ? QStringLiteral("Microsoft")
                                             : QStringLiteral("Offline")},
                {QStringLiteral("active"), active && active->id == account.id}});
        }
        return result;
    }

    QString LauncherController::activeAccountName() const
    {
        const auto active = m_accountManager.activeAccount();
        return active ? active->name : QString();
    }

    QString LauncherController::activeAccountId() const
    {
        const auto active = m_accountManager.activeAccount();
        return active ? active->id : QString();
    }

    QString LauncherController::microsoftClientId() const
    {
        return m_launcherSettings.azureClientId();
    }

    QString LauncherController::microsoftUserCode() const
    {
        return m_microsoftUserCode;
    }

    QString LauncherController::microsoftVerificationUrl() const
    {
        return m_microsoftVerificationUrl;
    }

    bool LauncherController::microsoftAuthInProgress() const
    {
        return m_microsoftAuthInProgress;
    }

    QString LauncherController::consoleText() const
    {
        return m_consoleText;
    }

    QString LauncherController::consoleInstanceName() const
    {
        for (const strata::Instance& instance : m_instanceRecords)
        {
            if (instance.id == m_consoleInstanceId)
            {
                return instance.name;
            }
        }
        return {};
    }

    QVariantList LauncherController::installedRuntimes() const
    {
        return m_installedRuntimes;
    }

    QString LauncherController::runtimeVendor() const
    {
        return m_runtimeVendor;
    }

    bool LauncherController::redundantRuntimes() const
    {
        return m_redundantRuntimes;
    }

    QStringList LauncherController::globalJavaArguments() const
    {
        return m_launcherSettings.globalJavaLaunchFlags();
    }

    QStringList LauncherController::recommendedJavaArguments() const
    {
        return strata::settings::LauncherSettings::recommendedJavaFlags();
    }

    bool LauncherController::installingRuntime() const
    {
        return m_installingRuntime;
    }

    bool LauncherController::deletingInstance() const
    {
        return m_deletingInstance;
    }

    double LauncherController::runtimeInstallationProgress() const
    {
        return m_runtimeInstallTotal == 0
                   ? 0.0
                   : static_cast<double>(m_runtimeInstallCompleted) / m_runtimeInstallTotal;
    }

    QString LauncherController::runtimeInstallationItem() const
    {
        return m_runtimeInstallationItem;
    }

    QVariantList LauncherController::servers() const
    {
        return m_servers;
    }

    bool LauncherController::serverBusy() const
    {
        return m_serverBusy;
    }

    QString LauncherController::serverStatus() const
    {
        return m_serverStatus;
    }

    QString LauncherController::serverProperties() const
    {
        return m_serverProperties;
    }

    double LauncherController::serverProgress() const
    {
        return m_serverProgress;
    }

    QString LauncherController::serverInstallItem() const
    {
        return m_serverInstallItem;
    }

    QString LauncherController::serverConsoleText() const
    {
        return m_serverConsoleText;
    }

    QString LauncherController::serverProcessState() const
    {
        return m_serverProcessState;
    }

    QString LauncherController::serverResourceStatus() const
    {
        return m_serverResourceStatus;
    }

    QVariantList LauncherController::serverContentProjects() const
    {
        return m_serverContentProjects;
    }

    QVariantList LauncherController::serverContentVersions() const
    {
        return m_serverContentVersions;
    }

    QString LauncherController::serverContentStatus() const
    {
        return m_serverContentStatus;
    }

    bool LauncherController::serverContentBusy() const
    {
        return m_serverContentBusy;
    }

    void LauncherController::publishInstances()
    {
        m_instances = toVariantInstances(m_instanceRecords, m_processStates, m_runtimeModes,
                                         m_runtimeComponents);
        emit instancesChanged();
        emit lastLaunchedChanged();
    }

    void
    LauncherController::publishRuntimes(const QList<strata::runtime::InstalledRuntime>& runtimes)
    {
        m_installedRuntimes.clear();
        for (const auto& runtime : runtimes)
        {
            QString vendorLabel;
            switch (runtime.vendor)
            {
            case strata::runtime::RuntimeVendor::Mojang:
                vendorLabel = QStringLiteral("Mojang");
                break;
            case strata::runtime::RuntimeVendor::Temurin:
                vendorLabel = QStringLiteral("Eclipse Temurin");
                break;
            case strata::runtime::RuntimeVendor::Zulu:
                vendorLabel = QStringLiteral("Azul Zulu");
                break;
            }
            m_installedRuntimes.append(
                QVariantMap{{QStringLiteral("directory"), runtime.directory},
                            {QStringLiteral("component"), runtime.component},
                            {QStringLiteral("vendor"), vendorLabel},
                            {QStringLiteral("version"), runtime.version},
                            {QStringLiteral("users"), runtime.users},
                            {QStringLiteral("inUse"), !runtime.users.isEmpty()}});
        }
        emit runtimesChanged();
    }

    void LauncherController::updateProcessState(const QString& instanceId, const QString& state)
    {
        m_processStates.insert(instanceId, state);
        publishInstances();
        emit processStateChanged(instanceId, state);
    }

    void LauncherController::refresh()
    {
        m_initializing = true;
        m_initializationTasks = 2;
        emit initializationChanged();
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self]()
            {
                if (self.isNull())
                {
                    return;
                }
                try
                {
                    const auto manifest = self->m_manifestClient.fetchManifest();
                    QMetaObject::invokeMethod(
                        self,
                        [self, manifest]()
                        {
                            if (self.isNull())
                            {
                                return;
                            }
                            self->m_manifest = manifest;
                            self->m_versions.clear();
                            self->m_versions.reserve(manifest.versions.size());
                            for (const auto& version : manifest.versions)
                            {
                                self->m_versions.append(
                                    QVariantMap{{QStringLiteral("id"), version.id},
                                                {QStringLiteral("type"), version.type},
                                                {QStringLiteral("label"), version.displayName()}});
                            }
                            emit self->versionsChanged();
                            if (manifest.usedCache)
                            {
                                self->m_errorMessage = QStringLiteral(
                                    "Using cached Minecraft versions while offline.");
                                emit self->errorMessageChanged();
                            }
                            self->finishInitializationTask();
                        },
                        Qt::QueuedConnection);
                }
                catch (...)
                {
                    const QString message = exceptionMessage();
                    QMetaObject::invokeMethod(
                        self,
                        [self, message]()
                        {
                            if (self.isNull())
                            {
                                return;
                            }
                            self->m_errorMessage = message;
                            emit self->errorMessageChanged();
                            self->finishInitializationTask();
                        },
                        Qt::QueuedConnection);
                }
            }));

        m_workers.start(QRunnable::create(
            [self]()
            {
                if (self.isNull())
                {
                    return;
                }
                try
                {
                    QList<strata::Instance> instances;
                    {
                        QMutexLocker lock(&self->m_instanceMutex);
                        instances = self->m_instanceManager.listInstances();
                    }
                    QHash<QString, QString> runtimeModes;
                    QHash<QString, QString> runtimeComponents;
                    QList<strata::runtime::RuntimeOwner> runtimeOwners;
                    readRuntimeOwners(instances, runtimeModes, runtimeComponents, runtimeOwners,
                                      &self->m_serverManager);
                    QMetaObject::invokeMethod(
                        self,
                        [self, instances, runtimeModes, runtimeComponents, runtimeOwners]()
                        {
                            if (self.isNull())
                            {
                                return;
                            }
                            self->m_instanceRecords = instances;
                            self->m_runtimeModes = runtimeModes;
                            self->m_runtimeComponents = runtimeComponents;
                            self->m_runtimeOwners = runtimeOwners;
                            for (const strata::Instance& instance : instances)
                            {
                                if (!self->m_processStates.contains(instance.id))
                                {
                                    self->m_processStates.insert(instance.id,
                                                                 QStringLiteral("stopped"));
                                }
                            }
                            self->publishInstances();
                            self->refreshRuntimes();
                            self->finishInitializationTask();
                        },
                        Qt::QueuedConnection);
                }
                catch (...)
                {
                    const QString message = exceptionMessage();
                    QMetaObject::invokeMethod(
                        self,
                        [self, message]()
                        {
                            if (self.isNull())
                            {
                                return;
                            }
                            self->m_errorMessage = message;
                            emit self->errorMessageChanged();
                            self->finishInitializationTask();
                        },
                        Qt::QueuedConnection);
                }
            }));
    }

    void LauncherController::finishInitializationTask()
    {
        if (m_initializationTasks <= 0)
        {
            return;
        }
        --m_initializationTasks;
        if (m_initializationTasks == 0)
        {
            m_initializing = false;
            emit initializationChanged();
        }
    }

    void LauncherController::loadLoaderVersions(const QString& loaderType,
                                                const QString& minecraftVersion)
    {
        const quint64 requestId = ++m_loaderVersionRequestId;
        m_loaderVersions.clear();
        m_loaderVersionsError.clear();
        m_loadingLoaderVersions = false;
        strata::loader::LoaderType type;
        try
        {
            type = strata::loader::LoaderSelection::parseType(loaderType);
        }
        catch (...)
        {
            m_loaderVersionsError = exceptionMessage();
            emit loaderVersionsChanged();
            return;
        }
        if (type == strata::loader::LoaderType::Vanilla)
        {
            emit loaderVersionsChanged();
            return;
        }
        m_loadingLoaderVersions = true;
        emit loaderVersionsChanged();

        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, requestId, type, minecraftVersion]()
            {
                if (self.isNull())
                {
                    return;
                }
                QStringList versions;
                QString error;
                try
                {
                    versions = self->m_loaderInstaller.availableVersions(type, minecraftVersion);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, requestId, versions, error]()
                    {
                        if (self.isNull() || self->m_loaderVersionRequestId != requestId)
                        {
                            return;
                        }
                        self->m_loaderVersions = versions;
                        self->m_loaderVersionsError = error;
                        self->m_loadingLoaderVersions = false;
                        emit self->loaderVersionsChanged();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::installInstance(const QString& name, const QString& versionId)
    {
        installInstanceWithLoader(name, versionId, QStringLiteral("VANILLA"), {});
    }

    void LauncherController::installInstanceWithLoader(const QString& name,
                                                       const QString& versionId,
                                                       const QString& loaderType,
                                                       const QString& loaderVersion)
    {
        const QString normalizedName = name.trimmed();
        if (normalizedName.isEmpty())
        {
            m_errorMessage = QStringLiteral("Instance name must not be blank.");
            emit errorMessageChanged();
            emit installFinished(false, normalizedName, m_errorMessage);
            return;
        }
        if (m_installing)
        {
            return;
        }
        strata::loader::LoaderSelection loaderSelection;
        try
        {
            loaderSelection.type = strata::loader::LoaderSelection::parseType(loaderType);
            loaderSelection.version = loaderVersion.trimmed();
            if (loaderSelection.type == strata::loader::LoaderType::Vanilla)
            {
                loaderSelection.version.clear();
            }
            else if (loaderSelection.version.isEmpty())
            {
                throw std::invalid_argument("Choose a loader version before installing.");
            }
        }
        catch (...)
        {
            m_errorMessage = exceptionMessage();
            emit errorMessageChanged();
            emit installFinished(false, normalizedName, m_errorMessage);
            return;
        }
        const auto version =
            std::find_if(m_manifest.versions.cbegin(), m_manifest.versions.cend(),
                         [&versionId](const auto& candidate) { return candidate.id == versionId; });
        if (version == m_manifest.versions.cend())
        {
            m_errorMessage =
                QStringLiteral("Choose a Minecraft version from the loaded version list.");
            emit errorMessageChanged();
            emit installFinished(false, normalizedName, m_errorMessage);
            return;
        }

        const strata::mojang::Version selectedVersion = *version;
        m_installing = true;
        m_currentInstallName = normalizedName;
        m_installCompleted = 0;
        m_installTotal = 2;
        m_currentInstallItem = QStringLiteral("Preparing Minecraft %1").arg(versionId);
        m_errorMessage.clear();
        emit installationChanged();
        emit errorMessageChanged();

        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, normalizedName, selectedVersion, loaderSelection]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                strata::Instance instance;
                bool created = false;
                try
                {
                    {
                        QMutexLocker lock(&self->m_instanceMutex);
                        instance =
                            self->m_instanceManager.create(normalizedName, selectedVersion.id);
                    }
                    created = true;
                    self->m_loaderInstaller.prepare(instance, loaderSelection);
                    const auto publishProgress = [self](const strata::download::Progress& progress)
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        QMetaObject::invokeMethod(
                            self,
                            [self, progress]()
                            {
                                if (self.isNull())
                                {
                                    return;
                                }
                                self->m_installCompleted = progress.completed;
                                self->m_installTotal = progress.total;
                                self->m_currentInstallItem = progress.currentItem;
                                emit self->installationChanged();
                            },
                            Qt::QueuedConnection);
                    };
                    self->m_installer.install(instance, selectedVersion, publishProgress);
                    self->m_loaderInstaller.install(instance, loaderSelection, publishProgress);
                }
                catch (...)
                {
                    error = exceptionMessage();
                    if (created)
                    {
                        try
                        {
                            QMutexLocker lock(&self->m_instanceMutex);
                            self->m_instanceManager.deleteInstance(instance);
                        }
                        catch (...)
                        {
                            error += QStringLiteral("; cleanup failed: ") + exceptionMessage();
                        }
                    }
                }

                QList<strata::Instance> updatedInstances;
                QHash<QString, QString> runtimeModes;
                QHash<QString, QString> runtimeComponents;
                QList<strata::runtime::RuntimeOwner> runtimeOwners;
                try
                {
                    {
                        QMutexLocker lock(&self->m_instanceMutex);
                        updatedInstances = self->m_instanceManager.listInstances();
                    }
                    if (error.isEmpty())
                    {
                        try
                        {
                            readRuntimeOwners(updatedInstances, runtimeModes, runtimeComponents,
                                              runtimeOwners, &self->m_serverManager);
                        }
                        catch (...)
                        {
                            error = QStringLiteral("Unable to read instance runtime settings: ") +
                                    exceptionMessage();
                        }
                    }
                }
                catch (...)
                {
                    if (error.isEmpty())
                    {
                        error = exceptionMessage();
                    }
                    else
                    {
                        error +=
                            QStringLiteral("; unable to refresh instances: ") + exceptionMessage();
                    }
                }
                const bool succeeded = error.isEmpty();
                QMetaObject::invokeMethod(
                    self,
                    [self, succeeded, normalizedName, error, updatedInstances, runtimeModes,
                     runtimeComponents, runtimeOwners]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_installing = false;
                        self->m_errorMessage = error;
                        self->m_instanceRecords = updatedInstances;
                        self->m_runtimeModes = runtimeModes;
                        self->m_runtimeComponents = runtimeComponents;
                        self->m_runtimeOwners = runtimeOwners;
                        for (const strata::Instance& instance : updatedInstances)
                        {
                            if (!self->m_processStates.contains(instance.id))
                            {
                                self->m_processStates.insert(instance.id,
                                                             QStringLiteral("stopped"));
                            }
                        }
                        self->m_currentInstallItem = succeeded
                                                         ? QStringLiteral("Installation complete")
                                                         : QStringLiteral("Installation failed");
                        self->publishInstances();
                        self->refreshRuntimes();
                        if (succeeded)
                        {
                            self->m_installCompleted = self->m_installTotal;
                        }
                        emit self->installationChanged();
                        emit self->errorMessageChanged();
                        emit self->installFinished(succeeded, normalizedName, error);
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::saveOfflineUsername(const QString& name)
    {
        QString normalized;
        try
        {
            normalized = strata::auth::OfflineAccountStore::validateUsername(name);
        }
        catch (const std::exception& exception)
        {
            m_errorMessage = QString::fromUtf8(exception.what());
            emit errorMessageChanged();
            return;
        }
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, normalized]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                try
                {
                    self->m_offlineAccountStore.setUsername(normalized);
                    const auto account = self->m_accountManager.addOffline(normalized);
                    self->m_accountManager.select(account.id);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, normalized, error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_errorMessage = error;
                        if (error.isEmpty())
                        {
                            self->m_offlineUsername = normalized;
                            emit self->offlineAccountChanged();
                            emit self->accountsChanged();
                        }
                        emit self->errorMessageChanged();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::addOfflineAccount(const QString& name)
    {
        saveOfflineUsername(name);
    }

    void LauncherController::selectAccount(const QString& accountId)
    {
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, accountId]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                std::optional<strata::auth::Account> active;
                try
                {
                    self->m_accountManager.select(accountId);
                    active = self->m_accountManager.activeAccount();
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, active, error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        if (error.isEmpty())
                        {
                            self->m_offlineUsername =
                                active && active->type == strata::auth::Account::Type::Offline
                                    ? active->name
                                    : QString();
                            emit self->accountsChanged();
                            emit self->offlineAccountChanged();
                        }
                        else
                        {
                            self->m_errorMessage = error;
                            emit self->errorMessageChanged();
                        }
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::removeAccount(const QString& accountId)
    {
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, accountId]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                std::optional<strata::auth::Account> active;
                try
                {
                    self->m_accountManager.remove(accountId);
                    active = self->m_accountManager.activeAccount();
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, active, error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        if (error.isEmpty())
                        {
                            self->m_offlineUsername =
                                active && active->type == strata::auth::Account::Type::Offline
                                    ? active->name
                                    : QString();
                            emit self->accountsChanged();
                            emit self->offlineAccountChanged();
                        }
                        else
                        {
                            self->m_errorMessage = error;
                            emit self->errorMessageChanged();
                        }
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::setMicrosoftClientId(const QString& clientId)
    {
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, clientId]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                try
                {
                    self->m_launcherSettings.setAzureClientId(clientId);
                    self->m_accountManager.setClientId(clientId);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        if (error.isEmpty())
                        {
                            emit self->accountsChanged();
                        }
                        else
                        {
                            self->m_errorMessage = error;
                            emit self->errorMessageChanged();
                        }
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::signInMicrosoft(const QString& clientId)
    {
        if (m_microsoftAuthInProgress)
        {
            return;
        }
        m_microsoftAuthInProgress = true;
        m_microsoftUserCode.clear();
        m_microsoftVerificationUrl.clear();
        emit microsoftAuthChanged();
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, clientId]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                try
                {
                    self->m_accountManager.setClientId(clientId);
                    const auto profile = self->m_microsoftAuth.signIn(clientId);
                    self->m_accountManager.addMicrosoft(profile);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_microsoftAuthInProgress = false;
                        if (error.isEmpty())
                        {
                            self->m_offlineUsername.clear();
                            emit self->accountsChanged();
                            emit self->offlineAccountChanged();
                        }
                        else if (error != QStringLiteral("Microsoft sign-in was cancelled"))
                        {
                            self->m_errorMessage = error;
                            emit self->errorMessageChanged();
                        }
                        emit self->microsoftAuthChanged();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::cancelMicrosoftSignIn()
    {
        if (m_microsoftAuthInProgress)
        {
            m_microsoftAuth.cancel();
        }
    }

    void LauncherController::launchInstance(const QString& instanceId)
    {
        const auto existing = m_processes.value(instanceId);
        if (existing && existing->isRunning())
        {
            m_errorMessage = QStringLiteral("This Minecraft instance is already running.");
            emit errorMessageChanged();
            return;
        }
        if (m_trackInstanceLaunches)
        {
            const bool historyChanged = m_recentLaunchedInstanceIds.isEmpty() ||
                                        m_recentLaunchedInstanceIds.constFirst() != instanceId;
            m_recentLaunchedInstanceIds.removeAll(instanceId);
            m_recentLaunchedInstanceIds.prepend(instanceId);
            while (m_recentLaunchedInstanceIds.size() > 2)
            {
                m_recentLaunchedInstanceIds.removeLast();
            }
            m_lastLaunchedInstanceId = instanceId;
            if (historyChanged)
            {
                emit lastLaunchedChanged();
            }
            QPointer<LauncherController> settingsSelf(this);
            m_workers.start(QRunnable::create(
                [settingsSelf, instanceId]()
                {
                    if (settingsSelf.isNull())
                    {
                        return;
                    }
                    try
                    {
                        settingsSelf->m_launcherSettings.recordInstanceLaunch(instanceId);
                    }
                    catch (const std::exception& exception)
                    {
                        const QString error = QString::fromUtf8(exception.what());
                        QMetaObject::invokeMethod(
                            settingsSelf,
                            [settingsSelf, error]()
                            {
                                if (!settingsSelf.isNull())
                                {
                                    settingsSelf->m_errorMessage = error;
                                    emit settingsSelf->errorMessageChanged();
                                }
                            },
                            Qt::QueuedConnection);
                    }
                }));
        }
        updateProcessState(instanceId, QStringLiteral("starting"));

        const QStringList globalJavaArguments = m_launcherSettings.globalJavaLaunchFlags();
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, instanceId, globalJavaArguments]()
            {
                if (self.isNull())
                {
                    return;
                }

                QString error;
                strata::Instance instance;
                strata::launch::LaunchPlan plan;
                strata::launch::LaunchAccount account;
                bool found = false;
                try
                {
                    {
                        QMutexLocker lock(&self->m_instanceMutex);
                        for (const strata::Instance& candidate :
                             self->m_instanceManager.listInstances())
                        {
                            if (candidate.id == instanceId)
                            {
                                instance = candidate;
                                found = true;
                                break;
                            }
                        }
                    }
                    if (!found)
                    {
                        throw std::runtime_error("Minecraft instance no longer exists");
                    }
                    const auto launchProfile =
                        self->m_accountManager.activeLaunchProfile(self->m_microsoftAuth);
                    account = {launchProfile.account.name, launchProfile.account.uuid,
                               launchProfile.accessToken, launchProfile.account.xuid,
                               launchProfile.account.type};
                    const strata::InstanceSettings settings(instance.directory);
                    QString javaExecutable;
                    if (settings.javaRuntimeMode() ==
                        strata::InstanceSettings::JavaRuntimeMode::Managed)
                    {
                        javaExecutable = self->m_runtimeManager.javaExecutable(instance);
                    }
                    plan = strata::launch::GameLauncher::buildPlan(
                        instance, account, self->m_instanceManager.dataDirectory(), javaExecutable,
                        globalJavaArguments);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, instanceId, plan, error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        if (!error.isEmpty())
                        {
                            self->updateProcessState(instanceId, QStringLiteral("crashed"));
                            self->m_errorMessage = error;
                            emit self->errorMessageChanged();
                            return;
                        }
                        auto* process = new strata::launch::GameProcess(instanceId, plan, self);
                        self->m_processes.insert(instanceId, process);
                        connect(process, &strata::launch::GameProcess::statusChanged, self,
                                [self](const QString& id, const QString& state)
                                {
                                    if (!self.isNull())
                                    {
                                        self->updateProcessState(id, state);
                                    }
                                });
                        connect(process, &strata::launch::GameProcess::outputReceived, self,
                                [self](const QString& id, const QString& text)
                                {
                                    if (self.isNull())
                                    {
                                        return;
                                    }
                                    QString& output = self->m_processOutput[id];
                                    output.append(text);
                                    constexpr qsizetype maxConsoleCharacters = 200'000;
                                    if (output.size() > maxConsoleCharacters)
                                    {
                                        output.remove(0, output.size() - maxConsoleCharacters);
                                    }
                                    if (self->m_consoleInstanceId == id)
                                    {
                                        self->m_consoleText = output;
                                        emit self->consoleTextChanged();
                                    }
                                });
                        connect(process, &strata::launch::GameProcess::logOutputReceived, self,
                                [self](const QString& logFile, const QByteArray& output)
                                {
                                    if (self.isNull())
                                    {
                                        return;
                                    }
                                    self->m_logWorkers.start(QRunnable::create(
                                        [logFile, output]()
                                        {
                                            const QString parentDirectory =
                                                QFileInfo(logFile).absolutePath();
                                            if (!QDir().mkpath(parentDirectory))
                                            {
                                                qWarning().noquote()
                                                    << "Unable to create Minecraft log directory:"
                                                    << parentDirectory;
                                                return;
                                            }
                                            QFile log(logFile);
                                            if (!log.open(QIODevice::WriteOnly | QIODevice::Append))
                                            {
                                                qWarning().noquote()
                                                    << "Unable to append Minecraft output:"
                                                    << log.errorString();
                                                return;
                                            }
                                            if (log.write(output) != output.size())
                                            {
                                                qWarning().noquote()
                                                    << "Unable to write Minecraft output:"
                                                    << log.errorString();
                                            }
                                        }));
                                });
                        connect(process, &strata::launch::GameProcess::finished, self,
                                [self](const QString& id, int, bool crashed)
                                {
                                    if (!self.isNull() && crashed)
                                    {
                                        self->m_errorMessage =
                                            QStringLiteral("Minecraft exited unexpectedly.");
                                        emit self->errorMessageChanged();
                                    }
                                });
                        process->start();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::setTrackInstanceLaunches(bool track)
    {
        if (m_trackInstanceLaunches == track)
        {
            return;
        }
        m_trackInstanceLaunches = track;
        emit launchTrackingChanged();
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, track]()
            {
                if (self.isNull())
                {
                    return;
                }
                try
                {
                    self->m_launcherSettings.setTrackInstanceLaunches(track);
                }
                catch (const std::exception& exception)
                {
                    const QString error = QString::fromUtf8(exception.what());
                    QMetaObject::invokeMethod(
                        self,
                        [self, error]()
                        {
                            if (!self.isNull())
                            {
                                self->m_errorMessage = error;
                                emit self->errorMessageChanged();
                            }
                        },
                        Qt::QueuedConnection);
                }
            }));
    }

    void LauncherController::deleteInstance(const QString& instanceId)
    {
        if (m_deletingInstance || m_installing)
        {
            m_errorMessage = QStringLiteral("Wait for the current instance operation to finish.");
            emit errorMessageChanged();
            return;
        }
        const auto process = m_processes.value(instanceId);
        if (process && process->isRunning())
        {
            m_errorMessage = QStringLiteral("Stop the instance before deleting it.");
            emit errorMessageChanged();
            return;
        }
        const auto instance = std::find_if(m_instanceRecords.cbegin(), m_instanceRecords.cend(),
                                           [&instanceId](const strata::Instance& value)
                                           { return value.id == instanceId; });
        if (instance == m_instanceRecords.cend())
        {
            m_errorMessage = QStringLiteral("The selected instance no longer exists.");
            emit errorMessageChanged();
            return;
        }
        const strata::Instance selected = *instance;
        m_deletingInstance = true;
        emit deletingInstanceChanged();
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, selected]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                QList<strata::Instance> instances;
                QHash<QString, QString> runtimeModes;
                QHash<QString, QString> runtimeComponents;
                QList<strata::runtime::RuntimeOwner> owners;
                QList<strata::runtime::InstalledRuntime> runtimes;
                try
                {
                    {
                        QMutexLocker lock(&self->m_instanceMutex);
                        self->m_instanceManager.deleteInstance(selected);
                        instances = self->m_instanceManager.listInstances();
                    }
                    readRuntimeOwners(instances, runtimeModes, runtimeComponents, owners,
                                      &self->m_serverManager);
                    runtimes = self->m_runtimeManager.listInstalledRuntimes(owners);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, selected, instances, runtimeModes, runtimeComponents, owners, runtimes,
                     error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_deletingInstance = false;
                        self->m_errorMessage = error;
                        if (error.isEmpty())
                        {
                            self->m_processStates.remove(selected.id);
                            self->m_processes.remove(selected.id);
                            if (self->m_consoleInstanceId == selected.id)
                            {
                                self->closeConsole();
                            }
                            self->m_instanceRecords = instances;
                            self->m_runtimeModes = runtimeModes;
                            self->m_runtimeComponents = runtimeComponents;
                            self->m_runtimeOwners = owners;
                            self->publishInstances();
                            self->publishRuntimes(runtimes);
                        }
                        emit self->deletingInstanceChanged();
                        emit self->errorMessageChanged();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::stopInstance(const QString& instanceId)
    {
        const auto process = m_processes.value(instanceId);
        if (process)
        {
            process->stop();
        }
    }

    void LauncherController::stopConsoleInstance()
    {
        stopInstance(m_consoleInstanceId);
    }

    void LauncherController::openConsole(const QString& instanceId)
    {
        m_consoleInstanceId = instanceId;
        m_consoleText = m_processOutput.value(instanceId);
        emit consoleInstanceChanged();
        emit consoleTextChanged();
    }

    void LauncherController::closeConsole()
    {
        m_consoleInstanceId.clear();
        m_consoleText.clear();
        emit consoleInstanceChanged();
        emit consoleTextChanged();
    }

    void LauncherController::setRuntimePreferences(const QString& vendor, bool redundant)
    {
        if (m_installingRuntime)
        {
            return;
        }
        strata::runtime::RuntimeVendor parsedVendor;
        try
        {
            parsedVendor = strata::runtime::JavaRuntimeManager::parseVendor(vendor);
        }
        catch (const std::exception& exception)
        {
            m_errorMessage = QString::fromUtf8(exception.what());
            emit errorMessageChanged();
            return;
        }
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, parsedVendor, redundant, normalizedVendor = vendor.toUpper()]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                try
                {
                    self->m_launcherSettings.setRuntimePreferences(normalizedVendor, redundant);
                    self->m_runtimeManager.setPreferences({parsedVendor, redundant});
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, parsedVendor, redundant, error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_errorMessage = error;
                        if (error.isEmpty())
                        {
                            self->m_runtimeVendor =
                                strata::runtime::JavaRuntimeManager::vendorName(parsedVendor)
                                    .toUpper();
                            self->m_redundantRuntimes = redundant;
                            emit self->runtimePreferencesChanged();
                        }
                        emit self->errorMessageChanged();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::setGlobalJavaArguments(const QString& flags)
    {
        const QStringList parsedFlags = flags.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, parsedFlags]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                try
                {
                    self->m_launcherSettings.setGlobalJavaLaunchFlags(parsedFlags);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        if (error.isEmpty())
                        {
                            emit self->globalJavaArgumentsChanged();
                        }
                        else
                        {
                            self->m_errorMessage = error;
                            emit self->errorMessageChanged();
                        }
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::installInstanceRuntime(const QString& instanceId)
    {
        if (m_installingRuntime)
        {
            return;
        }
        const auto vendor = strata::runtime::JavaRuntimeManager::parseVendor(m_runtimeVendor);
        const bool redundant = m_redundantRuntimes;
        m_installingRuntime = true;
        m_runtimeInstallCompleted = 0;
        m_runtimeInstallTotal = 1;
        m_runtimeInstallationItem = QStringLiteral("Preparing managed Java runtime");
        m_errorMessage.clear();
        emit runtimeInstallationChanged();
        emit errorMessageChanged();

        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, instanceId, vendor, redundant]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                QList<strata::Instance> instances;
                QHash<QString, QString> runtimeModes;
                QHash<QString, QString> runtimeComponents;
                QList<strata::runtime::RuntimeOwner> runtimeOwners;
                QList<strata::runtime::InstalledRuntime> installed;
                QString instanceName;
                try
                {
                    strata::Instance instance;
                    bool found = false;
                    {
                        QMutexLocker lock(&self->m_instanceMutex);
                        for (const strata::Instance& candidate :
                             self->m_instanceManager.listInstances())
                        {
                            if (candidate.id == instanceId)
                            {
                                instance = candidate;
                                found = true;
                                break;
                            }
                        }
                    }
                    if (!found)
                    {
                        throw std::runtime_error("Minecraft instance no longer exists");
                    }
                    instanceName = instance.name;
                    const QString metadataPath =
                        QDir(instance.gameDirectory())
                            .filePath(
                                QStringLiteral("versions/%1/%1.json").arg(instance.versionId));
                    QFile metadataFile(metadataPath);
                    if (!metadataFile.open(QIODevice::ReadOnly) ||
                        metadataFile.size() > 32 * 1024 * 1024)
                    {
                        throw std::runtime_error(
                            "Installed Minecraft version metadata is missing or too large");
                    }
                    QJsonParseError parseError;
                    const QJsonDocument metadataDocument =
                        QJsonDocument::fromJson(metadataFile.readAll(), &parseError);
                    if (parseError.error != QJsonParseError::NoError ||
                        !metadataDocument.isObject())
                    {
                        throw std::runtime_error("Installed Minecraft version metadata is invalid");
                    }
                    const QString component = self->m_runtimeManager.install(
                        instance.directory, instance.versionId, metadataDocument.object(), vendor,
                        redundant,
                        [self](const auto& progress)
                        {
                            if (self.isNull())
                            {
                                return;
                            }
                            QMetaObject::invokeMethod(
                                self,
                                [self, progress]()
                                {
                                    if (self.isNull())
                                    {
                                        return;
                                    }
                                    self->m_runtimeInstallCompleted = progress.completed;
                                    self->m_runtimeInstallTotal = progress.total;
                                    self->m_runtimeInstallationItem = progress.currentItem;
                                    emit self->runtimeInstallationChanged();
                                },
                                Qt::QueuedConnection);
                        });
                    strata::InstanceSettings settings(instance.directory);
                    settings.setJavaRuntimeComponent(component);
                    settings.setJavaRuntimeMode(strata::InstanceSettings::JavaRuntimeMode::Managed);
                    settings.setJavaRuntimeRedundant(redundant);
                    {
                        QMutexLocker lock(&self->m_instanceMutex);
                        instances = self->m_instanceManager.listInstances();
                    }
                    readRuntimeOwners(instances, runtimeModes, runtimeComponents, runtimeOwners,
                                      &self->m_serverManager);
                    installed = self->m_runtimeManager.listInstalledRuntimes(runtimeOwners);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, error, instances, runtimeModes, runtimeComponents, runtimeOwners,
                     installed, instanceName]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_installingRuntime = false;
                        self->m_errorMessage = error;
                        self->m_runtimeInstallationItem =
                            error.isEmpty()
                                ? QStringLiteral("Managed Java runtime installed for %1")
                                      .arg(instanceName)
                                : QStringLiteral("Java runtime installation failed");
                        if (error.isEmpty())
                        {
                            self->m_instanceRecords = instances;
                            self->m_runtimeModes = runtimeModes;
                            self->m_runtimeComponents = runtimeComponents;
                            self->m_runtimeOwners = runtimeOwners;
                            self->publishInstances();
                            self->publishRuntimes(installed);
                            self->m_runtimeInstallCompleted = self->m_runtimeInstallTotal;
                        }
                        emit self->runtimeInstallationChanged();
                        emit self->errorMessageChanged();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::useSystemJava(const QString& instanceId)
    {
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, instanceId]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                QList<strata::Instance> instances;
                QHash<QString, QString> runtimeModes;
                QHash<QString, QString> runtimeComponents;
                QList<strata::runtime::RuntimeOwner> runtimeOwners;
                QList<strata::runtime::InstalledRuntime> installed;
                try
                {
                    strata::Instance selected;
                    bool found = false;
                    {
                        QMutexLocker lock(&self->m_instanceMutex);
                        for (const strata::Instance& instance :
                             self->m_instanceManager.listInstances())
                        {
                            if (instance.id == instanceId)
                            {
                                selected = instance;
                                found = true;
                                break;
                            }
                        }
                    }
                    if (!found)
                    {
                        throw std::runtime_error("Minecraft instance no longer exists");
                    }
                    strata::InstanceSettings(selected.directory)
                        .setJavaRuntimeMode(strata::InstanceSettings::JavaRuntimeMode::System);
                    {
                        QMutexLocker lock(&self->m_instanceMutex);
                        instances = self->m_instanceManager.listInstances();
                    }
                    readRuntimeOwners(instances, runtimeModes, runtimeComponents, runtimeOwners,
                                      &self->m_serverManager);
                    installed = self->m_runtimeManager.listInstalledRuntimes(runtimeOwners);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, error, instances, runtimeModes, runtimeComponents, runtimeOwners,
                     installed]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_errorMessage = error;
                        if (error.isEmpty())
                        {
                            self->m_instanceRecords = instances;
                            self->m_runtimeModes = runtimeModes;
                            self->m_runtimeComponents = runtimeComponents;
                            self->m_runtimeOwners = runtimeOwners;
                            self->publishInstances();
                            self->publishRuntimes(installed);
                        }
                        emit self->errorMessageChanged();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::refreshRuntimes()
    {
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self]()
            {
                if (self.isNull())
                {
                    return;
                }
                QList<strata::Instance> instances;
                QHash<QString, QString> runtimeModes;
                QHash<QString, QString> runtimeComponents;
                QList<strata::runtime::RuntimeOwner> owners;
                QList<strata::runtime::InstalledRuntime> runtimes;
                QString error;
                try
                {
                    {
                        QMutexLocker lock(&self->m_instanceMutex);
                        instances = self->m_instanceManager.listInstances();
                    }
                    readRuntimeOwners(instances, runtimeModes, runtimeComponents, owners,
                                      &self->m_serverManager);
                    runtimes = self->m_runtimeManager.listInstalledRuntimes(owners);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, instances, runtimeModes, runtimeComponents, owners, runtimes, error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        if (error.isEmpty())
                        {
                            self->m_instanceRecords = instances;
                            self->m_runtimeModes = runtimeModes;
                            self->m_runtimeComponents = runtimeComponents;
                            self->m_runtimeOwners = owners;
                            self->publishInstances();
                            self->publishRuntimes(runtimes);
                        }
                        else
                        {
                            self->m_errorMessage = error;
                            emit self->errorMessageChanged();
                        }
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::deleteRuntime(const QString& directory)
    {
        const auto owners = m_runtimeOwners;
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, owners, directory]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                QList<strata::runtime::InstalledRuntime> runtimes;
                try
                {
                    runtimes = self->m_runtimeManager.listInstalledRuntimes(owners);
                    const auto runtime =
                        std::find_if(runtimes.cbegin(), runtimes.cend(),
                                     [&directory](const auto& candidate)
                                     {
                                         return QDir::cleanPath(candidate.directory) ==
                                                QDir::cleanPath(directory);
                                     });
                    if (runtime == runtimes.cend())
                    {
                        throw std::runtime_error("Selected managed Java runtime no longer exists");
                    }
                    self->m_runtimeManager.deleteRuntime(*runtime, owners);
                    runtimes = self->m_runtimeManager.listInstalledRuntimes(owners);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, runtimes, error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_errorMessage = error;
                        if (error.isEmpty())
                        {
                            self->publishRuntimes(runtimes);
                        }
                        emit self->errorMessageChanged();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::refreshServers()
    {
        runServerOperation(
            QStringLiteral("Loading servers…"),
            [this]
            {
                QVariantList result;
                for (const auto& server : m_serverManager.listServers())
                {
                    result.append(serverVariant(server));
                }
                return result;
            },
            [](LauncherController& self, const QVariant& result)
            {
                self.publishServers(result.toList());
                self.m_serverProperties.clear();
            });
    }

    void LauncherController::createServer(const QString& name, const QString& version,
                                          const QString& distribution)
    {
        runServerOperation(
            QStringLiteral("Creating server…"),
            [this, name, version, distribution]
            {
                const strata::server::ServerConfig created =
                    m_serverManager.create(name, version, serverDistribution(distribution));
                QVariantList result;
                for (const auto& server : m_serverManager.listServers())
                {
                    result.append(serverVariant(server));
                }
                return QVariantMap{{QStringLiteral("servers"), result},
                                   {QStringLiteral("createdId"), created.id}};
            },
            [](LauncherController& self, const QVariant& result)
            {
                const QVariantMap values = result.toMap();
                self.publishServers(values.value(QStringLiteral("servers")).toList());
                self.m_serverProperties.clear();
                const QString createdId = values.value(QStringLiteral("createdId")).toString();
                if (!createdId.isEmpty())
                {
                    self.selectServer(createdId);
                    self.runServerInstall(createdId, false);
                }
            });
    }

    void LauncherController::updateServerSettings(const QString& id, const QString& name,
                                                  bool eulaAccepted, const QString& javaFlags,
                                                  int viewDistance, bool onlineMode,
                                                  bool useManagedJavaRuntime)
    {
        runServerOperation(
            QStringLiteral("Saving server settings…"),
            [this, id, name, eulaAccepted, javaFlags, viewDistance, onlineMode,
             useManagedJavaRuntime]
            {
                strata::server::ServerConfig selected;
                selected.id = id;
                const QStringList flags = javaFlags.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                m_serverManager.updateSettings(selected, name, eulaAccepted, flags, viewDistance,
                                               onlineMode, useManagedJavaRuntime);
                QVariantList result;
                for (const auto& server : m_serverManager.listServers())
                {
                    result.append(serverVariant(server));
                }
                return result;
            },
            [](LauncherController& self, const QVariant& result)
            {
                self.publishServers(result.toList());
                self.refreshRuntimes();
            });
    }

    void LauncherController::setServerEulaAccepted(const QString& id, bool accepted)
    {
        runServerOperation(
            QStringLiteral("Saving EULA acceptance…"),
            [this, id, accepted]
            {
                strata::server::ServerConfig selected;
                selected.id = id;
                m_serverManager.setEulaAccepted(selected, accepted);
                QVariantList result;
                for (const auto& server : m_serverManager.listServers())
                {
                    result.append(serverVariant(server));
                }
                return result;
            },
            [](LauncherController& self, const QVariant& result)
            { self.publishServers(result.toList()); });
    }

    void LauncherController::loadServerProperties(const QString& id)
    {
        runServerOperation(
            QStringLiteral("Loading server.properties…"),
            [this, id]
            {
                strata::server::ServerConfig selected;
                selected.id = id;
                return m_serverManager.readServerProperties(selected);
            },
            [](LauncherController& self, const QVariant& result)
            { self.m_serverProperties = result.toString(); });
    }

    void LauncherController::saveServerProperties(const QString& id, const QString& contents)
    {
        runServerOperation(
            QStringLiteral("Saving server.properties…"),
            [this, id, contents]
            {
                strata::server::ServerConfig selected;
                selected.id = id;
                m_serverManager.saveServerProperties(selected, contents);
                QVariantMap result;
                QVariantList servers;
                for (const auto& server : m_serverManager.listServers())
                {
                    servers.append(serverVariant(server));
                }
                result.insert(QStringLiteral("servers"), servers);
                result.insert(QStringLiteral("properties"), contents);
                return result;
            },
            [id](LauncherController& self, const QVariant& result)
            {
                const QVariantMap values = result.toMap();
                self.publishServers(values.value(QStringLiteral("servers")).toList());
                if (self.m_selectedServerId == id)
                {
                    self.m_serverProperties = values.value(QStringLiteral("properties")).toString();
                }
            });
    }

    void LauncherController::deleteServer(const QString& id)
    {
        const auto process = m_serverProcesses.value(id);
        if (process && process->isRunning())
        {
            m_errorMessage = QStringLiteral("Stop the server before deleting it.");
            emit errorMessageChanged();
            return;
        }
        runServerOperation(
            QStringLiteral("Removing server…"),
            [this, id]
            {
                strata::server::ServerConfig selected;
                selected.id = id;
                m_serverManager.deleteServer(selected);
                QVariantList result;
                for (const auto& server : m_serverManager.listServers())
                {
                    result.append(serverVariant(server));
                }
                return result;
            },
            [id](LauncherController& self, const QVariant& result)
            {
                self.publishServers(result.toList());
                self.m_serverProperties.clear();
                self.m_serverConsoleLines.remove(id);
                self.m_serverConsoleCharacters.remove(id);
                if (self.m_selectedServerId == id)
                {
                    self.m_selectedServerId.clear();
                    self.m_serverConsoleText.clear();
                    self.m_serverProcessState.clear();
                    self.m_serverResourceStatus.clear();
                }
                self.refreshRuntimes();
            });
    }

    void LauncherController::selectServer(const QString& id)
    {
        m_selectedServerId = id;
        m_serverConsoleText = m_serverConsoleLines.value(id).join(QLatin1Char('\n'));
        m_serverProcessState = m_serverStates.value(id, QStringLiteral("stopped"));
        m_serverResourceStatus = m_serverResources.value(id, QStringLiteral("Server is stopped."));
        emit serversChanged();
    }

    void LauncherController::installServer(const QString& id)
    {
        runServerInstall(id, false);
    }

    void LauncherController::installServerRuntime(const QString& id)
    {
        runServerInstall(id, true);
    }

    void LauncherController::startServer(const QString& id)
    {
        const auto existing = m_serverProcesses.value(id);
        if (existing && existing->isRunning())
        {
            m_errorMessage = QStringLiteral("This server is already running.");
            emit errorMessageChanged();
            return;
        }
        runServerOperation(
            QStringLiteral("Preparing server launch…"),
            [this, id]
            {
                strata::server::ServerConfig selected;
                selected.id = id;
                const strata::server::ServerLaunchPlan plan = m_serverManager.launchPlan(selected);
                return QVariantMap{{QStringLiteral("program"), plan.program},
                                   {QStringLiteral("arguments"), plan.arguments},
                                   {QStringLiteral("workingDirectory"), plan.workingDirectory},
                                   {QStringLiteral("javaFlags"), plan.javaFlags}};
            },
            [id](LauncherController& self, const QVariant& value)
            {
                const QVariantMap data = value.toMap();
                const strata::server::ServerLaunchPlan plan{
                    data.value(QStringLiteral("program")).toString(),
                    data.value(QStringLiteral("arguments")).toStringList(),
                    data.value(QStringLiteral("workingDirectory")).toString(),
                    data.value(QStringLiteral("javaFlags")).toStringList()};
                const auto previous = self.m_serverProcesses.value(id);
                if (previous)
                {
                    previous->deleteLater();
                }
                auto* process = new strata::server::ServerProcess(id, plan, &self);
                self.m_serverProcesses.insert(id, process);
                QObject::connect(process, &strata::server::ServerProcess::outputReceived, &self,
                                 [&self](const QString& serverId, const QString& line)
                                 { self.appendServerConsoleLine(serverId, line); });
                QObject::connect(process, &strata::server::ServerProcess::statusChanged, &self,
                                 [&self](const QString& serverId, const QString& state)
                                 { self.updateServerProcessState(serverId, state); });
                QObject::connect(
                    process, &strata::server::ServerProcess::snapshotChanged, &self,
                    [&self](const QString& serverId, const QString& state, bool running,
                            qint64 uptime, qint64 memory, double cpu, qint64 cpuTime)
                    {
                        Q_UNUSED(running)
                        const qint64 seconds = uptime / 1'000;
                        const QString memoryText =
                            memory < 0
                                ? QStringLiteral("unavailable")
                                : QStringLiteral("%1 MiB").arg(
                                      static_cast<double>(memory) / (1024.0 * 1024.0), 0, 'f', 0);
                        const QString cpuText = cpu < 0.0
                                                    ? QStringLiteral("unavailable")
                                                    : QStringLiteral("%1%").arg(cpu, 0, 'f', 1);
                        const QString cpuTimeText =
                            cpuTime < 0 ? QStringLiteral("unavailable")
                                        : QStringLiteral("%1:%2:%3")
                                              .arg(cpuTime / 3'600'000, 2, 10, QLatin1Char('0'))
                                              .arg((cpuTime / 60'000) % 60, 2, 10, QLatin1Char('0'))
                                              .arg((cpuTime / 1'000) % 60, 2, 10, QLatin1Char('0'));
                        self.m_serverResources.insert(
                            serverId, QStringLiteral(
                                          "%1 · uptime %2:%3:%4 · memory %5 · CPU %6 · CPU time %7")
                                          .arg(state)
                                          .arg(seconds / 3'600, 2, 10, QLatin1Char('0'))
                                          .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
                                          .arg(seconds % 60, 2, 10, QLatin1Char('0'))
                                          .arg(memoryText, cpuText, cpuTimeText));
                        if (self.m_selectedServerId == serverId)
                        {
                            self.m_serverResourceStatus = self.m_serverResources.value(serverId);
                        }
                        emit self.serversChanged();
                    });
                process->start();
                self.m_serverProcessState = process->status();
                self.m_serverResourceStatus =
                    self.m_serverResources.value(id, QStringLiteral("Server is starting."));
                emit self.serversChanged();
            });
    }

    void LauncherController::stopServer(const QString& id)
    {
        const auto process = m_serverProcesses.value(id);
        if (!process)
        {
            m_errorMessage = QStringLiteral("The selected server is not running.");
            emit errorMessageChanged();
            return;
        }
        try
        {
            process->stop();
        }
        catch (const std::exception& exception)
        {
            m_errorMessage = QString::fromUtf8(exception.what());
            emit errorMessageChanged();
        }
    }

    void LauncherController::terminateServer(const QString& id)
    {
        const auto process = m_serverProcesses.value(id);
        if (!process || !process->isRunning())
        {
            m_errorMessage = QStringLiteral("The selected server is not running.");
            emit errorMessageChanged();
            return;
        }
        updateServerProcessState(id, QStringLiteral("crashed"));
        process->terminate();
    }

    void LauncherController::sendServerCommand(const QString& id, const QString& command)
    {
        const auto process = m_serverProcesses.value(id);
        if (!process)
        {
            m_errorMessage = QStringLiteral("The selected server is not running.");
            emit errorMessageChanged();
            return;
        }
        try
        {
            process->sendCommand(command);
        }
        catch (const std::exception& exception)
        {
            m_errorMessage = QString::fromUtf8(exception.what());
            emit errorMessageChanged();
        }
    }

    void LauncherController::searchServerContent(const QString& id, const QString& query)
    {
        const quint64 revision = ++m_serverContentRevision;
        m_serverContentBusy = true;
        m_serverContentProjects.clear();
        m_serverContentVersions.clear();
        m_serverContentStatus = QStringLiteral("Searching Modrinth…");
        emit serverContentChanged();
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, id, query, revision]()
            {
                if (self.isNull())
                {
                    return;
                }
                QVariantList projects;
                QString error;
                try
                {
                    const auto servers = self->m_serverManager.listServers();
                    const auto server =
                        std::find_if(servers.cbegin(), servers.cend(),
                                     [&id](const strata::server::ServerConfig& value)
                                     { return value.id == id; });
                    if (server == servers.cend())
                    {
                        throw std::runtime_error("The selected server no longer exists.");
                    }
                    strata::modrinth::ModrinthClient client;
                    const auto page = client.searchServer(serverContentType(server->distribution),
                                                          query, server->minecraftVersion, 0, 20);
                    for (const auto& project : page.projects)
                    {
                        projects.append(
                            QVariantMap{{QStringLiteral("id"), project.id},
                                        {QStringLiteral("title"), project.title},
                                        {QStringLiteral("description"), project.description},
                                        {QStringLiteral("downloads"), project.downloads},
                                        {QStringLiteral("iconUrl"), project.iconUrl}});
                    }
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, projects, error, revision]()
                    {
                        if (self.isNull() || revision != self->m_serverContentRevision)
                        {
                            return;
                        }
                        self->m_serverContentBusy = false;
                        self->m_serverContentProjects = projects;
                        self->m_serverContentStatus =
                            error.isEmpty()
                                ? (projects.isEmpty()
                                       ? QStringLiteral("No Modrinth projects found.")
                                       : QStringLiteral("Choose a project to view versions."))
                                : error;
                        emit self->serverContentChanged();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::loadServerContentVersions(const QString& id, const QString& projectId)
    {
        if (m_serverContentBusy)
        {
            return;
        }
        const quint64 revision = ++m_serverContentRevision;
        m_serverContentBusy = true;
        m_serverContentVersions.clear();
        m_serverContentStatus = QStringLiteral("Loading compatible versions…");
        emit serverContentChanged();
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, id, projectId, revision]()
            {
                if (self.isNull())
                {
                    return;
                }
                QVariantList versions;
                QString error;
                try
                {
                    const auto servers = self->m_serverManager.listServers();
                    const auto server =
                        std::find_if(servers.cbegin(), servers.cend(),
                                     [&id](const strata::server::ServerConfig& value)
                                     { return value.id == id; });
                    if (server == servers.cend())
                    {
                        throw std::runtime_error("The selected server no longer exists.");
                    }
                    strata::modrinth::ModrinthClient client;
                    const auto values =
                        client.serverVersions(projectId, server->minecraftVersion,
                                              serverContentType(server->distribution));
                    for (const auto& version : values)
                    {
                        versions.append(
                            QVariantMap{{QStringLiteral("id"), version.id},
                                        {QStringLiteral("projectId"), version.projectId},
                                        {QStringLiteral("versionNumber"), version.versionNumber}});
                    }
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, versions, error, revision]()
                    {
                        if (self.isNull() || revision != self->m_serverContentRevision)
                        {
                            return;
                        }
                        self->m_serverContentBusy = false;
                        self->m_serverContentVersions = versions;
                        self->m_serverContentStatus =
                            error.isEmpty() ? (versions.isEmpty()
                                                   ? QStringLiteral("No compatible versions found.")
                                                   : QStringLiteral("Choose a version to install."))
                                            : error;
                        emit self->serverContentChanged();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::installServerContent(const QString& id, const QString& projectId,
                                                  const QString& versionId)
    {
        if (m_serverContentBusy)
        {
            return;
        }
        const quint64 revision = ++m_serverContentRevision;
        m_serverContentBusy = true;
        m_serverContentStatus = QStringLiteral("Preparing server content download…");
        emit serverContentChanged();
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, id, projectId, versionId, revision]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                QString installedName;
                try
                {
                    const auto servers = self->m_serverManager.listServers();
                    const auto server =
                        std::find_if(servers.cbegin(), servers.cend(),
                                     [&id](const strata::server::ServerConfig& value)
                                     { return value.id == id; });
                    if (server == servers.cend())
                    {
                        throw std::runtime_error("The selected server no longer exists.");
                    }
                    const auto type = serverContentType(server->distribution);
                    strata::modrinth::ModrinthClient client;
                    const auto versions =
                        client.serverVersions(projectId, server->minecraftVersion, type);
                    const auto version =
                        std::find_if(versions.cbegin(), versions.cend(),
                                     [&versionId](const strata::modrinth::ProjectVersion& value)
                                     { return value.id == versionId; });
                    if (version == versions.cend())
                    {
                        throw std::runtime_error(
                            "The selected server-content version is no longer available.");
                    }
                    const auto& file = version->primaryFile();
                    strata::modrinth::ModrinthClient::validateFile(file);
                    if (QFileInfo(file.filename)
                            .suffix()
                            .compare(QStringLiteral("jar"), Qt::CaseInsensitive) != 0)
                    {
                        throw std::runtime_error(
                            "Modrinth server content must be distributed as a JAR file.");
                    }
                    const QString root = self->m_serverManager.serverDirectoryPath(*server);
                    const QFileInfo rootInfo(root);
                    const QString canonicalRoot = rootInfo.canonicalFilePath();
                    if (canonicalRoot.isEmpty() || rootInfo.isSymLink() || !rootInfo.isDir())
                    {
                        throw std::runtime_error("The server content directory is unsafe.");
                    }
                    const QString folder = type == strata::modrinth::ServerContentType::Plugin
                                               ? QStringLiteral("plugins")
                                               : QStringLiteral("mods");
                    const QString directory = QDir(root).filePath(folder);
                    if (!QDir().mkpath(directory))
                    {
                        throw std::runtime_error("Could not create the server content directory.");
                    }
                    const QFileInfo directoryInfo(directory);
                    const QString canonicalDirectory = directoryInfo.canonicalFilePath();
                    if (directoryInfo.isSymLink() || canonicalDirectory.isEmpty() ||
                        !canonicalDirectory.startsWith(canonicalRoot + QDir::separator(),
                                                       Qt::CaseSensitive))
                    {
                        throw std::runtime_error(
                            "The server content directory escapes its server.");
                    }
                    const QString target = QDir(canonicalDirectory).filePath(file.filename);
                    const QFileInfo targetInfo(target);
                    if (targetInfo.fileName() != file.filename || targetInfo.exists() ||
                        targetInfo.isSymLink())
                    {
                        throw std::runtime_error(
                            "A server-content file with this name already exists.");
                    }
                    strata::download::DownloadTask task{
                        QUrl(file.url), target, file.sha1,
                        QStringLiteral("Modrinth server content %1").arg(version->versionNumber)};
                    if (file.size > 0)
                    {
                        task.maxBytes = file.size;
                    }
                    self->m_downloadManager.downloadAll({task});
                    installedName = file.filename;
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, installedName, error, revision]()
                    {
                        if (self.isNull() || revision != self->m_serverContentRevision)
                        {
                            return;
                        }
                        self->m_serverContentBusy = false;
                        self->m_serverContentStatus =
                            error.isEmpty()
                                ? QStringLiteral("Installed %1. Restart the server to apply it.")
                                      .arg(installedName)
                                : error;
                        emit self->serverContentChanged();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::runServerInstall(const QString& id, bool installRuntime)
    {
        const auto process = m_serverProcesses.value(id);
        if (process && process->isRunning())
        {
            m_errorMessage = QStringLiteral("Stop the server before installing or repairing it.");
            emit errorMessageChanged();
            return;
        }
        if (m_serverBusy)
        {
            m_serverStatus = QStringLiteral("Wait for the current server operation to finish.");
            emit serversChanged();
            return;
        }
        m_serverBusy = true;
        m_serverProgress = 0.0;
        m_serverInstallItem = installRuntime ? QStringLiteral("Preparing managed Java runtime…")
                                             : QStringLiteral("Preparing server installation…");
        m_serverStatus = m_serverInstallItem;
        m_errorMessage.clear();
        emit serversChanged();
        emit errorMessageChanged();

        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, id, installRuntime]
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                QVariantList servers;
                const auto progress = [self](const strata::download::Progress& update)
                {
                    if (self.isNull())
                    {
                        return;
                    }
                    const double fraction =
                        update.total > 0
                            ? std::clamp(static_cast<double>(update.completed) / update.total, 0.0,
                                         1.0)
                            : 0.0;
                    QMetaObject::invokeMethod(
                        self,
                        [self, fraction, item = update.currentItem]
                        {
                            if (self.isNull())
                            {
                                return;
                            }
                            self->m_serverProgress = fraction;
                            self->m_serverInstallItem = item;
                            self->m_serverStatus = item;
                            emit self->serversChanged();
                        },
                        Qt::QueuedConnection);
                };
                try
                {
                    strata::server::ServerConfig selected;
                    selected.id = id;
                    if (installRuntime)
                    {
                        self->m_serverManager.installManagedJavaRuntime(selected, progress);
                    }
                    else
                    {
                        self->m_serverManager.install(selected, progress);
                    }
                    for (const auto& server : self->m_serverManager.listServers())
                    {
                        servers.append(serverVariant(server));
                    }
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, id, error, servers, installRuntime]
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_serverBusy = false;
                        if (error.isEmpty())
                        {
                            self->publishServers(servers);
                            self->m_serverProgress = 1.0;
                            self->m_serverInstallItem =
                                installRuntime ? QStringLiteral("Managed Java runtime installed.")
                                               : QStringLiteral("Server installation complete.");
                            self->m_serverStatus = self->m_serverInstallItem;
                        }
                        else
                        {
                            self->m_serverStatus = error;
                            self->m_errorMessage = error;
                            emit self->errorMessageChanged();
                        }
                        if (self->m_selectedServerId == id)
                        {
                            self->m_serverProperties.clear();
                        }
                        emit self->serversChanged();
                        if (error.isEmpty() && installRuntime)
                        {
                            self->refreshRuntimes();
                        }
                    },
                    Qt::QueuedConnection);
            }));
    }

    void LauncherController::publishServers(QVariantList servers)
    {
        for (QVariant& value : servers)
        {
            QVariantMap server = value.toMap();
            const QString id = server.value(QStringLiteral("id")).toString();
            const QString state = m_serverStates.value(id, QStringLiteral("stopped"));
            server.insert(QStringLiteral("processState"), state);
            server.insert(QStringLiteral("running"), state == QStringLiteral("starting") ||
                                                         state == QStringLiteral("running"));
            value = server;
        }
        m_servers = std::move(servers);
    }

    void LauncherController::appendServerConsoleLine(const QString& serverId, const QString& line)
    {
        QStringList& history = m_serverConsoleLines[serverId];
        history.append(line);
        qsizetype& characters = m_serverConsoleCharacters[serverId];
        characters += line.size() + 1;
        while (history.size() > 2'000 || characters > 100'000)
        {
            characters -= history.constFirst().size() + 1;
            history.removeFirst();
        }
        if (m_selectedServerId == serverId)
        {
            m_serverConsoleText = history.join(QLatin1Char('\n'));
        }
        emit serversChanged();
    }

    void LauncherController::updateServerProcessState(const QString& serverId, const QString& state)
    {
        const QString effectiveState =
            m_serverStates.value(serverId) == QStringLiteral("crashed") &&
                    state == QStringLiteral("stopped")
                ? QStringLiteral("crashed")
                : state;
        m_serverStates.insert(serverId, effectiveState);
        if (m_selectedServerId == serverId)
        {
            m_serverProcessState = effectiveState;
        }
        for (QVariant& value : m_servers)
        {
            QVariantMap server = value.toMap();
            if (server.value(QStringLiteral("id")).toString() == serverId)
            {
                server.insert(QStringLiteral("processState"), effectiveState);
                server.insert(QStringLiteral("running"),
                              effectiveState == QStringLiteral("starting") ||
                                  effectiveState == QStringLiteral("running"));
                value = server;
                break;
            }
        }
        emit serversChanged();
    }

    void LauncherController::runServerOperation(
        const QString& status, std::function<QVariant()> operation,
        std::function<void(LauncherController&, const QVariant&)> success)
    {
        if (m_serverBusy)
        {
            m_serverStatus = QStringLiteral("Wait for the current server operation to finish.");
            emit serversChanged();
            return;
        }
        m_serverBusy = true;
        m_serverStatus = status;
        m_errorMessage.clear();
        emit serversChanged();
        emit errorMessageChanged();

        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, operation = std::move(operation), success = std::move(success)]()
            {
                if (self.isNull())
                {
                    return;
                }
                QVariant result;
                QString error;
                try
                {
                    result = operation();
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, result, error, success]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_serverBusy = false;
                        if (error.isEmpty())
                        {
                            success(*self, result);
                            if (!self->m_serverBusy)
                            {
                                self->m_serverStatus = QStringLiteral("Ready");
                            }
                        }
                        else
                        {
                            self->m_serverStatus = error;
                            self->m_errorMessage = error;
                            emit self->errorMessageChanged();
                        }
                        emit self->serversChanged();
                    },
                    Qt::QueuedConnection);
            }));
    }
} // namespace strata::ui
