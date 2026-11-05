#include "MojangManifestClient.h"

#include "../Paths.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>

#include <algorithm>
#include <stdexcept>

namespace strata::mojang
{
    namespace
    {
        constexpr qsizetype maxManifestBytes = 16 * 1024 * 1024;
        constexpr int manifestTimeoutMs = 30'000;

        [[noreturn]] void throwManifestError(const QString& message)
        {
            throw std::runtime_error(message.toStdString());
        }

        QString requiredString(const QJsonObject& object, const QString& key)
        {
            const QJsonValue value = object.value(key);
            if (!value.isString() || value.toString().isEmpty())
            {
                throwManifestError(
                    QStringLiteral("Mojang version manifest is missing %1").arg(key));
            }
            return value.toString();
        }

        bool validVersionId(const QString& id)
        {
            static const QRegularExpression pattern(
                QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+() -]{0,127}$"));
            return pattern.match(id).hasMatch() && !id.endsWith(QLatin1Char(' ')) &&
                   !id.contains(QStringLiteral(".."));
        }

        bool validSha1(const QString& sha1)
        {
            static const QRegularExpression pattern(QStringLiteral("^[0-9a-fA-F]{40}$"));
            return sha1.isEmpty() || pattern.match(sha1).hasMatch();
        }
    } // namespace

    QString Version::displayName() const
    {
        return QStringLiteral("%1 (%2)").arg(id, type);
    }

    const Version& VersionManifest::latestReleaseVersion() const
    {
        const auto found =
            std::find_if(versions.cbegin(), versions.cend(),
                         [this](const Version& version) { return version.id == latestRelease; });
        if (found == versions.cend())
        {
            throw std::logic_error("Latest release is missing from the Mojang version list");
        }
        return *found;
    }

    QUrl MojangManifestClient::defaultManifestUrl()
    {
        return QUrl(
            QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"));
    }

    MojangManifestClient::MojangManifestClient(QString cacheFilePath, QUrl manifestUrl)
        : m_cacheFilePath(cacheFilePath.isEmpty()
                              ? QDir(Paths::dataDirectory())
                                    .filePath(QStringLiteral("cache/version_manifest_v2.json"))
                              : QFileInfo(cacheFilePath).absoluteFilePath()),
          m_manifestUrl(std::move(manifestUrl))
    {
        if (!m_manifestUrl.isValid() || (m_manifestUrl.scheme() != QStringLiteral("https") &&
                                         m_manifestUrl.scheme() != QStringLiteral("http")))
        {
            throw std::invalid_argument("Mojang manifest URL must use HTTP or HTTPS");
        }
    }

    VersionManifest MojangManifestClient::fetchManifest() const
    {
        QNetworkAccessManager manager;
        QNetworkRequest request(m_manifestUrl);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("StrataLauncher/0.1.0"));
        request.setRawHeader("Accept", "application/json");
        request.setTransferTimeout(manifestTimeoutMs);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);

        QNetworkReply* reply = manager.get(request);
        QByteArray response;
        QString readError;
        QObject::connect(reply, &QIODevice::readyRead, reply,
                         [&]()
                         {
                             const QByteArray chunk = reply->readAll();
                             if (response.size() > maxManifestBytes - chunk.size())
                             {
                                 readError = QStringLiteral(
                                     "Mojang version manifest exceeds the 16 MiB response limit");
                                 reply->abort();
                                 return;
                             }
                             response.append(chunk);
                         });

        QEventLoop eventLoop;
        QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
        eventLoop.exec();
        const QByteArray remaining = reply->readAll();
        if (readError.isEmpty())
        {
            if (response.size() > maxManifestBytes - remaining.size())
            {
                readError =
                    QStringLiteral("Mojang version manifest exceeds the 16 MiB response limit");
            }
            else
            {
                response.append(remaining);
            }
        }
        const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString networkError = reply->errorString();
        const bool networkFailed = reply->error() != QNetworkReply::NoError;
        reply->deleteLater();

        if (!readError.isEmpty())
        {
            throwManifestError(readError);
        }
        if (networkFailed || statusCode < 200 || statusCode >= 300)
        {
            try
            {
                VersionManifest cached = readCache();
                cached.usedCache = true;
                return cached;
            }
            catch (const std::exception& cacheError)
            {
                const QString reason =
                    networkFailed ? networkError : QStringLiteral("HTTP %1").arg(statusCode);
                throwManifestError(
                    QStringLiteral(
                        "Mojang version manifest request failed (%1); no valid cached manifest: %2")
                        .arg(reason, QString::fromUtf8(cacheError.what())));
            }
        }

