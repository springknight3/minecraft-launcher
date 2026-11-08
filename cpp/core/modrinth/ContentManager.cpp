#include "ContentManager.h"

#include "../loader/LoaderInstaller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <functional>
#include <stdexcept>

namespace strata::modrinth
{
    namespace
    {
        constexpr qsizetype kMaxStateBytes = 4 * 1024 * 1024;
        const QString kStateFile = QStringLiteral(".stratalauncher-content.json");

        [[noreturn]] void fail(const QString& message)
        {
            throw std::runtime_error(message.toStdString());
        }

        QString contentTypeKey(ContentType type)
        {
            switch (type)
            {
            case ContentType::Mod:
                return QStringLiteral("MOD");
            case ContentType::ResourcePack:
                return QStringLiteral("RESOURCE_PACK");
            case ContentType::DataPack:
                return QStringLiteral("DATA_PACK");
            case ContentType::Shader:
                return QStringLiteral("SHADER");
            }
            fail(QStringLiteral("Unknown content type."));
        }

        QString loaderKey(loader::LoaderType type)
        {
            switch (type)
            {
            case loader::LoaderType::Fabric:
                return QStringLiteral("fabric");
            case loader::LoaderType::Quilt:
                return QStringLiteral("quilt");
            case loader::LoaderType::Forge:
                return QStringLiteral("forge");
            case loader::LoaderType::NeoForge:
                return QStringLiteral("neoforge");
            case loader::LoaderType::Vanilla:
                fail(QStringLiteral("Vanilla instances cannot install mods."));
            }
            fail(QStringLiteral("Unknown mod loader."));
        }

        bool safeWorldName(const QString& value)
        {
            return !value.trimmed().isEmpty() && value != QStringLiteral(".") &&
                   value != QStringLiteral("..") && !value.contains(QLatin1Char('/')) &&
                   !value.contains(QLatin1Char('\\')) &&
                   std::none_of(value.cbegin(), value.cend(), [](QChar character)
                                { return character.isNull() || !character.isPrint(); });
        }

        bool isWithin(const QString& root, const QString& path);

        QString canonicalGameRoot(const Instance& instance)
        {
            const QFileInfo instanceInfo(instance.directory);
            const QFileInfo info(instance.gameDirectory());
            const QString instanceRoot = instanceInfo.canonicalFilePath();
            const QString root = info.canonicalFilePath();
            if (instanceRoot.isEmpty() || instanceInfo.isSymLink() || root.isEmpty() ||
                !info.isDir() || info.isSymLink() ||
                !isWithin(QDir::cleanPath(instanceRoot), QDir::cleanPath(root)))
            {
                fail(QStringLiteral("Instance game directory is missing or unsafe."));
            }
            return QDir::cleanPath(root);
        }

        bool isWithin(const QString& root, const QString& path)
        {
            const QString prefix =
                root.endsWith(QDir::separator()) ? root : root + QDir::separator();
            return path == root || path.startsWith(prefix);
        }

        void ensureSafeDirectory(const QString& gameRoot, const QString& targetDirectory)
        {
            const QString absoluteTarget =
                QDir::cleanPath(QFileInfo(targetDirectory).absoluteFilePath());
            const QString relative = QDir(gameRoot).relativeFilePath(absoluteTarget);
            if (relative == QStringLiteral(".") || QDir::isAbsolutePath(relative) ||
                relative == QStringLiteral("..") || relative.startsWith(QStringLiteral("../")) ||
                relative.startsWith(QStringLiteral("..\\")))
            {
                fail(QStringLiteral("Content directory escapes the instance."));
            }

            QString current = gameRoot;
            const QStringList segments =
                QDir::fromNativeSeparators(relative).split(QLatin1Char('/'), Qt::SkipEmptyParts);
            for (const QString& segment : segments)
            {
                if (segment == QStringLiteral(".") || segment == QStringLiteral(".."))
                {
                    fail(QStringLiteral("Content directory contains an unsafe path."));
                }
                current = QDir(current).filePath(segment);
                QFileInfo info(current);
                if (info.exists() || info.isSymLink())
                {
                    if (info.isSymLink() || !info.isDir())
                    {
                        fail(QStringLiteral("Content directory contains an unsafe path."));
                    }
                }
                else if (!QDir().mkdir(current))
                {
                    fail(QStringLiteral("Could not create content directory: %1").arg(current));
                }
                const QString canonical = QFileInfo(current).canonicalFilePath();
                if (canonical.isEmpty() || !isWithin(gameRoot, QDir::cleanPath(canonical)))
                {
                    fail(QStringLiteral("Content directory resolves outside the instance."));
                }
            }
        }

