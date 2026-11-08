#pragma once

#include "NbtFile.h"

#include <QString>
#include <QVector>

namespace strata::settings
{
    class ServerListStore
    {
    public:
        struct Server
        {
            QString name;
            QString address;

            bool operator==(const Server&) const = default;
        };

        explicit ServerListStore(const QString& file, const QString& minecraftVersion = {});

        QVector<Server> servers() const;
        void add(const QString& name, const QString& address);
        void update(qsizetype index, const QString& name, const QString& address);
        void remove(qsizetype index);
        void save() const;

    private:
        QString m_file;
        NbtDocument m_document;
        bool m_compressedOnSave = true;

        QVector<NbtTag> serverTags() const;
        void replaceServerTags(const QVector<NbtTag>& entries);
    };
} // namespace strata::settings
