#pragma once

#include "../../core/download/DownloadManager.h"
#include "../../core/instance/InstanceManager.h"
#include "../../core/loader/LoaderInstaller.h"
#include "../../core/modrinth/ContentManager.h"
#include "../../core/modrinth/ModrinthClient.h"

#include <QThreadPool>
#include <QVariantList>

#include <optional>

namespace strata::ui
{
    class ContentController : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(QVariantList projects READ projects NOTIFY contentChanged)
        Q_PROPERTY(QVariantList versions READ versions NOTIFY contentChanged)
        Q_PROPERTY(QVariantList installedContent READ installedContent NOTIFY contentChanged)
        Q_PROPERTY(QStringList worlds READ worlds NOTIFY contentChanged)
        Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY contentChanged)
        Q_PROPERTY(QString selectedProjectId READ selectedProjectId NOTIFY contentChanged)
        Q_PROPERTY(int totalProjects READ totalProjects NOTIFY contentChanged)
        Q_PROPERTY(int pageOffset READ pageOffset NOTIFY contentChanged)
        Q_PROPERTY(int pageSize READ pageSize CONSTANT)
        Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY contentChanged)
        Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY contentChanged)
        Q_PROPERTY(bool modsAvailable READ modsAvailable NOTIFY contentChanged)
        Q_PROPERTY(bool loadingTarget READ loadingTarget NOTIFY contentChanged)
        Q_PROPERTY(bool searching READ searching NOTIFY contentChanged)
        Q_PROPERTY(bool loadingVersions READ loadingVersions NOTIFY contentChanged)
        Q_PROPERTY(bool busy READ busy NOTIFY contentChanged)
        Q_PROPERTY(double installProgress READ installProgress NOTIFY contentChanged)
        Q_PROPERTY(QString installItem READ installItem NOTIFY contentChanged)
        Q_PROPERTY(
            QVariantList dependenciesToInstall READ dependenciesToInstall NOTIFY contentChanged)
        Q_PROPERTY(bool dependencyConfirmationVisible READ dependencyConfirmationVisible NOTIFY
                       contentChanged)

    public:
        ContentController(const InstanceManager& instanceManager,
                          const loader::LoaderInstaller& loaderInstaller,
                          const download::DownloadManager& downloadManager,
                          QObject* parent = nullptr);
        ~ContentController() override;

        QVariantList projects() const;
        QVariantList versions() const;
        QVariantList installedContent() const;
        QStringList worlds() const;
        QString statusMessage() const;
        QString selectedProjectId() const;
        int totalProjects() const;
        int pageOffset() const;
        int pageSize() const;
        bool canGoBack() const;
        bool canGoForward() const;
        bool modsAvailable() const;
        bool loadingTarget() const;
        bool searching() const;
        bool loadingVersions() const;
        bool busy() const;
        double installProgress() const;
        QString installItem() const;
        QVariantList dependenciesToInstall() const;
        bool dependencyConfirmationVisible() const;

        Q_INVOKABLE void selectTarget(const QString& instanceId, const QString& contentType);
        Q_INVOKABLE void search(const QString& query, int offset);
        Q_INVOKABLE void loadVersions(const QString& projectId);
        Q_INVOKABLE void installSelected(const QString& versionId, const QString& worldName);
        Q_INVOKABLE void confirmDependencyInstall();
        Q_INVOKABLE void cancelDependencyInstall();
        Q_INVOKABLE void setInstalledEnabled(int index, bool enabled);
        Q_INVOKABLE void removeInstalled(int index);

    signals:
        void contentChanged();

    private:
        struct PendingInstall
        {
            quint64 targetRevision = 0;
            QString instanceId;
            QString worldName;
            modrinth::ContentType type = modrinth::ContentType::Mod;
            modrinth::Project project;
            modrinth::ProjectVersion version;
            QList<modrinth::DependencyInstall> dependencies;
        };

        const InstanceManager& m_instanceManager;
        const loader::LoaderInstaller& m_loaderInstaller;
        modrinth::ModrinthClient m_modrinthClient;
        modrinth::ContentManager m_contentManager;
        QThreadPool m_workers;
        QList<modrinth::Project> m_projectRecords;
        QList<modrinth::ProjectVersion> m_versionRecords;
        QList<modrinth::InstalledContent> m_installedRecords;
        QStringList m_worldNames;
        QString m_instanceId;
        QString m_selectedProjectId;
        QString m_statusMessage;
        QString m_installItem;
        modrinth::ContentType m_type = modrinth::ContentType::Mod;
        int m_totalProjects = 0;
        int m_pageOffset = 0;
        int m_installCompleted = 0;
        int m_installTotal = 0;
        quint64 m_targetRevision = 0;
        quint64 m_targetDataRevision = 0;
        quint64 m_searchRevision = 0;
        quint64 m_versionsRevision = 0;
        bool m_modsAvailable = false;
        bool m_loadingTarget = false;
        bool m_searching = false;
        bool m_loadingVersions = false;
        bool m_busy = false;
        std::optional<PendingInstall> m_pendingInstall;

        strata::Instance findInstance(const QString& id) const;
        void loadTargetData(quint64 targetRevision);
        void startInstall(PendingInstall request);
        void publishContent();
    };
} // namespace strata::ui
