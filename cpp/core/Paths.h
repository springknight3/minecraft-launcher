#pragma once

#include <QString>

namespace strata
{
    enum class OperatingSystem
    {
        Windows,
        MacOS,
        Linux
    };

    class Paths
    {
    public:
        static QString dataDirectory();
        static QString dataDirectory(OperatingSystem operatingSystem, const QString& homeDirectory,
                                     const QString& appDataDirectory = QString(),
                                     const QString& xdgDataHome = QString());
    };
} // namespace strata
