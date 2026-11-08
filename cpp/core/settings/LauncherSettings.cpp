#include "LauncherSettings.h"

#include "InstanceSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>

#include <stdexcept>

namespace
{
    constexpr qsizetype maxSettingsSize = 1024 * 1024;
    QMutex settingsFileMutex;
    const QRegularExpression
        clientIdPattern(QStringLiteral("(?i)^[0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12}$"));

    bool readBoolean(const QJsonObject& object, const QString& key, bool fallback)
    {
        const QJsonValue value = object.value(key);
        if (value.isUndefined() || value.isNull())
        {
            return fallback;
        }
        if (!value.isBool())
        {
            throw std::runtime_error(QStringLiteral("Launcher setting '%1' must be true or false.")
                                         .arg(key)
                                         .toStdString());
        }
        return value.toBool();
    }

    QString readString(const QJsonObject& object, const QString& key, const QString& fallback)
    {
        const QJsonValue value = object.value(key);
        if (value.isUndefined() || value.isNull())
        {
            return fallback;
        }
        if (!value.isString())
        {
            throw std::runtime_error(
                QStringLiteral("Launcher setting '%1' must be a string.").arg(key).toStdString());
        }
        return value.toString();
    }
} // namespace

namespace strata::settings
{
    LauncherSettings::LauncherSettings(const QString& dataDirectory)
        : m_settingsFile(QDir(dataDirectory).filePath(QStringLiteral("settings.json"))),
          m_values(loadValues())
    {
        if (QFileInfo::exists(m_settingsFile))
        {
            return;
        }

        QSettings legacy(QDir(dataDirectory).filePath(QStringLiteral("settings.ini")),
                         QSettings::IniFormat);
        if (legacy.contains(QStringLiteral("theme")) ||
            legacy.contains(QStringLiteral("reduceMotion")))
        {
            m_values.darkMode =
                legacy.value(QStringLiteral("theme"), QStringLiteral("dark")).toString() !=
                QStringLiteral("light");
            m_values.reduceMotion = legacy.value(QStringLiteral("reduceMotion"), false).toBool();
            persist(m_values);
        }
    }

    QString LauncherSettings::azureClientId() const
    {
        QMutexLocker lock(&m_mutex);
        return m_values.azureClientId;
    }

    QStringList LauncherSettings::globalJavaLaunchFlags() const
    {
        QMutexLocker lock(&m_mutex);
        return m_values.globalJavaLaunchFlags;
    }

    QString LauncherSettings::lastLaunchedInstanceId() const
    {
        QMutexLocker lock(&m_mutex);
        return m_values.lastLaunchedInstanceId;
    }

    QStringList LauncherSettings::recentLaunchedInstanceIds() const
    {
        QMutexLocker lock(&m_mutex);
        return m_values.recentLaunchedInstanceIds;
    }

    bool LauncherSettings::trackInstanceLaunches() const
    {
        QMutexLocker lock(&m_mutex);
        return m_values.trackInstanceLaunches;
    }

    bool LauncherSettings::darkMode() const
    {
        QMutexLocker lock(&m_mutex);
        return m_values.darkMode;
    }

    bool LauncherSettings::reduceMotion() const
    {
        QMutexLocker lock(&m_mutex);
        return m_values.reduceMotion;
    }

    QString LauncherSettings::runtimeVendor() const
    {
        QMutexLocker lock(&m_mutex);
        return m_values.runtimeVendor;
    }

    bool LauncherSettings::redundantJavaRuntimes() const
    {
        QMutexLocker lock(&m_mutex);
        return m_values.redundantJavaRuntimes;
    }

    bool LauncherSettings::vsyncEnabled() const
    {
        QMutexLocker lock(&m_mutex);
        return m_values.vsyncEnabled;
    }

    bool LauncherSettings::frameTimeOverlayEnabled() const
    {
        QMutexLocker lock(&m_mutex);
        return m_values.frameTimeOverlayEnabled;
    }

    void LauncherSettings::setAzureClientId(const QString& clientId)
    {
        QMutexLocker fileLock(&settingsFileMutex);
        QMutexLocker lock(&m_mutex);
        Values values = loadValues();
        const QString normalized = clientId.trimmed();
        if (!normalized.isEmpty() && !clientIdPattern.match(normalized).hasMatch())
        {
            throw std::invalid_argument("Azure client ID must be a GUID or empty.");
        }
        values.azureClientId = normalized;
        update(values);
    }

