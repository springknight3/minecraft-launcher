#pragma once

#include <QString>
#include <QUrl>

#include <QtTypes>

namespace strata::download
{
    struct DownloadTask
    {
        QUrl source;
        QString destination;
        QString expectedSha1;
        QString description;
        qint64 maxBytes = 2LL * 1024 * 1024 * 1024;
    };
} // namespace strata::download