        QString filePath(const QString& directory, const QString& filename, bool enabled)
        {
            return QDir(directory).filePath(enabled ? filename
                                                    : filename + QStringLiteral(".disabled"));
        }

        bool sameEntry(const InstalledContent& left, const InstalledContent& right)
        {
            return left.type == right.type && left.projectId == right.projectId &&
                   left.versionId == right.versionId && left.filename == right.filename &&
                   left.worldName == right.worldName;
        }

        QJsonObject toJson(const InstalledContent& entry)
        {
            return {{QStringLiteral("type"), contentTypeKey(entry.type)},
                    {QStringLiteral("projectId"), entry.projectId},
                    {QStringLiteral("slug"), entry.slug},
                    {QStringLiteral("title"), entry.title},
                    {QStringLiteral("versionId"), entry.versionId},
                    {QStringLiteral("versionNumber"), entry.versionNumber},
                    {QStringLiteral("worldName"), entry.worldName},
                    {QStringLiteral("filename"), entry.filename},
                    {QStringLiteral("installedAt"), entry.installedAt},
                    {QStringLiteral("enabled"), entry.enabled}};
        }

        InstalledContent fromJson(const QJsonObject& value)
        {
            const QString type = value.value(QStringLiteral("type")).toString();
            InstalledContent entry;
            entry.type = parseContentType(type);
            entry.projectId = value.value(QStringLiteral("projectId")).toString();
            entry.slug = value.value(QStringLiteral("slug")).toString();
            entry.title = value.value(QStringLiteral("title")).toString();
            entry.versionId = value.value(QStringLiteral("versionId")).toString();
            entry.versionNumber = value.value(QStringLiteral("versionNumber")).toString();
            entry.worldName = value.value(QStringLiteral("worldName")).toString();
            entry.filename = value.value(QStringLiteral("filename")).toString();
            entry.installedAt = value.value(QStringLiteral("installedAt")).toString();
            const QJsonValue enabled = value.value(QStringLiteral("enabled"));
            if (!enabled.isUndefined() && !enabled.isBool())
            {
                fail(QStringLiteral("Instance content state has an invalid enabled flag."));
            }
            entry.enabled = enabled.toBool(true);
            const bool validFilename =
                !entry.filename.trimmed().isEmpty() && entry.filename != QStringLiteral(".") &&
                entry.filename != QStringLiteral("..") &&
                !entry.filename.contains(QLatin1Char('/')) &&
                !entry.filename.contains(QLatin1Char('\\')) &&
                std::none_of(entry.filename.cbegin(), entry.filename.cend(), [](QChar character)
                             { return character.isNull() || !character.isPrint(); });
            if (entry.projectId.isEmpty() || entry.versionId.isEmpty() || !validFilename ||
                (entry.type == ContentType::DataPack && !safeWorldName(entry.worldName)) ||
                (entry.type != ContentType::DataPack && !entry.worldName.isEmpty()))
            {
                fail(QStringLiteral("Instance content state contains an invalid entry."));
            }
            return entry;
        }

        bool dependencyMatches(const InstalledContent& entry, const VersionDependency& dependency)
        {
            if (!entry.enabled)
            {
                return false;
            }
            return dependency.versionId.isEmpty() ? entry.projectId == dependency.projectId
                                                  : entry.versionId == dependency.versionId;
        }

