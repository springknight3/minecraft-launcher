#pragma once

#include "Instance.h"

#include <QList>
#include <QString>

namespace strata
{
    class InstanceManager
    {
    public:
        explicit InstanceManager(const QString& dataDirectory = QString());

        QString dataDirectory() const;
        QString instancesDirectory() const;
        QList<Instance> listInstances() const;
        Instance create(const QString& name, const QString& versionId) const;
        Instance duplicate(const Instance& source, const QString& newName) const;
        Instance rename(const Instance& instance, const QString& newName) const;
        Instance setFavorite(const Instance& instance, bool favorite) const;
        void deleteInstance(const Instance& instance) const;

    private:
        QString m_dataDirectory;
        QString m_instancesDirectory;

        Instance requireStoredInstance(const QString& id) const;
        Instance readInstance(const QString& directory) const;
        void writeMetadata(const Instance& instance) const;
        static QString requireText(const QString& value, const QString& label);
        static void copyTree(const QString& source, const QString& target);
        static void deleteTree(const QString& target);
    };
} // namespace strata
