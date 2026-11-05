#pragma once

#include <QString>

namespace strata
{
    class LauncherLogging
    {
    public:
        static bool configure(const QString& dataDirectory, QString* error = nullptr);
        static QString redact(const QString& message);
    };
} // namespace strata
