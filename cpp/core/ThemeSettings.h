#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

class ThemeSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY reduceMotionChanged)

public:
    explicit ThemeSettings(const QString &settingsPath = QString(), QObject *parent = nullptr);

    QString theme() const;
    void setTheme(const QString &themeName);

    bool reduceMotion() const;
    void setReduceMotion(bool enabled);

    Q_INVOKABLE void save();
    Q_INVOKABLE void load();
    Q_INVOKABLE void toggleTheme();

signals:
    void themeChanged();
    void reduceMotionChanged();

private:
    QString m_settingsPath;
    QSettings m_settings;
    QString m_theme;
    bool m_reduceMotion = false;

    QString resolveSettingsPath() const;
};
