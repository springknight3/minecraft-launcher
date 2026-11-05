#pragma once

#include <QDateTime>
#include <QString>

namespace strata
{
    struct Instance
    {
        QString id;
        QString name;
        QString versionId;
        QDateTime createdAt;
        QString directory;
        bool favorite = false;

        QString gameDirectory() const;

        bool operator==(const Instance&) const = default;
    };
} // namespace strata
