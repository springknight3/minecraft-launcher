#include "ThemeSettings.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace
{
QString defaultSettingsPath()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appData.isEmpty()) {
        return QStringLiteral("stratalauncher.ini");
    }

    QDir dir(appData);
    if (!dir.exists()) {
        dir.mkpath(appData);
    }
    return QFileInfo(dir.filePath(QStringLiteral("settings.ini")).toLocal8Bit().constData()).absoluteFilePath();
}
}

ThemeSettings::ThemeSettings(const QString &settingsPath, QObject *parent)
    : QObject(parent)
    , m_settingsPath(settingsPath.isEmpty() ? defaultSettingsPath() : settingsPath)
    , m_settings(m_settingsPath, QSettings::IniFormat)
{
    load();
}

QString ThemeSettings::theme() const
{
    return m_theme;
}

void ThemeSettings::setTheme(const QString &themeName)
{
    const QString normalized = themeName == QStringLiteral("light") ? QStringLiteral("light")
                                                                      : QStringLiteral("dark");
    if (m_theme == normalized) {
        return;
    }

    m_theme = normalized;
    emit themeChanged();
}

bool ThemeSettings::reduceMotion() const
{
    return m_reduceMotion;
}

void ThemeSettings::setReduceMotion(bool enabled)
{
    if (m_reduceMotion == enabled) {
        return;
    }

    m_reduceMotion = enabled;
    emit reduceMotionChanged();
}

void ThemeSettings::save()
{
    m_settings.setValue(QStringLiteral("theme"), m_theme);
    m_settings.setValue(QStringLiteral("reduceMotion"), m_reduceMotion);
    m_settings.sync();
}

void ThemeSettings::load()
{
    m_theme = m_settings.value(QStringLiteral("theme"), QStringLiteral("dark")).toString();
    if (m_theme != QStringLiteral("light") && m_theme != QStringLiteral("dark")) {
        m_theme = QStringLiteral("dark");
    }

    m_reduceMotion = m_settings.value(QStringLiteral("reduceMotion"), false).toBool();
}

void ThemeSettings::toggleTheme()
{
    setTheme(m_theme == QStringLiteral("dark") ? QStringLiteral("light") : QStringLiteral("dark"));
    save();
}

QString ThemeSettings::resolveSettingsPath() const
{
    return m_settingsPath;
}
