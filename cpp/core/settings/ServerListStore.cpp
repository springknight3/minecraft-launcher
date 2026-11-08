#include "ServerListStore.h"

#include <QFileInfo>
#include <QRegularExpression>

#include <stdexcept>

namespace strata::settings
{
    namespace
    {
        [[noreturn]] void throwServerError(const QString& message)
        {
            throw std::runtime_error(message.toStdString());
        }

        QString requiredText(const QString& text, const QString& label)
        {
            const QString normalized = text.trimmed();
            if (normalized.isEmpty())
            {
                throw std::invalid_argument(
                    QStringLiteral("%1 cannot be empty.").arg(label).toStdString());
            }
            return normalized;
        }

        bool usesCompressedNbt(const QString& version)
        {
            static const QRegularExpression majorPattern(QStringLiteral("^(\\d+)\\."));
            const QRegularExpressionMatch match = majorPattern.match(version);
            return !match.hasMatch() || match.captured(1).toInt() < 26;
        }

        const NbtCompound& requireCompound(const NbtTag& tag, const QString& label)
        {
            const auto* compound = std::get_if<std::shared_ptr<NbtCompound>>(&tag.value);
            if (tag.type != 10 || compound == nullptr || !*compound)
            {
                throwServerError(
                    QStringLiteral("servers.dat contains an invalid %1; it was not modified.")
                        .arg(label));
            }
            return **compound;
        }

        QString stringValue(const NbtCompound& compound, const QString& key)
        {
            const NbtTag* tag = compound.find(key);
            if (tag == nullptr || tag->type != 8)
            {
                return {};
            }
            const auto* value = std::get_if<QString>(&tag->value);
            return value == nullptr ? QString() : *value;
        }
    } // namespace

    ServerListStore::ServerListStore(const QString& file, const QString& minecraftVersion)
        : m_file(QFileInfo(file).absoluteFilePath())
    {
        const bool existed = QFileInfo(m_file).isFile();
        const bool existingCompression = existed && NbtFile::isCompressed(m_file);
        if (existed)
        {
            m_document = NbtFile::read(m_file);
        }
        if (minecraftVersion.isNull() || minecraftVersion.isEmpty())
        {
            m_compressedOnSave = existed ? existingCompression : true;
        }
        else
        {
            m_compressedOnSave = usesCompressedNbt(minecraftVersion);
        }
        static_cast<void>(servers());
        if (existed && existingCompression != m_compressedOnSave)
        {
            save();
        }
    }

    QVector<ServerListStore::Server> ServerListStore::servers() const
    {
        QVector<Server> result;
        for (const NbtTag& tag : serverTags())
        {
            const NbtCompound& server = requireCompound(tag, QStringLiteral("server list entry"));
            result.append({stringValue(server, QStringLiteral("name")),
                           stringValue(server, QStringLiteral("ip"))});
        }
        return result;
    }

    void ServerListStore::add(const QString& name, const QString& address)
    {
        QVector<NbtTag> entries = serverTags();
        auto fields = std::make_shared<NbtCompound>();
        fields->insert(QStringLiteral("name"),
                       {8, requiredText(name, QStringLiteral("Server name"))});
        fields->insert(QStringLiteral("ip"),
                       {8, requiredText(address, QStringLiteral("Server address"))});
        entries.append({10, fields});
        replaceServerTags(entries);
    }

    void ServerListStore::update(qsizetype index, const QString& name, const QString& address)
    {
        QVector<NbtTag> entries = serverTags();
        if (index < 0 || index >= entries.size())
        {
            throw std::out_of_range("Server list index is out of range.");
        }
        const QString normalizedName = requiredText(name, QStringLiteral("Server name"));
        const QString normalizedAddress = requiredText(address, QStringLiteral("Server address"));
        const auto* stored = std::get_if<std::shared_ptr<NbtCompound>>(&entries.at(index).value);
        if (entries.at(index).type != 10 || stored == nullptr || !*stored)
        {
            throwServerError(QStringLiteral(
                "servers.dat contains an invalid server list entry; it was not modified."));
        }
        auto fields = std::make_shared<NbtCompound>(**stored);
        fields->insert(QStringLiteral("name"), {8, normalizedName});
        fields->insert(QStringLiteral("ip"), {8, normalizedAddress});
        entries[index] = {10, fields};
        replaceServerTags(entries);
    }

    void ServerListStore::remove(qsizetype index)
    {
        QVector<NbtTag> entries = serverTags();
        if (index < 0 || index >= entries.size())
        {
            throw std::out_of_range("Server list index is out of range.");
        }
        entries.removeAt(index);
        replaceServerTags(entries);
    }

    void ServerListStore::save() const
    {
        NbtFile::write(m_file, m_document, m_compressedOnSave);
    }

    QVector<NbtTag> ServerListStore::serverTags() const
    {
        const NbtTag* tag = m_document.root.find(QStringLiteral("servers"));
        if (tag == nullptr)
        {
            return {};
        }
        const auto* list = std::get_if<std::shared_ptr<NbtList>>(&tag->value);
        if (tag->type != 9 || list == nullptr || !*list || (*list)->elementType != 10)
        {
            throwServerError(
                QStringLiteral("servers.dat has an invalid server list; it was not modified."));
        }
        return (*list)->elements;
    }

    void ServerListStore::replaceServerTags(const QVector<NbtTag>& entries)
    {
        auto list = std::make_shared<NbtList>();
        list->elementType = 10;
        list->elements = entries;
        m_document.root.insert(QStringLiteral("servers"), {9, list});
    }
} // namespace strata::settings
