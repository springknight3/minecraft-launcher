#include "Paths.h"

#include <QDir>
#include <QStandardPaths>

namespace strata
{
    QString Paths::dataDirectory()
    {
#if defined(Q_OS_WIN)
        constexpr auto operatingSystem = OperatingSystem::Windows;
#elif defined(Q_OS_MACOS)
        constexpr auto operatingSystem = OperatingSystem::MacOS;
#else
        constexpr auto operatingSystem = OperatingSystem::Linux;
#endif
        return dataDirectory(operatingSystem, QDir::homePath(), qEnvironmentVariable("APPDATA"),
                             qEnvironmentVariable("XDG_DATA_HOME"));
    }

    QString Paths::dataDirectory(OperatingSystem operatingSystem, const QString& homeDirectory,
                                 const QString& appDataDirectory, const QString& xdgDataHome)
    {
        QString baseDirectory;
        switch (operatingSystem)
        {
        case OperatingSystem::Windows:
            baseDirectory = appDataDirectory.trimmed().isEmpty()
                                ? QDir(homeDirectory).filePath(QStringLiteral("AppData/Roaming"))
                                : appDataDirectory;
            break;
        case OperatingSystem::MacOS:
            baseDirectory =
                QDir(homeDirectory).filePath(QStringLiteral("Library/Application Support"));
            break;
        case OperatingSystem::Linux:
            baseDirectory = xdgDataHome.trimmed().isEmpty()
                                ? QDir(homeDirectory).filePath(QStringLiteral(".local/share"))
                                : xdgDataHome;
            break;
        }
        return QDir::cleanPath(QDir(baseDirectory).filePath(QStringLiteral("StrataLauncher")));
    }
} // namespace strata
