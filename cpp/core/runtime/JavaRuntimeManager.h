#pragma once

#include "../download/DownloadManager.h"
#include "../instance/Instance.h"

#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QUrl>

namespace strata::runtime
{
    enum class RuntimeVendor
    {
        Mojang,
        Temurin,
        Zulu
    };

    struct RuntimePreferences
    {
        RuntimeVendor vendor = RuntimeVendor::Mojang;
        bool redundantRuntimes = false;
    };

    struct RuntimeOwner
    {
        QString name;
        QString directory;
        QString component;
        bool managed = false;
        bool redundant = false;
    };

    struct InstalledRuntime
    {
        QString directory;
        QString component;
        RuntimeVendor vendor = RuntimeVendor::Mojang;
        QString version;
        QStringList users;
    };

    struct RuntimeFile
    {
        enum class Type
        {
            File,
            Directory,
            Link
        };

        QString path;
        Type type = Type::File;
        QUrl url;
        QString sha1;
        bool executable = false;
        QString target;
    };

    class JavaRuntimeManager
    {
    public:
        using ProgressCallback = download::DownloadManager::ProgressCallback;

        JavaRuntimeManager(const download::DownloadManager& downloadManager, QString dataDirectory);

        RuntimePreferences preferences() const;
        void setPreferences(RuntimePreferences preferences) const;
        QString install(const strata::Instance& instance, const QJsonObject& versionMetadata,
                        ProgressCallback progress = {}) const;
        QString javaExecutableForInstaller(const strata::Instance& instance,
                                           const QJsonObject& versionMetadata,
                                           ProgressCallback progress = {}) const;
        QString install(const QString& ownerDirectory, const QString& versionId,
                        const QJsonObject& versionMetadata, RuntimeVendor vendor, bool redundant,
                        ProgressCallback progress = {}) const;
        QString runtimeComponent(RuntimeVendor vendor, const QJsonObject& metadata,
                                 const QString& versionId) const;
        QString javaExecutable(const strata::Instance& instance) const;
        QString javaExecutable(const QString& ownerDirectory, const QString& versionId,
                               const QString& component, bool redundant) const;
        QList<InstalledRuntime> listInstalledRuntimes(const QList<RuntimeOwner>& owners) const;
        void deleteRuntime(const InstalledRuntime& runtime,
                           const QList<RuntimeOwner>& owners) const;

        static QString requiredComponent(const QJsonObject& metadata, const QString& versionId);
        static int requiredJavaMajor(const QJsonObject& metadata, const QString& versionId);
        static QString platformName(const QString& osName, const QString& architecture);
        static QString vendorName(RuntimeVendor vendor);
        static RuntimeVendor parseVendor(const QString& name);
        static QString selectOfficialRuntime(const QJsonObject& index, const QString& platform,
                                             const QString& component,
                                             QJsonObject* selectedVersion = nullptr);
        static QList<RuntimeFile> parseRuntimeFiles(const QJsonObject& manifest);
        static QString runtimeDirectory(const QString& ownerDirectory, const QString& dataDirectory,
                                        const QString& component, bool redundant);
        static QString validateRuntimePath(const QString& root, const QString& relative);
        static void extractArchive(const QString& archivePath, const QString& destination);

    private:
        const download::DownloadManager& m_downloadManager;
        QString m_dataDirectory;

        QString installMojang(const QString& ownerDirectory, const QString& versionId,
                              const QJsonObject& metadata, bool redundant,
                              const ProgressCallback& progress) const;
        QString installExternal(const QString& ownerDirectory, const QString& versionId,
                                const QJsonObject& metadata, RuntimeVendor vendor, bool redundant,
                                const ProgressCallback& progress) const;
        QString fetchRuntimeMetadata(const QUrl& url, const QString& expectedSha1 = {}) const;
        QString managedRuntimeDirectory(const QString& ownerDirectory, const QString& component,
                                        bool redundant) const;
    };
} // namespace strata::runtime