    void LauncherSettings::setGlobalJavaLaunchFlags(const QStringList& flags)
    {
        QMutexLocker fileLock(&settingsFileMutex);
        QMutexLocker lock(&m_mutex);
        Values values = loadValues();
        values.globalJavaLaunchFlags = InstanceSettings::validateFlags(flags);
        update(values);
    }

    void LauncherSettings::setLastLaunchedInstanceId(const QString& instanceId)
    {
        QMutexLocker fileLock(&settingsFileMutex);
        QMutexLocker lock(&m_mutex);
        Values values = loadValues();
        const QString normalized = instanceId.trimmed();
        if (normalized.size() > 128)
        {
            throw std::invalid_argument("Last launched instance identifier is too long.");
        }
        values.lastLaunchedInstanceId = normalized;
        values.recentLaunchedInstanceIds =
            normalized.isEmpty() ? QStringList{} : QStringList{normalized};
        update(values);
    }

    void LauncherSettings::recordInstanceLaunch(const QString& instanceId)
    {
        QMutexLocker fileLock(&settingsFileMutex);
        QMutexLocker lock(&m_mutex);
        Values values = loadValues();
        if (!values.trackInstanceLaunches)
        {
            return;
        }
        const QString normalized = instanceId.trimmed();
        if (normalized.isEmpty() || normalized.size() > 128)
        {
            throw std::invalid_argument("Launched instance identifier is invalid.");
        }
        values.recentLaunchedInstanceIds.removeAll(normalized);
        values.recentLaunchedInstanceIds.prepend(normalized);
        while (values.recentLaunchedInstanceIds.size() > 2)
        {
            values.recentLaunchedInstanceIds.removeLast();
        }
        values.lastLaunchedInstanceId = normalized;
        update(values);
    }

    void LauncherSettings::setTrackInstanceLaunches(bool track)
    {
        QMutexLocker fileLock(&settingsFileMutex);
        QMutexLocker lock(&m_mutex);
        Values values = loadValues();
        values.trackInstanceLaunches = track;
        update(values);
    }

    void LauncherSettings::setAppearance(bool darkMode, bool reduceMotion)
    {
        QMutexLocker fileLock(&settingsFileMutex);
        QMutexLocker lock(&m_mutex);
        Values values = loadValues();
        values.darkMode = darkMode;
        values.reduceMotion = reduceMotion;
        update(values);
    }

    void LauncherSettings::setRuntimePreferences(const QString& vendor, bool redundant)
    {
        QMutexLocker fileLock(&settingsFileMutex);
        QMutexLocker lock(&m_mutex);
        Values values = loadValues();
        if (vendor != QStringLiteral("MOJANG") && vendor != QStringLiteral("TEMURIN") &&
            vendor != QStringLiteral("ZULU"))
        {
            throw std::invalid_argument("Unknown Java runtime vendor.");
        }
        values.runtimeVendor = vendor;
        values.redundantJavaRuntimes = redundant;
        update(values);
    }

    void LauncherSettings::setDiagnosticsPreferences(bool vsyncEnabled,
                                                     bool frameTimeOverlayEnabled)
    {
        QMutexLocker fileLock(&settingsFileMutex);
        QMutexLocker lock(&m_mutex);
        Values values = loadValues();
        values.vsyncEnabled = vsyncEnabled;
        values.frameTimeOverlayEnabled = frameTimeOverlayEnabled;
        update(values);
    }