        void validateDependencies(const QList<InstalledContent>& installed,
                                  const ProjectVersion& version)
        {
            for (const VersionDependency& dependency : version.dependencies)
            {
                const bool present = std::any_of(installed.cbegin(), installed.cend(),
                                                 [&dependency](const InstalledContent& entry)
                                                 { return dependencyMatches(entry, dependency); });
                if (dependency.dependencyType == QStringLiteral("required") && !present)
                {
                    const QString label = dependency.projectId.isEmpty() ? dependency.versionId
                                                                         : dependency.projectId;
                    fail(QStringLiteral("Required Modrinth dependency %1 must be installed first.")
                             .arg(label));
                }
                if (dependency.dependencyType == QStringLiteral("incompatible") && present)
                {
                    const QString label = dependency.projectId.isEmpty() ? dependency.versionId
                                                                         : dependency.projectId;
                    fail(QStringLiteral(
                             "This Modrinth version is incompatible with installed content %1.")
                             .arg(label));
                }
                if (dependency.dependencyType != QStringLiteral("required") &&
                    dependency.dependencyType != QStringLiteral("optional") &&
                    dependency.dependencyType != QStringLiteral("incompatible") &&
                    dependency.dependencyType != QStringLiteral("embedded"))
                {
                    fail(QStringLiteral("Modrinth returned an unsupported dependency type."));
                }
            }
        }
    } // namespace

    ContentManager::ContentManager(const ModrinthClient& client,
                                   const download::DownloadManager& downloadManager)
        : m_client(client), m_downloadManager(downloadManager)
    {
    }

    QList<InstalledContent> ContentManager::installed(const Instance& instance,
                                                      ContentType type) const
    {
        const QList<InstalledContent> entries = readState(instance);
        QList<InstalledContent> result;
        for (const InstalledContent& entry : entries)
        {
            if (entry.type == type)
            {
                result.append(entry);
            }
        }
        return result;
    }

