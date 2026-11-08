#pragma once

#include "settings/LauncherSettings.h"

#include <QObject>
#include <QString>
#include <QThreadPool>

#include <memory>

class ThemeSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY reduceMotionChanged)
    Q_PROPERTY(bool vsyncEnabled READ vsyncEnabled WRITE setVsyncEnabled NOTIFY diagnosticsChanged)
    Q_PROPERTY(bool frameTimeOverlayEnabled READ frameTimeOverlayEnabled WRITE
                   setFrameTimeOverlayEnabled NOTIFY diagnosticsChanged)

public:
    explicit ThemeSettings(const QString& dataDirectory = QString(), QObject* parent = nullptr);

    QString theme() const;
    void setTheme(const QString& themeName);

    bool reduceMotion() const;
    void setReduceMotion(bool enabled);
    bool vsyncEnabled() const;
    void setVsyncEnabled(bool enabled);
    bool frameTimeOverlayEnabled() const;
    void setFrameTimeOverlayEnabled(bool enabled);

    Q_INVOKABLE void save();
    Q_INVOKABLE void load();
    Q_INVOKABLE void toggleTheme();

signals:
    void themeChanged();
    void reduceMotionChanged();
    void diagnosticsChanged();
    void settingsError(const QString& message);

private:
    std::shared_ptr<strata::settings::LauncherSettings> m_settings;
    QThreadPool m_settingsWorker;
    QString m_theme = QStringLiteral("dark");
    bool m_reduceMotion = false;
    bool m_vsyncEnabled = false;
    bool m_frameTimeOverlayEnabled = false;

    void saveAppearanceAsync();
    void saveDiagnosticsAsync();
};