        const VersionManifest manifest = parseManifest(response);
        writeCache(response);
        return manifest;
    }

    VersionManifest MojangManifestClient::parseManifest(const QByteArray& json)
    {
        if (json.size() > maxManifestBytes)
        {
            throwManifestError(
                QStringLiteral("Mojang version manifest exceeds the 16 MiB response limit"));
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            throwManifestError(QStringLiteral("Could not parse Mojang version manifest: %1")
                                   .arg(parseError.errorString()));
        }

        const QJsonObject root = document.object();
        const QJsonObject latest = root.value(QStringLiteral("latest")).toObject();
        const QString latestRelease = requiredString(latest, QStringLiteral("release"));
        const QString latestSnapshot = requiredString(latest, QStringLiteral("snapshot"));
        const QJsonValue versionsValue = root.value(QStringLiteral("versions"));
        if (!versionsValue.isArray())
        {
            throwManifestError(QStringLiteral("Mojang version manifest has no versions list"));
        }

        QList<Version> versions;
        for (const QJsonValue& value : versionsValue.toArray())
        {
            if (!value.isObject())
            {
                throwManifestError(
                    QStringLiteral("Mojang version manifest contains an invalid version entry"));
            }
            const QJsonObject item = value.toObject();
            const QString id = requiredString(item, QStringLiteral("id"));
            const QString type = requiredString(item, QStringLiteral("type"));
            const QUrl url(requiredString(item, QStringLiteral("url")));
            const QString timestamp = requiredString(item, QStringLiteral("releaseTime"));
            const QDateTime releaseTime = QDateTime::fromString(timestamp, Qt::ISODateWithMs);
            const QJsonValue sha1Value = item.value(QStringLiteral("sha1"));
            if (!sha1Value.isUndefined() && !sha1Value.isNull() && !sha1Value.isString())
            {
                throwManifestError(
                    QStringLiteral("Mojang version manifest contains an invalid SHA-1 for %1")
                        .arg(id));
            }
            const QString sha1 = sha1Value.toString();
            if (!validVersionId(id) || !url.isValid() ||
                (url.scheme() != QStringLiteral("https") &&
                 url.scheme() != QStringLiteral("http")) ||
                !releaseTime.isValid() || !validSha1(sha1))
            {
                throwManifestError(
                    QStringLiteral("Mojang version manifest contains invalid metadata for %1")
                        .arg(id));
            }
            versions.append({id, type, url, releaseTime.toUTC(), sha1.toLower()});
        }
        if (std::none_of(versions.cbegin(), versions.cend(),
                         [&latestRelease](const Version& version)
                         { return version.id == latestRelease; }))
        {
            throwManifestError(
                QStringLiteral("Latest release is missing from the Mojang version list"));
        }
        return {latestRelease, latestSnapshot, versions, false};
    }

    VersionManifest MojangManifestClient::readCache() const
    {
        QFile file(m_cacheFilePath);
        if (!file.open(QIODevice::ReadOnly))
        {
            throwManifestError(QStringLiteral("Unable to read manifest cache %1: %2")
                                   .arg(m_cacheFilePath, file.errorString()));
        }
        if (file.size() > maxManifestBytes)
        {
            throwManifestError(
                QStringLiteral("Cached Mojang manifest exceeds the 16 MiB response limit"));
        }
        VersionManifest cached = parseManifest(file.readAll());
        cached.usedCache = true;
        return cached;
    }

    void MojangManifestClient::writeCache(const QByteArray& json) const
    {
        const QString parentDirectory = QFileInfo(m_cacheFilePath).absolutePath();
        if (!QDir().mkpath(parentDirectory))
        {
            throwManifestError(QStringLiteral("Unable to create manifest cache directory %1")
                                   .arg(parentDirectory));
        }
        QSaveFile file(m_cacheFilePath);
        if (!file.open(QIODevice::WriteOnly))
        {
            throwManifestError(QStringLiteral("Unable to write manifest cache %1: %2")
                                   .arg(m_cacheFilePath, file.errorString()));
        }
        if (file.write(json) != json.size() || !file.commit())
        {
            throwManifestError(QStringLiteral("Unable to commit manifest cache %1: %2")
                                   .arg(m_cacheFilePath, file.errorString()));
        }
    }
} // namespace strata::mojang