    QStringList ContentManager::worlds(const Instance& instance) const
    {
        const QString savesDirectory =
            QDir(instance.gameDirectory()).filePath(QStringLiteral("saves"));
        const QFileInfo savesInfo(savesDirectory);
        if (!savesInfo.exists())
        {
            return {};
        }
        if (savesInfo.isSymLink() || !savesInfo.isDir())
        {
            fail(QStringLiteral("Instance saves directory is unsafe."));
        }
        QStringList result;
        const QFileInfoList worlds =
            QDir(savesDirectory).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& world : worlds)
        {
            if (!world.isSymLink() && safeWorldName(world.fileName()))
            {
                result.append(world.fileName());
            }
        }
        std::sort(result.begin(), result.end(), [](const QString& left, const QString& right)
                  { return QString::compare(left, right, Qt::CaseInsensitive) < 0; });
        return result;
    }

    QList<DependencyInstall>
    ContentManager::resolveRequiredDependencies(const Instance& instance, loader::LoaderType loader,
                                                const Project& project,
                                                const ProjectVersion& version) const
    {
        const QList<InstalledContent> existing = readState(instance);
        QList<DependencyInstall> plan;
        QHash<QString, QString> plannedVersions;
        QSet<QString> visitingProjects;

        const auto isInstalled = [&existing](const VersionDependency& dependency)
        {
            return std::any_of(existing.cbegin(), existing.cend(),
                               [&dependency](const InstalledContent& entry)
                               { return dependencyMatches(entry, dependency); });
        };
        const auto resolveDependency =
            [this, &instance, loader](const VersionDependency& dependency)
        {
            ProjectVersion resolvedVersion;
            Project resolvedProject;
            if (!dependency.versionId.isEmpty())
            {
                resolvedVersion = m_client.version(dependency.versionId);
                if (!dependency.projectId.isEmpty() &&
                    resolvedVersion.projectId != dependency.projectId)
                {
                    fail(QStringLiteral("Modrinth dependency version does not match its project."));
                }
                resolvedProject = m_client.project(resolvedVersion.projectId);
            }
            else
            {
                resolvedProject = m_client.project(dependency.projectId);
                const ContentType type = parseContentType(resolvedProject.projectType);
                const QList<ProjectVersion> versions =
                    m_client.versions(resolvedProject.id, instance.versionId, loader, type);
                if (versions.isEmpty())
                {
                    fail(QStringLiteral(
                             "No compatible version of required dependency %1 is available.")
                             .arg(resolvedProject.title));
                }
                resolvedVersion = versions.constFirst();
            }

            const ContentType type = parseContentType(resolvedProject.projectType);
            if (resolvedVersion.projectId != resolvedProject.id ||
                resolvedVersion.projectType != resolvedProject.projectType ||
                !resolvedVersion.gameVersions.contains(instance.versionId))
            {
                fail(QStringLiteral(
                         "Required dependency %1 has no compatible version for Minecraft %2.")
                         .arg(resolvedProject.title, instance.versionId));
            }
            if (type == ContentType::Mod && (loader == loader::LoaderType::Vanilla ||
                                             !resolvedVersion.loaders.contains(loaderKey(loader))))
            {
                fail(QStringLiteral(
                         "Required mod dependency %1 is incompatible with this instance's loader.")
                         .arg(resolvedProject.title));
            }
            return DependencyInstall{type, resolvedProject, resolvedVersion};
        };

        std::function<void(const Project&, const ProjectVersion&)> visit;
        visit = [&](const Project& currentProject, const ProjectVersion& currentVersion)
        {
            if (visitingProjects.contains(currentProject.id))
            {
                fail(QStringLiteral("Modrinth required dependencies contain a cycle at %1.")
                         .arg(currentProject.title));
            }
            visitingProjects.insert(currentProject.id);
            for (const VersionDependency& dependency : currentVersion.dependencies)
            {
                if (dependency.dependencyType != QStringLiteral("required") ||
                    isInstalled(dependency))
                {
                    continue;
                }

                const DependencyInstall resolved = resolveDependency(dependency);
                const auto planned = plannedVersions.constFind(resolved.project.id);
                if (planned != plannedVersions.cend())
                {
                    if (*planned != resolved.version.id)
                    {
                        fail(QStringLiteral("Conflicting required versions were found for %1.")
                                 .arg(resolved.project.title));
                    }
                    continue;
                }
                if (visitingProjects.contains(resolved.project.id))
                {
                    fail(QStringLiteral("Modrinth required dependencies contain a cycle at %1.")
                             .arg(resolved.project.title));
                }

                visit(resolved.project, resolved.version);
                plannedVersions.insert(resolved.project.id, resolved.version.id);
                plan.append(resolved);
            }
            visitingProjects.remove(currentProject.id);
        };

        visit(project, version);
        return plan;
    }

    InstalledContent ContentManager::install(const Instance& instance, ContentType type,
                                             loader::LoaderType loader, const QString& worldName,
                                             const Project& project, const ProjectVersion& version,
                                             ProgressCallback progress) const
    {
        if (project.id.isEmpty() || project.id != version.projectId ||
            project.projectType != projectType(type) || version.projectType != projectType(type))
        {
            fail(QStringLiteral(
                "Selected Modrinth version does not match the project and content category."));
        }
        if (!version.gameVersions.contains(instance.versionId))
        {
            fail(QStringLiteral("Selected Modrinth version is not compatible with Minecraft %1.")
                     .arg(instance.versionId));
        }
        if (type == ContentType::Mod &&
            (loader == loader::LoaderType::Vanilla || !version.loaders.contains(loaderKey(loader))))
        {
            fail(QStringLiteral(
                "Selected mod version is not compatible with this instance's installed loader."));
        }
        if (type == ContentType::DataPack && !worlds(instance).contains(worldName))
        {
            fail(QStringLiteral("Select an existing world before installing a data pack."));
        }
        const QList<InstalledContent> existing = readState(instance);
        validateDependencies(existing, version);
        const VersionFile& file = version.primaryFile();
        validateFile(file);

        const QString gameRoot = canonicalGameRoot(instance);
        const QString directory = targetDirectory(instance, type, worldName);
        ensureSafeDirectory(gameRoot, directory);
        const QString normalizedDirectory =
            QDir::cleanPath(QFileInfo(directory).absoluteFilePath());
        const QString target = QDir(normalizedDirectory).filePath(file.filename);
        const QString normalizedTarget = QDir::cleanPath(QFileInfo(target).absoluteFilePath());
        if (!isWithin(normalizedDirectory, normalizedTarget) || QFileInfo::exists(target) ||
            QFileInfo(target).isSymLink())
        {
            fail(QStringLiteral(
                "A content file with this name already exists in the selected instance."));
        }
        download::DownloadTask task{
            QUrl(file.url), target, file.sha1,
            QStringLiteral("%1 %2").arg(project.title, version.versionNumber)};
        if (file.size > 0)
        {
            task.maxBytes = file.size;
        }
        m_downloadManager.downloadAll({task}, std::move(progress));

        InstalledContent entry{type,
                               project.id,
                               project.slug,
                               project.title,
                               version.id,
                               version.versionNumber,
                               type == ContentType::DataPack ? worldName : QString(),
                               file.filename,
                               QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
                               true};
        QList<InstalledContent> current = readState(instance);
        current.append(entry);
        try
        {
            writeState(instance, current);
        }
        catch (const std::exception& exception)
        {
            if (QFileInfo::exists(target) && !QFile::remove(target))
            {
                fail(QStringLiteral("%1; installation rollback could not remove %2.")
                         .arg(QString::fromUtf8(exception.what()), target));
            }
            throw;
        }
        return entry;
    }

    void ContentManager::setEnabled(const Instance& instance, const InstalledContent& content,
                                    bool enabled) const
    {
        QList<InstalledContent> entries = readState(instance);
        auto found =
            std::find_if(entries.begin(), entries.end(), [&content](const InstalledContent& entry)
                         { return sameEntry(entry, content); });
        if (found == entries.end())
        {
            fail(QStringLiteral("Installed content entry was not found."));
        }
        if (found->enabled == enabled)
        {
            return;
        }
        const QString root = canonicalGameRoot(instance);
        const QString directory = targetDirectory(instance, found->type, found->worldName);
        const QFileInfo directoryInfo(directory);
        if (!directoryInfo.exists())
        {
            fail(QStringLiteral("Installed content directory no longer exists."));
        }
        ensureSafeDirectory(root, directory);
        const QString source = filePath(directory, found->filename, found->enabled);
        const QString destination = filePath(directory, found->filename, enabled);
        const QFileInfo sourceInfo(source);
        if (!sourceInfo.exists() || sourceInfo.isSymLink() || QFileInfo::exists(destination))
        {
            fail(QStringLiteral(
                "Installed content file is missing or its target name is already in use."));
        }
        if (!QFile::rename(source, destination))
        {
            fail(QStringLiteral("Could not change the enabled state of the content file."));
        }
        found->enabled = enabled;
        try
        {
            writeState(instance, entries);
        }
        catch (const std::exception& exception)
        {
            if (!QFile::rename(destination, source))
            {
                fail(QStringLiteral("%1; rollback could not restore %2.")
                         .arg(QString::fromUtf8(exception.what()), source));
            }
            throw;
        }
    }

    void ContentManager::remove(const Instance& instance, const InstalledContent& content) const
    {
        QList<InstalledContent> entries = readState(instance);
        const auto found =
            std::find_if(entries.cbegin(), entries.cend(), [&content](const InstalledContent& entry)
                         { return sameEntry(entry, content); });
        if (found == entries.cend())
        {
            fail(QStringLiteral("Installed content entry was not found."));
        }
        const QString root = canonicalGameRoot(instance);
        const QString directory = targetDirectory(instance, found->type, found->worldName);
        const QFileInfo directoryInfo(directory);
        if (directoryInfo.exists())
        {
            ensureSafeDirectory(root, directory);
        }
        const QString path = filePath(directory, found->filename, found->enabled);
        const QFileInfo fileInfo(path);
        if (fileInfo.exists() && (fileInfo.isSymLink() || !fileInfo.isFile()))
        {
            fail(QStringLiteral("Refusing to remove an unsafe content file."));
        }

        QString quarantinePath;
        if (fileInfo.exists())
        {
            quarantinePath = path + QStringLiteral(".removing");
            if (QFileInfo::exists(quarantinePath) || !QFile::rename(path, quarantinePath))
            {
                fail(QStringLiteral("Could not stage the content file for removal."));
            }
        }
        entries.removeAt(static_cast<qsizetype>(std::distance(entries.cbegin(), found)));
        try
        {
            writeState(instance, entries);
        }
        catch (const std::exception& exception)
        {
            if (!quarantinePath.isEmpty() && !QFile::rename(quarantinePath, path))
            {
                fail(QStringLiteral("%1; rollback could not restore %2.")
                         .arg(QString::fromUtf8(exception.what()), path));
            }
            throw;
        }
        if (!quarantinePath.isEmpty() && !QFile::remove(quarantinePath))
        {
            fail(
                QStringLiteral("Content was untracked but its staged file could not be deleted: %1")
                    .arg(quarantinePath));
        }
    }

    void ContentManager::validateFile(const VersionFile& file)
    {
        ModrinthClient::validateFile(file);
    }

    QString ContentManager::targetDirectory(const Instance& instance, ContentType type,
                                            const QString& worldName)
    {
        QString relative = directoryName(type);
        if (type == ContentType::DataPack)
        {
            if (!safeWorldName(worldName))
            {
                fail(QStringLiteral("Select a world before installing a data pack."));
            }
            relative = QStringLiteral("saves/%1/datapacks").arg(worldName);
        }
        return QDir(instance.gameDirectory()).filePath(relative);
    }

    QList<InstalledContent> ContentManager::readState(const Instance& instance) const
    {
        const QString path = QDir(instance.gameDirectory()).filePath(kStateFile);
        const QFileInfo info(path);
        if (!info.exists())
        {
            return {};
        }
        if (info.isSymLink() || !info.isFile() || info.size() > kMaxStateBytes)
        {
            fail(QStringLiteral("Instance content state file is unsafe or too large."));
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            fail(QStringLiteral("Could not read instance content state."));
        }
        QJsonParseError error{};
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
        {
            fail(QStringLiteral("Instance content state is invalid JSON."));
        }
        const QJsonValue entriesValue = document.object().value(QStringLiteral("entries"));
        if (!entriesValue.isArray())
        {
            fail(QStringLiteral("Instance content state is incomplete."));
        }
        QList<InstalledContent> entries;
        for (const QJsonValue& value : entriesValue.toArray())
        {
            if (!value.isObject())
            {
                fail(QStringLiteral("Instance content state contains an invalid entry."));
            }
            entries.append(fromJson(value.toObject()));
        }
        return entries;
    }

    void ContentManager::writeState(const Instance& instance,
                                    const QList<InstalledContent>& entries) const
    {
        const QString root = canonicalGameRoot(instance);
        QJsonArray jsonEntries;
        for (const InstalledContent& entry : entries)
        {
            jsonEntries.append(toJson(entry));
        }
        const QByteArray json = QJsonDocument(QJsonObject{{QStringLiteral("entries"), jsonEntries}})
                                    .toJson(QJsonDocument::Indented);
        if (json.size() > kMaxStateBytes)
        {
            fail(QStringLiteral("Instance content state exceeds the size limit."));
        }
        QSaveFile file(QDir(root).filePath(kStateFile));
        if (!file.open(QIODevice::WriteOnly) || file.write(json) != json.size() || !file.commit())
        {
            fail(QStringLiteral("Could not save instance content state."));
        }
    }
} // namespace strata::modrinth
