#include "OfflineAccountStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

#include <stdexcept>

namespace strata::auth
{
    OfflineAccountStore::OfflineAccountStore(QString dataDirectory)
        : m_accountsFile(QDir(QFileInfo(dataDirectory).absoluteFilePath())
                             .filePath(QStringLiteral("offline-profile.json")))
    {
    }

    QString OfflineAccountStore::username() const
    {
        QFile file(m_accountsFile);
        if (!file.exists())
        {
            return {};
        }
        if (!file.open(QIODevice::ReadOnly))
        {
            throw std::runtime_error(QStringLiteral("Unable to read offline account: %1")
                                         .arg(file.errorString())
                                         .toStdString());
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            throw std::runtime_error("Offline account data is invalid");
        }
        const QJsonObject profile = document.object().value(QStringLiteral("offline")).toObject();
        if (profile.isEmpty())
        {
            return {};
        }
        const QString name = validateUsername(profile.value(QStringLiteral("name")).toString());
        const QString uuid = profile.value(QStringLiteral("uuid")).toString();
        if (uuid != offlineUuid(name))
        {
            throw std::runtime_error("Offline account identifier does not match its player name");
        }
        return name;
    }

    OfflineAccount OfflineAccountStore::setUsername(const QString& name) const
    {
        const QString normalized = validateUsername(name);
        const QString parentDirectory = QFileInfo(m_accountsFile).absolutePath();
        if (!QDir().mkpath(parentDirectory))
        {
            throw std::runtime_error(QStringLiteral("Unable to create account directory: %1")
                                         .arg(parentDirectory)
                                         .toStdString());
        }
        const QJsonObject profile{{QStringLiteral("name"), normalized},
                                  {QStringLiteral("uuid"), offlineUuid(normalized)}};
        QSaveFile file(m_accountsFile);
        if (!file.open(QIODevice::WriteOnly))
        {
            throw std::runtime_error(QStringLiteral("Unable to save offline account: %1")
                                         .arg(file.errorString())
                                         .toStdString());
        }
        const QByteArray json = QJsonDocument(QJsonObject{{QStringLiteral("offline"), profile}})
                                    .toJson(QJsonDocument::Indented);
        if (file.write(json) != json.size() || !file.commit())
        {
            throw std::runtime_error(QStringLiteral("Unable to commit offline account: %1")
                                         .arg(file.errorString())
                                         .toStdString());
        }
        return {normalized, offlineUuid(normalized)};
    }

    QString OfflineAccountStore::validateUsername(const QString& name)
    {
        const QString normalized = name.trimmed();
        static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_]{1,16}$"));
        if (!pattern.match(normalized).hasMatch())
        {
            throw std::invalid_argument(
                "Offline player name must be 1-16 letters, numbers, or underscores");
        }
        return normalized;
    }

    QString OfflineAccountStore::offlineUuid(const QString& name)
    {
        const QString normalized = validateUsername(name);
        QByteArray bytes = QCryptographicHash::hash(
            QByteArrayLiteral("OfflinePlayer:") + normalized.toUtf8(), QCryptographicHash::Md5);
        bytes[6] = static_cast<char>((static_cast<unsigned char>(bytes.at(6)) & 0x0fU) | 0x30U);
        bytes[8] = static_cast<char>((static_cast<unsigned char>(bytes.at(8)) & 0x3fU) | 0x80U);
        return QUuid::fromRfc4122(bytes).toString(QUuid::WithoutBraces);
    }
} // namespace strata::auth
