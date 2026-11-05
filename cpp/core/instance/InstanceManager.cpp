#include "InstanceManager.h"

#include "../Paths.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>
#include <stdexcept>

namespace strata
{
    namespace
    {
        [[noreturn]] void throwFileError(const QString& operation, const QString& path,
                                         const QString& detail = {})
        {
            const QString message =
                detail.isEmpty() ? QStringLiteral("%1: %2").arg(operation, path)
                                 : QStringLiteral("%1: %2 (%3)").arg(operation, path, detail);
            throw std::runtime_error(message.toStdString());
        }

        bool validInstanceId(const QString& id)
        {
            static const QRegularExpression pattern(QStringLiteral("^[0-9a-fA-F-]{36}$"));
            return pattern.match(id).hasMatch();
        }

        void copyDirectoryContents(const QString& sourcePath, const QString& targetPath)
        {
            const QFileInfo sourceInfo(sourcePath);
            if (sourceInfo.isSymLink() || !sourceInfo.isDir())
            {
                throwFileError(QStringLiteral("Instance game directory is missing"), sourcePath);
            }
            if (!QDir().mkpath(targetPath))
            {
                throwFileError(QStringLiteral("Unable to create directory"), targetPath);
            }
            const QDir source(sourcePath);
            const QFileInfoList entries = source.entryInfoList(
                QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
            for (const QFileInfo& entry : entries)
            {
                if (entry.isSymLink())
                {
                    continue;
                }
                const QString target = QDir(targetPath).filePath(entry.fileName());
                if (entry.isDir())
                {
                    copyDirectoryContents(entry.absoluteFilePath(), target);
                }
                else if (entry.isFile() && !QFile::copy(entry.absoluteFilePath(), target))
                {
                    throwFileError(QStringLiteral("Unable to copy file"), entry.absoluteFilePath());
                }
            }
        }

        void removeTree(const QString& path)
        {
            const QFileInfo info(path);
            if (!info.exists() && !info.isSymLink())
            {
                return;
            }
            if (info.isSymLink() || info.isFile())
            {
                if (!QFile::remove(path))
                {
                    throwFileError(QStringLiteral("Unable to remove file"), path);
                }
                return;
            }

            const QDir directory(path);
            const QFileInfoList entries = directory.entryInfoList(
                QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
            for (const QFileInfo& entry : entries)
            {
                removeTree(entry.absoluteFilePath());
            }
            if (!QDir().rmdir(path))
            {
                throwFileError(QStringLiteral("Unable to remove directory"), path);
            }
        }
    } // namespace

    InstanceManager::InstanceManager(const QString& dataDirectory)
    {
        m_dataDirectory = QDir::cleanPath(
            QFileInfo(dataDirectory.isEmpty() ? Paths::dataDirectory() : dataDirectory)
                .absoluteFilePath());
        m_instancesDirectory = QDir(m_dataDirectory).filePath(QStringLiteral("instances"));
    }

    QString InstanceManager::dataDirectory() const
    {
        return m_dataDirectory;
    }

    QString InstanceManager::instancesDirectory() const
    {
        return m_instancesDirectory;
    }

    QList<Instance> InstanceManager::listInstances() const
    {
        if (!QDir().mkpath(m_instancesDirectory))
        {
            throwFileError(QStringLiteral("Unable to create directory"), m_instancesDirectory);
        }
        QList<Instance> instances;
        const QDir directory(m_instancesDirectory);
        const QFileInfoList entries =
            directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& entry : entries)
        {
            if (!entry.isSymLink())
            {
                instances.append(readInstance(entry.absoluteFilePath()));
            }
        }
        std::stable_sort(instances.begin(), instances.end(),
                         [](const Instance& left, const Instance& right)
                         {
                             if (left.favorite != right.favorite)
                             {
                                 return left.favorite;
                             }
                             return QString::compare(left.name, right.name, Qt::CaseInsensitive) <
                                    0;
                         });
        return instances;
    }

    Instance InstanceManager::create(const QString& name, const QString& versionId) const
    {
        const QString normalizedName = requireText(name, QStringLiteral("Instance name"));
        const QString normalizedVersion =
            requireText(versionId, QStringLiteral("Minecraft version"));
        if (!QDir().mkpath(m_instancesDirectory))
        {
            throwFileError(QStringLiteral("Unable to create directory"), m_instancesDirectory);
        }

        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString directory = QDir(m_instancesDirectory).filePath(id);
        if (!QDir().mkpath(QDir(directory).filePath(QStringLiteral("game"))))
        {
            throwFileError(QStringLiteral("Unable to create instance directory"), directory);
        }
        const Instance instance{
            id,        normalizedName, normalizedVersion, QDateTime::currentDateTimeUtc(),
            directory, false};
        try
        {
            writeMetadata(instance);
        }
        catch (...)
        {
            deleteTree(directory);
            throw;
        }
        return instance;
    }