    QStringList LauncherSettings::recommendedJavaFlags()
    {
        return {QStringLiteral("-Xms6G"),
                QStringLiteral("-Xmx8G"),
                QStringLiteral("-XX:+UnlockExperimentalVMOptions"),
                QStringLiteral("-XX:+UnlockDiagnosticVMOptions"),
                QStringLiteral("-XX:+DisableExplicitGC"),
                QStringLiteral("-XX:NmethodSweepActivity=1"),
                QStringLiteral("-XX:ReservedCodeCacheSize=400M"),
                QStringLiteral("-XX:NonNMethodCodeHeapSize=12M"),
                QStringLiteral("-XX:ProfiledCodeHeapSize=194M"),
                QStringLiteral("-XX:NonProfiledCodeHeapSize=194M"),
                QStringLiteral("-XX:-DontCompileHugeMethods"),
                QStringLiteral("-XX:MaxNodeLimit=240000"),
                QStringLiteral("-XX:NodeLimitFudgeFactor=8000"),
                QStringLiteral("-XX:+UseVectorCmov"),
                QStringLiteral("-XX:+PerfDisableSharedMem"),
                QStringLiteral("-XX:+UseFastUnorderedTimeStamps"),
                QStringLiteral("-XX:+UseCriticalJavaThreadPriority"),
                QStringLiteral("-XX:AllocatePrefetchStyle=3"),
                QStringLiteral("-XX:+UseG1GC"),
                QStringLiteral("-XX:MaxGCPauseMillis=37"),
                QStringLiteral("-XX:G1HeapRegionSize=16M"),
                QStringLiteral("-XX:G1NewSizePercent=23"),
                QStringLiteral("-XX:G1ReservePercent=20"),
                QStringLiteral("-XX:SurvivorRatio=32"),
                QStringLiteral("-XX:G1MixedGCCountTarget=3"),
                QStringLiteral("-XX:G1HeapWastePercent=20"),
                QStringLiteral("-XX:InitiatingHeapOccupancyPercent=10"),
                QStringLiteral("-XX:G1RSetUpdatingPauseTimePercent=0"),
                QStringLiteral("-XX:MaxTenuringThreshold=1"),
                QStringLiteral("-XX:G1SATBBufferEnqueueingThresholdPercent=30"),
                QStringLiteral("-XX:G1ConcMarkStepDurationMillis=5.0"),
                QStringLiteral("-XX:GCTimeRatio=99")};
    }

