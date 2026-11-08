#include "ContentController.h"

#include <QMetaObject>
#include <QPointer>
#include <QRunnable>

#include <algorithm>
#include <stdexcept>

namespace strata::ui
{
    namespace
    {
        [[noreturn]] void fail(const QString& message)
        {
            throw std::runtime_error(message.toStdString());
        }

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
                return QStringLiteral("An unknown content error occurred.");
            }
        }

        QVariantMap projectMap(const modrinth::Project& project)
        {
            return {{QStringLiteral("id"), project.id},
                    {QStringLiteral("slug"), project.slug},
                    {QStringLiteral("title"), project.title},
                    {QStringLiteral("description"), project.description},
                    {QStringLiteral("projectType"), project.projectType},
                    {QStringLiteral("downloads"), project.downloads},
                    {QStringLiteral("iconUrl"), project.iconUrl},
                    {QStringLiteral("categories"), project.categories}};
        }

        QVariantMap versionMap(const modrinth::ProjectVersion& version)
        {
            QVariantList dependencies;
            for (const modrinth::VersionDependency& dependency : version.dependencies)
            {
                dependencies.append(
                    QVariantMap{{QStringLiteral("projectId"), dependency.projectId},
                                {QStringLiteral("versionId"), dependency.versionId},
                                {QStringLiteral("type"), dependency.dependencyType}});
            }
            return {{QStringLiteral("id"), version.id},
                    {QStringLiteral("projectId"), version.projectId},
                    {QStringLiteral("versionNumber"), version.versionNumber},
                    {QStringLiteral("projectType"), version.projectType},
                    {QStringLiteral("gameVersions"), version.gameVersions},
                    {QStringLiteral("loaders"), version.loaders},
                    {QStringLiteral("dependencies"), dependencies}};
        }

        QVariantMap installedMap(const modrinth::InstalledContent& item)
        {
            return {{QStringLiteral("type"), modrinth::displayName(item.type)},
                    {QStringLiteral("projectId"), item.projectId},
                    {QStringLiteral("slug"), item.slug},
                    {QStringLiteral("title"), item.title},
                    {QStringLiteral("versionId"), item.versionId},
                    {QStringLiteral("versionNumber"), item.versionNumber},
                    {QStringLiteral("worldName"), item.worldName},
                    {QStringLiteral("filename"), item.filename},
                    {QStringLiteral("installedAt"), item.installedAt},
                    {QStringLiteral("enabled"), item.enabled}};
        }
    } // namespace

    ContentController::ContentController(const InstanceManager& instanceManager,
                                         const loader::LoaderInstaller& loaderInstaller,
                                         const download::DownloadManager& downloadManager,
                                         QObject* parent)
        : QObject(parent), m_instanceManager(instanceManager), m_loaderInstaller(loaderInstaller),
          m_contentManager(m_modrinthClient, downloadManager)
    {
        m_workers.setMaxThreadCount(3);
    }

    ContentController::~ContentController()
    {
        m_workers.waitForDone();
    }

    QVariantList ContentController::projects() const
    {
        QVariantList result;
        result.reserve(m_projectRecords.size());
        for (const modrinth::Project& project : m_projectRecords)
        {
            result.append(projectMap(project));
        }
        return result;
    }

    QVariantList ContentController::versions() const
    {
        QVariantList result;
        result.reserve(m_versionRecords.size());
        for (const modrinth::ProjectVersion& version : m_versionRecords)
        {
            result.append(versionMap(version));
        }
        return result;
    }

    QVariantList ContentController::installedContent() const
    {
        QVariantList result;
        result.reserve(m_installedRecords.size());
        for (const modrinth::InstalledContent& item : m_installedRecords)
        {
            result.append(installedMap(item));
        }
        return result;
    }

    QStringList ContentController::worlds() const
    {
        return m_worldNames;
    }

    QString ContentController::statusMessage() const
    {
        return m_statusMessage;
    }

    QString ContentController::selectedProjectId() const
    {
        return m_selectedProjectId;
    }

    int ContentController::totalProjects() const
    {
        return m_totalProjects;
    }

    int ContentController::pageOffset() const
    {
        return m_pageOffset;
    }

    int ContentController::pageSize() const
    {
        return 20;
    }

    bool ContentController::canGoBack() const
    {
        return m_pageOffset > 0;
    }

    bool ContentController::canGoForward() const
    {
        return m_pageOffset + pageSize() < m_totalProjects;
    }

    bool ContentController::modsAvailable() const
    {
        return m_modsAvailable;
    }

    bool ContentController::loadingTarget() const
    {
        return m_loadingTarget;
    }

    bool ContentController::searching() const
    {
        return m_searching;
    }

    bool ContentController::loadingVersions() const
    {
        return m_loadingVersions;
    }

    bool ContentController::busy() const
    {
        return m_busy;
    }

    double ContentController::installProgress() const
    {
        return m_installTotal > 0 ? static_cast<double>(m_installCompleted) / m_installTotal : -1.0;
    }

    QString ContentController::installItem() const
    {
        return m_installItem;
    }

    QVariantList ContentController::dependenciesToInstall() const
    {
        QVariantList result;
        if (!m_pendingInstall.has_value())
        {
            return result;
        }
        result.reserve(m_pendingInstall->dependencies.size());
        for (const modrinth::DependencyInstall& dependency : m_pendingInstall->dependencies)
        {
            result.append(
                QVariantMap{{QStringLiteral("title"), dependency.project.title},
                            {QStringLiteral("versionNumber"), dependency.version.versionNumber},
                            {QStringLiteral("projectType"), dependency.project.projectType}});
        }
        return result;
    }

    bool ContentController::dependencyConfirmationVisible() const
    {
        return m_pendingInstall.has_value() && !m_pendingInstall->dependencies.isEmpty();
    }

    void ContentController::selectTarget(const QString& instanceId, const QString& contentType)
    {
        const quint64 revision = ++m_targetRevision;
        ++m_targetDataRevision;
        ++m_searchRevision;
        ++m_versionsRevision;
        m_pendingInstall.reset();
        m_instanceId = instanceId;
        m_selectedProjectId.clear();
        m_projectRecords.clear();
        m_versionRecords.clear();
        m_installedRecords.clear();
        m_worldNames.clear();
        m_totalProjects = 0;
        m_pageOffset = 0;
        m_searching = false;
        m_loadingVersions = false;
        m_loadingTarget = false;
        m_modsAvailable = false;
        try
        {
            m_type = modrinth::parseContentType(contentType);
        }
        catch (...)
        {
            m_statusMessage = exceptionMessage();
            publishContent();
            return;
        }
        if (m_instanceId.isEmpty())
        {
            m_statusMessage = QStringLiteral("Select an instance to browse content.");
            publishContent();
            return;
        }
        m_loadingTarget = true;
        m_statusMessage = QStringLiteral("Loading instance content...");
        publishContent();
        loadTargetData(revision);
    }

    void ContentController::search(const QString& query, int offset)
    {
        if (m_instanceId.isEmpty() || m_loadingTarget)
        {
            m_statusMessage = QStringLiteral("Select an instance before searching Modrinth.");
            publishContent();
            return;
        }
        if (m_type == modrinth::ContentType::Mod && !m_modsAvailable)
        {
            m_statusMessage = QStringLiteral(
                "Mods require an installed Fabric, Quilt, Forge, or NeoForge loader.");
            publishContent();
            return;
        }
        if (offset < 0 || offset % pageSize() != 0)
        {
            m_statusMessage = QStringLiteral("The requested Modrinth page is invalid.");
            publishContent();
            return;
        }
        const quint64 targetRevision = m_targetRevision;
        const quint64 searchRevision = ++m_searchRevision;
        const QString instanceId = m_instanceId;
        const modrinth::ContentType type = m_type;
        m_projectRecords.clear();
        m_versionRecords.clear();
        m_selectedProjectId.clear();
        m_totalProjects = 0;
        m_pageOffset = offset;
        m_searching = true;
        m_statusMessage = QStringLiteral("Searching Modrinth...");
        publishContent();
        QPointer<ContentController> self(this);
        m_workers.start(QRunnable::create(
            [self, targetRevision, searchRevision, instanceId, type, query, offset]()
            {
                if (self.isNull())
                {
                    return;
                }
                QList<modrinth::Project> projects;
                int total = 0;
                QString error;
                try
                {
                    const Instance instance = self->findInstance(instanceId);
                    const loader::LoaderSelection selection =
                        self->m_loaderInstaller.installedSelection(instance);
                    const modrinth::SearchPage page = self->m_modrinthClient.search(
                        type, query, instance.versionId, selection.type, offset, self->pageSize());
                    projects = page.projects;
                    total = page.total;
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, targetRevision, searchRevision, projects, total, offset, error]()
                    {
                        if (self.isNull() || self->m_targetRevision != targetRevision ||
                            self->m_searchRevision != searchRevision)
                        {
                            return;
                        }
                        self->m_projectRecords = projects;
                        self->m_totalProjects = total;
                        self->m_pageOffset = offset;
                        self->m_searching = false;
                        self->m_statusMessage =
                            error.isEmpty()
                                ? QStringLiteral("Showing %1 of %2 compatible projects.")
                                      .arg(projects.size())
                                      .arg(total)
                                : QStringLiteral("Modrinth search failed: %1").arg(error);
                        self->publishContent();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void ContentController::loadVersions(const QString& projectId)
    {
        const auto project = std::find_if(m_projectRecords.cbegin(), m_projectRecords.cend(),
                                          [&projectId](const modrinth::Project& item)
                                          { return item.id == projectId; });
        if (project == m_projectRecords.cend())
        {
            m_selectedProjectId.clear();
            m_versionRecords.clear();
            m_statusMessage = QStringLiteral("Select a project from the current search results.");
            publishContent();
            return;
        }
        const quint64 targetRevision = m_targetRevision;
        const quint64 versionsRevision = ++m_versionsRevision;
        const QString instanceId = m_instanceId;
        const QString selectedProjectId = project->id;
        const QString title = project->title;
        const modrinth::ContentType type = m_type;
        m_selectedProjectId = selectedProjectId;
        m_versionRecords.clear();
        m_loadingVersions = true;
        m_statusMessage = QStringLiteral("Loading compatible versions for %1...").arg(title);
        publishContent();
        QPointer<ContentController> self(this);
        m_workers.start(QRunnable::create(
            [self, targetRevision, versionsRevision, instanceId, selectedProjectId, type, title]()
            {
                if (self.isNull())
                {
                    return;
                }
                QList<modrinth::ProjectVersion> versions;
                QString error;
                try
                {
                    const Instance instance = self->findInstance(instanceId);
                    const loader::LoaderSelection selection =
                        self->m_loaderInstaller.installedSelection(instance);
                    versions = self->m_modrinthClient.versions(
                        selectedProjectId, instance.versionId, selection.type, type);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, targetRevision, versionsRevision, selectedProjectId, title, versions,
                     error]()
                    {
                        if (self.isNull() || self->m_targetRevision != targetRevision ||
                            self->m_versionsRevision != versionsRevision ||
                            self->m_selectedProjectId != selectedProjectId)
                        {
                            return;
                        }
                        self->m_versionRecords = versions;
                        self->m_loadingVersions = false;
                        self->m_statusMessage =
                            error.isEmpty()
                                ? (versions.isEmpty()
                                       ? QStringLiteral("No compatible versions found for %1.")
                                             .arg(title)
                                       : QStringLiteral("%1 compatible versions available for %2.")
                                             .arg(versions.size())
                                             .arg(title))
                                : QStringLiteral("Could not load project versions: %1").arg(error);
                        self->publishContent();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void ContentController::installSelected(const QString& versionId, const QString& worldName)
    {
        if (m_busy || m_pendingInstall.has_value() || m_instanceId.isEmpty())
        {
            return;
        }
        const auto project = std::find_if(m_projectRecords.cbegin(), m_projectRecords.cend(),
                                          [this](const modrinth::Project& item)
                                          { return item.id == m_selectedProjectId; });
        const auto version = std::find_if(m_versionRecords.cbegin(), m_versionRecords.cend(),
                                          [&versionId](const modrinth::ProjectVersion& item)
                                          { return item.id == versionId; });
        if (project == m_projectRecords.cend() || version == m_versionRecords.cend() ||
            version->projectId != project->id)
        {
            m_statusMessage = QStringLiteral("Choose a project and compatible version first.");
            publishContent();
            return;
        }
        const quint64 targetRevision = m_targetRevision;
        PendingInstall request{targetRevision, m_instanceId, worldName, m_type,
                               *project,       *version,     {}};
        m_busy = true;
        m_installCompleted = 0;
        m_installTotal = 0;
        m_installItem = QStringLiteral("Checking required dependencies");
        m_statusMessage =
            QStringLiteral("Checking required dependencies for %1...").arg(project->title);
        publishContent();
        QPointer<ContentController> self(this);
        m_workers.start(QRunnable::create(
            [self, request]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                QList<modrinth::DependencyInstall> dependencies;
                try
                {
                    const Instance instance = self->findInstance(request.instanceId);
                    const loader::LoaderSelection selection =
                        self->m_loaderInstaller.installedSelection(instance);
                    dependencies = self->m_contentManager.resolveRequiredDependencies(
                        instance, selection.type, request.project, request.version);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, request, dependencies, error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_busy = false;
                        if (self->m_targetRevision != request.targetRevision)
                        {
                            self->publishContent();
                            return;
                        }
                        if (!error.isEmpty())
                        {
                            self->m_statusMessage =
                                QStringLiteral("Could not resolve dependencies: %1").arg(error);
                            self->m_installItem = QStringLiteral("Dependency check failed");
                        }
                        else if (!dependencies.isEmpty())
                        {
                            PendingInstall pending = request;
                            pending.dependencies = dependencies;
                            self->m_pendingInstall = std::move(pending);
                            self->m_statusMessage = QStringLiteral(
                                "Review the required dependencies before installing.");
                            self->m_installItem.clear();
                        }
                        else
                        {
                            self->startInstall(request);
                            return;
                        }
                        self->publishContent();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void ContentController::confirmDependencyInstall()
    {
        if (m_busy || !m_pendingInstall.has_value())
        {
            return;
        }
        PendingInstall request = std::move(*m_pendingInstall);
        m_pendingInstall.reset();
        if (request.targetRevision != m_targetRevision)
        {
            m_statusMessage =
                QStringLiteral("The selected instance changed; choose the project again.");
            publishContent();
            return;
        }
        startInstall(std::move(request));
    }

    void ContentController::cancelDependencyInstall()
    {
        if (!m_pendingInstall.has_value())
        {
            return;
        }
        m_pendingInstall.reset();
        m_statusMessage = QStringLiteral("Installation cancelled.");
        m_installItem.clear();
        publishContent();
    }

    void ContentController::startInstall(PendingInstall request)
    {
        if (m_busy)
        {
            return;
        }
        m_pendingInstall.reset();
        m_busy = true;
        m_installCompleted = 0;
        m_installTotal = 0;
        m_installItem = QStringLiteral("Preparing %1").arg(request.project.title);
        m_statusMessage = QStringLiteral("Installing %1...").arg(request.project.title);
        publishContent();
        QPointer<ContentController> self(this);
        m_workers.start(QRunnable::create(
            [self, request = std::move(request)]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                QList<modrinth::InstalledContent> installed;
                QStringList worlds;
                try
                {
                    const Instance instance = self->findInstance(request.instanceId);
                    const loader::LoaderSelection selection =
                        self->m_loaderInstaller.installedSelection(instance);
                    const auto progress = [self, targetRevision = request.targetRevision](
                                              const download::Progress& update)
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        QMetaObject::invokeMethod(
                            self,
                            [self, targetRevision, update]()
                            {
                                if (self.isNull() || self->m_targetRevision != targetRevision)
                                {
                                    return;
                                }
                                self->m_installCompleted = update.completed;
                                self->m_installTotal = update.total;
                                self->m_installItem = update.currentItem;
                                self->publishContent();
                            },
                            Qt::QueuedConnection);
                    };
                    for (const modrinth::DependencyInstall& dependency : request.dependencies)
                    {
                        try
                        {
                            const QString worldName =
                                dependency.type == modrinth::ContentType::DataPack
                                    ? request.worldName
                                    : QString();
                            self->m_contentManager.install(
                                instance, dependency.type, selection.type, worldName,
                                dependency.project, dependency.version, progress);
                        }
                        catch (...)
                        {
                            fail(QStringLiteral("Could not install required dependency %1: %2")
                                     .arg(dependency.project.title, exceptionMessage()));
                        }
                    }
                    self->m_contentManager.install(instance, request.type, selection.type,
                                                   request.worldName, request.project,
                                                   request.version, progress);
                    installed = self->m_contentManager.installed(instance, request.type);
                    if (request.type == modrinth::ContentType::DataPack)
                    {
                        worlds = self->m_contentManager.worlds(instance);
                    }
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, request, installed, worlds, error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_busy = false;
                        if (self->m_targetRevision == request.targetRevision)
                        {
                            if (error.isEmpty())
                            {
                                self->m_installedRecords = installed;
                                self->m_worldNames = worlds;
                                self->m_statusMessage =
                                    request.dependencies.isEmpty()
                                        ? QStringLiteral("Installed %1.").arg(request.project.title)
                                        : QStringLiteral(
                                              "Installed %1 and %2 required dependencies.")
                                              .arg(request.project.title)
                                              .arg(request.dependencies.size());
                                self->m_installItem = QStringLiteral("Installation complete");
                            }
                            else
                            {
                                self->m_statusMessage =
                                    QStringLiteral("Could not install content: %1").arg(error);
                                self->m_installItem = QStringLiteral("Installation failed");
                            }
                        }
                        self->publishContent();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void ContentController::setInstalledEnabled(int index, bool enabled)
    {
        if (m_busy || index < 0 || index >= m_installedRecords.size() || m_instanceId.isEmpty())
        {
            return;
        }
        const quint64 targetRevision = m_targetRevision;
        const QString instanceId = m_instanceId;
        const modrinth::ContentType type = m_type;
        const modrinth::InstalledContent entry = m_installedRecords.at(index);
        m_busy = true;
        m_statusMessage = enabled ? QStringLiteral("Enabling %1...").arg(entry.title)
                                  : QStringLiteral("Disabling %1...").arg(entry.title);
        publishContent();
        QPointer<ContentController> self(this);
        m_workers.start(QRunnable::create(
            [self, targetRevision, instanceId, type, entry, enabled]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                QList<modrinth::InstalledContent> installed;
                try
                {
                    const Instance instance = self->findInstance(instanceId);
                    self->m_contentManager.setEnabled(instance, entry, enabled);
                    installed = self->m_contentManager.installed(instance, type);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, targetRevision, installed, entry, enabled, error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_busy = false;
                        if (self->m_targetRevision == targetRevision)
                        {
                            if (error.isEmpty())
                            {
                                self->m_installedRecords = installed;
                                self->m_statusMessage =
                                    enabled ? QStringLiteral("Enabled %1.").arg(entry.title)
                                            : QStringLiteral("Disabled %1.").arg(entry.title);
                            }
                            else
                            {
                                self->m_statusMessage =
                                    QStringLiteral("Could not change content state: %1").arg(error);
                            }
                        }
                        self->publishContent();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void ContentController::removeInstalled(int index)
    {
        if (m_busy || index < 0 || index >= m_installedRecords.size() || m_instanceId.isEmpty())
        {
            return;
        }
        const quint64 targetRevision = m_targetRevision;
        const QString instanceId = m_instanceId;
        const modrinth::ContentType type = m_type;
        const modrinth::InstalledContent entry = m_installedRecords.at(index);
        m_busy = true;
        m_statusMessage = QStringLiteral("Removing %1...").arg(entry.title);
        publishContent();
        QPointer<ContentController> self(this);
        m_workers.start(QRunnable::create(
            [self, targetRevision, instanceId, type, entry]()
            {
                if (self.isNull())
                {
                    return;
                }
                QString error;
                QList<modrinth::InstalledContent> installed;
                try
                {
                    const Instance instance = self->findInstance(instanceId);
                    self->m_contentManager.remove(instance, entry);
                    installed = self->m_contentManager.installed(instance, type);
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, targetRevision, installed, entry, error]()
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_busy = false;
                        if (self->m_targetRevision == targetRevision)
                        {
                            if (error.isEmpty())
                            {
                                self->m_installedRecords = installed;
                                self->m_statusMessage =
                                    QStringLiteral("Removed %1.").arg(entry.title);
                            }
                            else
                            {
                                self->m_statusMessage =
                                    QStringLiteral("Could not remove content: %1").arg(error);
                            }
                        }
                        self->publishContent();
                    },
                    Qt::QueuedConnection);
            }));
    }

    Instance ContentController::findInstance(const QString& id) const
    {
        const QList<Instance> instances = m_instanceManager.listInstances();
        const auto found =
            std::find_if(instances.cbegin(), instances.cend(),
                         [&id](const Instance& instance) { return instance.id == id; });
        if (found == instances.cend())
        {
            fail(QStringLiteral("Selected instance no longer exists."));
        }
        return *found;
    }

    void ContentController::loadTargetData(quint64 targetRevision)
    {
        const quint64 dataRevision = ++m_targetDataRevision;
        const QString instanceId = m_instanceId;
        const modrinth::ContentType type = m_type;
        QPointer<ContentController> self(this);
        m_workers.start(QRunnable::create(
            [self, targetRevision, dataRevision, instanceId, type]()
            {
                if (self.isNull())
                {
                    return;
                }
                QList<modrinth::InstalledContent> installed;
                QStringList worlds;
                loader::LoaderType loaderType = loader::LoaderType::Vanilla;
                QString error;
                try
                {
                    const Instance instance = self->findInstance(instanceId);
                    loaderType = self->m_loaderInstaller.installedSelection(instance).type;
                    installed = self->m_contentManager.installed(instance, type);
                    if (type == modrinth::ContentType::DataPack)
                    {
                        worlds = self->m_contentManager.worlds(instance);
                    }
                }
                catch (...)
                {
                    error = exceptionMessage();
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, targetRevision, dataRevision, installed, worlds, loaderType, error]()
                    {
                        if (self.isNull() || self->m_targetRevision != targetRevision ||
                            self->m_targetDataRevision != dataRevision)
                        {
                            return;
                        }
                        self->m_installedRecords = installed;
                        self->m_worldNames = worlds;
                        self->m_modsAvailable = loaderType != loader::LoaderType::Vanilla;
                        self->m_loadingTarget = false;
                        if (!error.isEmpty())
                        {
                            self->m_statusMessage =
                                QStringLiteral("Could not load instance content: %1").arg(error);
                        }
                        else if (self->m_type == modrinth::ContentType::Mod &&
                                 !self->m_modsAvailable)
                        {
                            self->m_statusMessage =
                                QStringLiteral("Mods require an installed mod loader.");
                        }
                        else if (self->m_type == modrinth::ContentType::DataPack &&
                                 self->m_worldNames.isEmpty())
                        {
                            self->m_statusMessage = QStringLiteral(
                                "Create or launch a world before installing data packs.");
                        }
                        else
                        {
                            self->m_statusMessage.clear();
                        }
                        self->publishContent();
                        if (error.isEmpty() &&
                            (self->m_type != modrinth::ContentType::Mod || self->m_modsAvailable))
                        {
                            self->search(QString(), 0);
                        }
                    },
                    Qt::QueuedConnection);
            }));
    }

    void ContentController::publishContent()
    {
        emit contentChanged();
    }
} // namespace strata::ui