    Instance InstanceManager::duplicate(const Instance& source, const QString& newName) const
    {
        const Instance storedSource = requireStoredInstance(source.id);
        const QString normalizedName = requireText(newName, QStringLiteral("Instance name"));
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString directory = QDir(m_instancesDirectory).filePath(id);
        if (!QDir().mkpath(directory))
        {
            throwFileError(QStringLiteral("Unable to create instance directory"), directory);
        }
        try
        {
            copyTree(storedSource.gameDirectory(),
                     QDir(directory).filePath(QStringLiteral("game")));
            const Instance copy{id,
                                normalizedName,
                                storedSource.versionId,
                                QDateTime::currentDateTimeUtc(),
                                directory,
                                storedSource.favorite};
            writeMetadata(copy);
            return copy;
        }
        catch (...)
        {
            deleteTree(directory);
            throw;
        }
    }

    Instance InstanceManager::rename(const Instance& instance, const QString& newName) const
    {
        const Instance stored = requireStoredInstance(instance.id);
        Instance renamed = stored;
        renamed.name = requireText(newName, QStringLiteral("Instance name"));
        writeMetadata(renamed);
        return renamed;
    }

    Instance InstanceManager::setFavorite(const Instance& instance, bool favorite) const
    {
        const Instance stored = requireStoredInstance(instance.id);
        Instance updated = stored;
        updated.favorite = favorite;
        writeMetadata(updated);
        return updated;
    }

    void InstanceManager::deleteInstance(const Instance& instance) const
    {
        const Instance stored = requireStoredInstance(instance.id);
        deleteTree(stored.directory);
    }

    Instance InstanceManager::requireStoredInstance(const QString& id) const
    {
        if (!validInstanceId(id))
        {
            throw std::invalid_argument("Invalid instance identifier");
        }
        const QString directory = QDir::cleanPath(QDir(m_instancesDirectory).filePath(id));
        if (QFileInfo(directory).path() != QDir::cleanPath(m_instancesDirectory) ||
            !QFileInfo(directory).isDir() || QFileInfo(directory).isSymLink())
        {
            throwFileError(QStringLiteral("Instance does not exist"), id);
        }
        return readInstance(directory);
    }

    Instance InstanceManager::readInstance(const QString& directory) const
    {
        const QString metadataPath = QDir(directory).filePath(QStringLiteral("instance.json"));
        QFile file(metadataPath);
        if (!file.open(QIODevice::ReadOnly))
        {
            throwFileError(QStringLiteral("Unable to read instance metadata"), metadataPath,
                           file.errorString());
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            throwFileError(QStringLiteral("Could not parse instance metadata"), metadataPath,
                           parseError.errorString());
        }
        const QJsonObject metadata = document.object();
        const QString id = metadata.value(QStringLiteral("id")).toString();
        const QString name = metadata.value(QStringLiteral("name")).toString();
        const QString versionId = metadata.value(QStringLiteral("versionId")).toString();
        const QString createdAtString = metadata.value(QStringLiteral("createdAt")).toString();
        const QDateTime createdAt = QDateTime::fromString(createdAtString, Qt::ISODateWithMs);
        const QString directoryName = QFileInfo(directory).fileName();
        if (id.isEmpty() || id != directoryName || name.isNull() || versionId.isNull() ||
            !createdAt.isValid())
        {
            throwFileError(QStringLiteral("Instance metadata is incomplete or invalid"),
                           metadataPath);
        }
        return {id,
                name,
                versionId,
                createdAt.toUTC(),
                QDir::cleanPath(QFileInfo(directory).absoluteFilePath()),
                metadata.value(QStringLiteral("favorite")).toBool(false)};
    }

    void InstanceManager::writeMetadata(const Instance& instance) const
    {
        QJsonObject metadata;
        metadata.insert(QStringLiteral("id"), instance.id);
        metadata.insert(QStringLiteral("name"), instance.name);
        metadata.insert(QStringLiteral("versionId"), instance.versionId);
        metadata.insert(QStringLiteral("createdAt"),
                        instance.createdAt.toUTC().toString(Qt::ISODateWithMs));
        metadata.insert(QStringLiteral("favorite"), instance.favorite);

        const QString metadataPath =
            QDir(instance.directory).filePath(QStringLiteral("instance.json"));
        QSaveFile file(metadataPath);
        if (!file.open(QIODevice::WriteOnly))
        {
            throwFileError(QStringLiteral("Unable to write instance metadata"), metadataPath,
                           file.errorString());
        }
        const QByteArray json = QJsonDocument(metadata).toJson(QJsonDocument::Indented);
        if (file.write(json) != json.size() || !file.commit())
        {
            throwFileError(QStringLiteral("Unable to commit instance metadata"), metadataPath,
                           file.errorString());
        }
    }

    QString InstanceManager::requireText(const QString& value, const QString& label)
    {
        const QString normalized = value.trimmed();
        if (normalized.isEmpty())
        {
            throw std::invalid_argument(label.toStdString() + " must not be blank");
        }
        return normalized;
    }

    void InstanceManager::copyTree(const QString& source, const QString& target)
    {
        copyDirectoryContents(source, target);
    }

    void InstanceManager::deleteTree(const QString& target)
    {
        removeTree(target);
    }
} // namespace strata
