#include "ThemeSettings.h"

#include "Paths.h"

#include <QMetaObject>
#include <QPointer>
#include <QRunnable>

#include <exception>

ThemeSettings::ThemeSettings(const QString& dataDirectory, QObject* parent)
    : QObject(parent),
      m_settings(std::make_shared<strata::settings::LauncherSettings>(
          dataDirectory.isEmpty() ? strata::Paths::dataDirectory() : dataDirectory))
{
    m_settingsWorker.setMaxThreadCount(1);
    load();
}

QString ThemeSettings::theme() const
{
    return m_theme;
}

void ThemeSettings::setTheme(const QString& themeName)
{
    const QString normalized =
        themeName == QStringLiteral("light") ? QStringLiteral("light") : QStringLiteral("dark");
    if (m_theme == normalized)
    {
        return;
    }

    m_theme = normalized;
    emit themeChanged();
    saveAppearanceAsync();
}

bool ThemeSettings::reduceMotion() const
{
    return m_reduceMotion;
}

void ThemeSettings::setReduceMotion(bool enabled)
{
    if (m_reduceMotion == enabled)
    {
        return;
    }

    m_reduceMotion = enabled;
    emit reduceMotionChanged();
    saveAppearanceAsync();
}

bool ThemeSettings::vsyncEnabled() const
{
    return m_vsyncEnabled;
}

void ThemeSettings::setVsyncEnabled(bool enabled)
{
    if (m_vsyncEnabled == enabled)
    {
        return;
    }

    m_vsyncEnabled = enabled;
    emit diagnosticsChanged();
    saveDiagnosticsAsync();
}

bool ThemeSettings::frameTimeOverlayEnabled() const
{
    return m_frameTimeOverlayEnabled;
}

void ThemeSettings::setFrameTimeOverlayEnabled(bool enabled)
{
    if (m_frameTimeOverlayEnabled == enabled)
    {
        return;
    }

    m_frameTimeOverlayEnabled = enabled;
    emit diagnosticsChanged();
    saveDiagnosticsAsync();
}

void ThemeSettings::save()
{
    saveAppearanceAsync();
    saveDiagnosticsAsync();
}

void ThemeSettings::load()
{
    m_theme = m_settings->darkMode() ? QStringLiteral("dark") : QStringLiteral("light");
    m_reduceMotion = m_settings->reduceMotion();
    m_vsyncEnabled = m_settings->vsyncEnabled();
    m_frameTimeOverlayEnabled = m_settings->frameTimeOverlayEnabled();
}

void ThemeSettings::toggleTheme()
{
    setTheme(m_theme == QStringLiteral("dark") ? QStringLiteral("light") : QStringLiteral("dark"));
}

void ThemeSettings::saveAppearanceAsync()
{
    const bool darkMode = m_theme == QStringLiteral("dark");
    const bool reduceMotion = m_reduceMotion;
    const auto settings = m_settings;
    QPointer<ThemeSettings> self(this);
    m_settingsWorker.start(QRunnable::create(
        [settings, self, darkMode, reduceMotion]()
        {
            QString error;
            try
            {
                settings->setAppearance(darkMode, reduceMotion);
            }
            catch (const std::exception& exception)
            {
                error = QString::fromUtf8(exception.what());
            }
            if (!error.isEmpty() && !self.isNull())
            {
                QMetaObject::invokeMethod(
                    self,
                    [self, error]()
                    {
                        if (!self.isNull())
                        {
                            emit self->settingsError(error);
                        }
                    },
                    Qt::QueuedConnection);
            }
        }));
}

void ThemeSettings::saveDiagnosticsAsync()
{
    const bool vsyncEnabled = m_vsyncEnabled;
    const bool frameTimeOverlayEnabled = m_frameTimeOverlayEnabled;
    const auto settings = m_settings;
    QPointer<ThemeSettings> self(this);
    m_settingsWorker.start(QRunnable::create(
        [settings, self, vsyncEnabled, frameTimeOverlayEnabled]()
        {
            QString error;
            try
            {
                settings->setDiagnosticsPreferences(vsyncEnabled, frameTimeOverlayEnabled);
            }
            catch (const std::exception& exception)
            {
                error = QString::fromUtf8(exception.what());
            }
            if (!error.isEmpty() && !self.isNull())
            {
                QMetaObject::invokeMethod(
                    self,
                    [self, error]()
                    {
                        if (!self.isNull())
                        {
                            emit self->settingsError(error);
                        }
                    },
                    Qt::QueuedConnection);
            }
        }));
}
