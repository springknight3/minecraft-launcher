#include "Instance.h"

#include <QDir>

namespace strata
{
    QString Instance::gameDirectory() const
    {
        return QDir(directory).filePath(QStringLiteral("game"));
    }
} // namespace strata
