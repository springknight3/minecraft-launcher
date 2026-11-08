#pragma once

#include <QString>

namespace strata::settings
{
    class MinecraftOptionValues
    {
    public:
        static QString toDisplayValue(const QString& minecraftVersion, const QString& key,
                                      const QString& storedValue);
        static QString toStoredValue(const QString& minecraftVersion, const QString& key,
                                     const QString& displayValue);
        static bool usesNormalizedFov(const QString& minecraftVersion);
    };
} // namespace strata::settings