    LauncherSettings::Values LauncherSettings::loadValues() const
    {
        Values values;
        values.globalJavaLaunchFlags = recommendedJavaFlags();
#if !defined(Q_OS_LINUX)
        values.vsyncEnabled = true;
#endif

        QFile settingsFile(m_settingsFile);
        if (!settingsFile.exists())
        {
            return values;
        }
        if (!settingsFile.open(QIODevice::ReadOnly) || settingsFile.size() > maxSettingsSize)
        {
            throw std::runtime_error(QStringLiteral("Unable to read launcher settings: %1")
                                         .arg(m_settingsFile)
                                         .toStdString());
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(settingsFile.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            throw std::runtime_error(QStringLiteral("Launcher settings are invalid: %1")
                                         .arg(m_settingsFile)
                                         .toStdString());
        }

        const QJsonObject object = document.object();
        values.azureClientId = readString(object, QStringLiteral("azureClientId"), {});
        values.lastLaunchedInstanceId =
            readString(object, QStringLiteral("lastLaunchedInstanceId"), {});
        values.trackInstanceLaunches =
            readBoolean(object, QStringLiteral("trackInstanceLaunches"), true);
        const QJsonValue recentValue = object.value(QStringLiteral("recentLaunchedInstanceIds"));
        if (recentValue.isArray())
        {
            for (const QJsonValue& value : recentValue.toArray())
            {
                if (!value.isString() || value.toString().trimmed().isEmpty() ||
                    value.toString().size() > 128)
                {
                    throw std::runtime_error(
                        "Recent launched instance identifiers must be valid strings.");
                }
                const QString id = value.toString();
                if (!values.recentLaunchedInstanceIds.contains(id) &&
                    values.recentLaunchedInstanceIds.size() < 2)
                {
                    values.recentLaunchedInstanceIds.append(id);
                }
            }
        }
        else if (!recentValue.isUndefined() && !recentValue.isNull())
        {
            throw std::runtime_error("Recent launched instance identifiers must be an array.");
        }
        if (values.recentLaunchedInstanceIds.isEmpty() && !values.lastLaunchedInstanceId.isEmpty())
        {
            values.recentLaunchedInstanceIds.append(values.lastLaunchedInstanceId);
        }
        else if (!values.recentLaunchedInstanceIds.isEmpty())
        {
            values.lastLaunchedInstanceId = values.recentLaunchedInstanceIds.constFirst();
        }
        if (!values.azureClientId.isEmpty() &&
            !clientIdPattern.match(values.azureClientId).hasMatch())
        {
            throw std::runtime_error("Azure client ID must be a GUID or empty.");
        }
        if (object.contains(QStringLiteral("globalJavaLaunchFlags")) &&
            !object.value(QStringLiteral("globalJavaLaunchFlags")).isNull())
        {
            const QJsonValue flagsValue = object.value(QStringLiteral("globalJavaLaunchFlags"));
            if (!flagsValue.isArray())
            {
                throw std::runtime_error("Global Java launch flags must be an array.");
            }
            QStringList flags;
            for (const QJsonValue& value : flagsValue.toArray())
            {
                if (!value.isString())
                {
                    throw std::runtime_error("Global Java launch flags must contain strings.");
                }
                flags.append(value.toString());
            }
            values.globalJavaLaunchFlags = InstanceSettings::validateFlags(flags);
        }
        values.darkMode = readBoolean(object, QStringLiteral("darkMode"), true);
        values.reduceMotion = readBoolean(object, QStringLiteral("reduceMotion"), false);
        values.runtimeVendor =
            readString(object, QStringLiteral("runtimeVendor"), QStringLiteral("MOJANG"));
        if (values.runtimeVendor != QStringLiteral("MOJANG") &&
            values.runtimeVendor != QStringLiteral("TEMURIN") &&
            values.runtimeVendor != QStringLiteral("ZULU"))
        {
            throw std::runtime_error("Unknown Java runtime vendor in launcher settings.");
        }
        values.redundantJavaRuntimes =
            readBoolean(object, QStringLiteral("redundantJavaRuntimes"), false);
        values.vsyncEnabled =
            readBoolean(object, QStringLiteral("vsyncEnabled"), values.vsyncEnabled);
        values.frameTimeOverlayEnabled =
            readBoolean(object, QStringLiteral("frameTimeOverlayEnabled"), false);
        return values;
    }

    void LauncherSettings::update(const Values& values)
    {
        persist(values);
        m_values = values;
    }

    void LauncherSettings::persist(const Values& values) const
    {
        const QFileInfo fileInfo(m_settingsFile);
        if (!QDir().mkpath(fileInfo.absolutePath()))
        {
            throw std::runtime_error(
                QStringLiteral("Unable to create launcher settings directory: %1")
                    .arg(fileInfo.absolutePath())
                    .toStdString());
        }

        QJsonArray flags;
        for (const QString& flag : values.globalJavaLaunchFlags)
        {
            flags.append(flag);
        }
        QJsonArray recentInstances;
        for (const QString& instanceId : values.recentLaunchedInstanceIds)
        {
            recentInstances.append(instanceId);
        }
        const QJsonObject object{
            {QStringLiteral("azureClientId"), values.azureClientId},
            {QStringLiteral("globalJavaLaunchFlags"), flags},
            {QStringLiteral("lastLaunchedInstanceId"), values.lastLaunchedInstanceId},
            {QStringLiteral("recentLaunchedInstanceIds"), recentInstances},
            {QStringLiteral("trackInstanceLaunches"), values.trackInstanceLaunches},
            {QStringLiteral("darkMode"), values.darkMode},
            {QStringLiteral("reduceMotion"), values.reduceMotion},
            {QStringLiteral("runtimeVendor"), values.runtimeVendor},
            {QStringLiteral("redundantJavaRuntimes"), values.redundantJavaRuntimes},
            {QStringLiteral("vsyncEnabled"), values.vsyncEnabled},
            {QStringLiteral("frameTimeOverlayEnabled"), values.frameTimeOverlayEnabled}};

        QSaveFile settingsFile(m_settingsFile);
        const QByteArray contents = QJsonDocument(object).toJson(QJsonDocument::Indented);
        if (!settingsFile.open(QIODevice::WriteOnly) ||
            settingsFile.write(contents) != contents.size() || !settingsFile.commit())
        {
            throw std::runtime_error(QStringLiteral("Unable to save launcher settings: %1")
                                         .arg(settingsFile.errorString())
                                         .toStdString());
        }
    }
} // namespace strata::settings
