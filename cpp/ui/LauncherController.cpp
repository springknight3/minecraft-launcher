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
                               QList<strata::runtime::RuntimeOwner>& owners)
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
        }
    } // namespace

    LauncherController::LauncherController(QObject* parent)
        : QObject(parent), m_instanceManager(), m_manifestClient(),
          m_offlineAccountStore(m_instanceManager.dataDirectory()), m_downloadManager(),
          m_runtimeManager(m_downloadManager, m_instanceManager.dataDirectory()),
          m_installer(m_downloadManager, m_instanceManager.dataDirectory())
    {
        m_workers.setMaxThreadCount(3);
        m_logWorkers.setMaxThreadCount(1);
        try
        {
            const auto preferences = m_runtimeManager.preferences();
            m_runtimeVendor =
                strata::runtime::JavaRuntimeManager::vendorName(preferences.vendor).toUpper();
            m_redundantRuntimes = preferences.redundantRuntimes;
        }
        catch (const std::exception& exception)
        {
            m_errorMessage = QString::fromUtf8(exception.what());
        }
        try
        {
            m_offlineUsername = m_offlineAccountStore.username();
        }
        catch (const std::exception& exception)
        {
            m_errorMessage = QString::fromUtf8(exception.what());
        }
    }

    LauncherController::~LauncherController()
    {
        for (const auto& process : m_processes)
        {
            if (process && process->isRunning())
            {
                process->stop();
            }
        }
        m_workers.waitForDone();
        m_logWorkers.waitForDone();
    }

    QVariantList LauncherController::versions() const
    {
        return m_versions;
    }

    QVariantList LauncherController::instances() const
    {
        return m_instances;
    }

    QString LauncherController::latestRelease() const
    {
        return m_manifest.latestRelease;
    }

    bool LauncherController::installing() const
    {
        return m_installing;
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

    bool LauncherController::installingRuntime() const
    {
        return m_installingRuntime;
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

    void LauncherController::publishInstances()
    {
        m_instances = toVariantInstances(m_instanceRecords, m_processStates, m_runtimeModes,
                                         m_runtimeComponents);
        emit instancesChanged();
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
                    readRuntimeOwners(instances, runtimeModes, runtimeComponents, runtimeOwners);
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
                        },
                        Qt::QueuedConnection);
                }
            }));
    }

    void LauncherController::installInstance(const QString& name, const QString& versionId)
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
            [self, normalizedName, selectedVersion]()
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
                    self->m_installer.install(instance, selectedVersion,
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
                                                          self->m_installCompleted =
                                                              progress.completed;
                                                          self->m_installTotal = progress.total;
                                                          self->m_currentInstallItem =
                                                              progress.currentItem;
                                                          emit self->installationChanged();
                                                      },
                                                      Qt::QueuedConnection);
                                              });
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
                                              runtimeOwners);
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
                        }
                        emit self->errorMessageChanged();
                    },
                    Qt::QueuedConnection);
            }));
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
        if (m_offlineUsername.isEmpty())
        {
            m_errorMessage = QStringLiteral("Set an offline player name before launching.");
            emit errorMessageChanged();
            return;
        }
        const strata::auth::OfflineAccount account{
            m_offlineUsername, strata::auth::OfflineAccountStore::offlineUuid(m_offlineUsername)};
        updateProcessState(instanceId, QStringLiteral("starting"));

        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, instanceId, account]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                strata::Instance instance;
                strata::launch::LaunchPlan plan;
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
                    const strata::InstanceSettings settings(instance.directory);
                    QString javaExecutable;
                    if (settings.javaRuntimeMode() ==
                        strata::InstanceSettings::JavaRuntimeMode::Managed)
                    {
                        javaExecutable = self->m_runtimeManager.javaExecutable(instance);
                    }
                    plan = strata::launch::GameLauncher::buildPlan(
                        instance, account, self->m_instanceManager.dataDirectory(), javaExecutable);
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
            [self, parsedVendor, redundant]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                try
                {
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
                    readRuntimeOwners(instances, runtimeModes, runtimeComponents, runtimeOwners);
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
                    readRuntimeOwners(instances, runtimeModes, runtimeComponents, runtimeOwners);
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
        const auto owners = m_runtimeOwners;
        QPointer<LauncherController> self(this);
        m_workers.start(QRunnable::create(
            [self, owners]()
            {
                if (self.isNull())
                {
                    return;
                }
                QList<strata::runtime::InstalledRuntime> runtimes;
                QString error;
                try
                {
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
                        if (error.isEmpty())
                        {
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
} // namespace strata::ui
