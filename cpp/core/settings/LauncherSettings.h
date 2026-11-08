#pragma once

#include <QMutex>
#include <QString>
#include <QStringList>

namespace strata::settings
{
    class LauncherSettings
    {
    public:
        explicit LauncherSettings(const QString& dataDirectory);

        QString azureClientId() const;
        QStringList globalJavaLaunchFlags() const;
        QString lastLaunchedInstanceId() const;
        QStringList recentLaunchedInstanceIds() const;
        bool trackInstanceLaunches() const;
        bool darkMode() const;
        bool reduceMotion() const;
        QString runtimeVendor() const;
        bool redundantJavaRuntimes() const;
        bool vsyncEnabled() const;
        bool frameTimeOverlayEnabled() const;

        void setAzureClientId(const QString& clientId);
        void setGlobalJavaLaunchFlags(const QStringList& flags);
        void setLastLaunchedInstanceId(const QString& instanceId);
        void recordInstanceLaunch(const QString& instanceId);
        void setTrackInstanceLaunches(bool track);
        void setAppearance(bool darkMode, bool reduceMotion);
        void setRuntimePreferences(const QString& vendor, bool redundant);
        void setDiagnosticsPreferences(bool vsyncEnabled, bool frameTimeOverlayEnabled);

        static QStringList recommendedJavaFlags();

    private:
        struct Values
        {
            QString azureClientId;
            QStringList globalJavaLaunchFlags;
            QString lastLaunchedInstanceId;
            QStringList recentLaunchedInstanceIds;
            bool trackInstanceLaunches = true;
            bool darkMode = true;
            bool reduceMotion = false;
            QString runtimeVendor = QStringLiteral("MOJANG");
            bool redundantJavaRuntimes = false;
            bool vsyncEnabled = false;
            bool frameTimeOverlayEnabled = false;
        };

        QString m_settingsFile;
        Values m_values;
        mutable QMutex m_mutex;

        Values loadValues() const;
        void update(const Values& values);
        void persist(const Values& values) const;
    };
} // namespace strata::settings
