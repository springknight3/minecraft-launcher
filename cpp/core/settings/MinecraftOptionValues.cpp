#include "MinecraftOptionValues.h"

#include <QRegularExpression>

#include <cmath>
#include <stdexcept>

namespace strata::settings
{
    QString MinecraftOptionValues::toDisplayValue(const QString& minecraftVersion,
                                                  const QString& key, const QString& storedValue)
    {
        if (!usesNormalizedFov(minecraftVersion) || key != QStringLiteral("fov"))
        {
            return storedValue;
        }
        bool ok = false;
        const double normalized = storedValue.toDouble(&ok);
        if (!ok)
        {
            return storedValue;
        }
        return QString::number(static_cast<int>(std::round(70.0 + normalized * 40.0)));
    }

    QString MinecraftOptionValues::toStoredValue(const QString& minecraftVersion,
                                                 const QString& key, const QString& displayValue)
    {
        if (!usesNormalizedFov(minecraftVersion) || key != QStringLiteral("fov"))
        {
            return displayValue;
        }
        bool ok = false;
        const int degrees = displayValue.trimmed().toInt(&ok);
        if (!ok)
        {
            throw std::invalid_argument("Minecraft 26.3 FOV must be an integer from 30 to 110.");
        }
        if (degrees < 30 || degrees > 110)
        {
            throw std::invalid_argument("Minecraft 26.3 FOV must be between 30 and 110 degrees.");
        }
        QString stored = QString::number((degrees - 70) / 40.0, 'f', 3);
        while (stored.endsWith(QLatin1Char('0')) && !stored.endsWith(QStringLiteral(".0")))
        {
            stored.chop(1);
        }
        if (stored.endsWith(QLatin1Char('.')))
        {
            stored.append(QLatin1Char('0'));
        }
        return stored;
    }

    bool MinecraftOptionValues::usesNormalizedFov(const QString& minecraftVersion)
    {
        static const QRegularExpression majorPattern(QStringLiteral("^(\\d+)\\."));
        const QRegularExpressionMatch match = majorPattern.match(minecraftVersion);
        return match.hasMatch() && match.captured(1).toInt() >= 26;
    }
} // namespace strata::settings
