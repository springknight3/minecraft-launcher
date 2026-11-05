#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QUrl>

namespace strata::mojang
{
    struct Version
    {
        QString id;
        QString type;
        QUrl url;
        QDateTime releaseTime;
        QString sha1;

        QString displayName() const;
        bool operator==(const Version&) const = default;
    };

    struct VersionManifest
    {
        QString latestRelease;
        QString latestSnapshot;
        QList<Version> versions;
        bool usedCache = false;

        const Version& latestReleaseVersion() const;
    };

    class MojangManifestClient
    {
    public:
        static QUrl defaultManifestUrl();

        explicit MojangManifestClient(QString cacheFilePath = QString(),
                                      QUrl manifestUrl = defaultManifestUrl());

        VersionManifest fetchManifest() const;
        static VersionManifest parseManifest(const QByteArray& json);

    private:
        QString m_cacheFilePath;
        QUrl m_manifestUrl;

        VersionManifest readCache() const;
        void writeCache(const QByteArray& json) const;
    };
} // namespace strata::mojang
