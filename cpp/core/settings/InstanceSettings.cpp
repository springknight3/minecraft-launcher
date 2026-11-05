#include "InstanceSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

#include <stdexcept>

namespace strata
{
    namespace
    {
        [[noreturn]] void throwSettingsError(const QString& message, const QString& path,
                                             const QString& detail = {})
        {
            const QString fullMessage =
                detail.isEmpty() ? QStringLiteral("%1: %2").arg(message, path)
                                 : QStringLiteral("%1: %2 (%3)").arg(message, path, detail);
            throw std::runtime_error(fullMessage.toStdString());
        }

        QString runtimeModeName(InstanceSettings::JavaRuntimeMode mode)
        {
            switch (mode)
            {
            case InstanceSettings::JavaRuntimeMode::System:
                return QStringLiteral("SYSTEM");
            case InstanceSettings::JavaRuntimeMode::Managed:
                return QStringLiteral("MANAGED");
            }
            throw std::invalid_argument("Invalid Java runtime mode");
        }
    } // namespace

    InstanceSettings::InstanceSettings(const QString& instanceDirectory)
        : m_settingsFile(QDir(QFileInfo(instanceDirectory).absoluteFilePath())
                             .filePath(QStringLiteral("settings.json")))
    {
        QFile file(m_settingsFile);
        if (!file.exists())
        {
            return;
        }
        if (!file.open(QIODevice::ReadOnly))
        {
            throwSettingsError(QStringLiteral("Unable to read instance settings"), m_settingsFile,
                               file.errorString());
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            throwSettingsError(QStringLiteral("Instance settings are invalid"), m_settingsFile,
                               parseError.errorString());
        }
        const QJsonObject settings = document.object();
        if (settings.contains(QStringLiteral("javaLaunchFlagsOverride")))
        {
            const QJsonValue flagsValue = settings.value(QStringLiteral("javaLaunchFlagsOverride"));
            if (!flagsValue.isNull())
            {
                if (!flagsValue.isArray())
                {
                    throwSettingsError(QStringLiteral("Instance settings contain invalid values"),
                                       m_settingsFile);
                }
                QStringList flags;
                for (const QJsonValue& value : flagsValue.toArray())
                {
                    if (!value.isString())
                    {
                        throwSettingsError(
                            QStringLiteral("Instance settings contain invalid values"),
                            m_settingsFile);
                    }
                    flags.append(value.toString());
                }
                m_javaLaunchFlagsOverride = validateFlags(flags);
                m_hasJavaLaunchFlagsOverride = true;
            }
        }
        if (settings.contains(QStringLiteral("javaRuntimeMode")))
        {
            const QString mode = settings.value(QStringLiteral("javaRuntimeMode")).toString();
            if (mode == QStringLiteral("SYSTEM"))
            {
                m_javaRuntimeMode = JavaRuntimeMode::System;
            }
            else if (mode == QStringLiteral("MANAGED"))
            {
                m_javaRuntimeMode = JavaRuntimeMode::Managed;
            }
            else
            {
                throwSettingsError(QStringLiteral("Instance settings contain invalid values"),
                                   m_settingsFile);
            }
        }
        if (settings.contains(QStringLiteral("javaRuntimeComponent")) &&
            !settings.value(QStringLiteral("javaRuntimeComponent")).isNull())
        {
            if (!settings.value(QStringLiteral("javaRuntimeComponent")).isString())
            {
                throwSettingsError(QStringLiteral("Instance settings contain invalid values"),
                                   m_settingsFile);
            }
            m_javaRuntimeComponent =
                settings.value(QStringLiteral("javaRuntimeComponent")).toString();
            static const QRegularExpression componentPattern(QStringLiteral("^[a-z0-9-]+$"));
            if (!componentPattern.match(m_javaRuntimeComponent).hasMatch())
            {
                throwSettingsError(QStringLiteral("Instance settings contain invalid values"),
                                   m_settingsFile);
            }
            m_hasJavaRuntimeComponent = true;
        }
        if (settings.contains(QStringLiteral("javaRuntimeRedundant")))
        {
            const QJsonValue redundant = settings.value(QStringLiteral("javaRuntimeRedundant"));
            if (!redundant.isBool())
            {
                throwSettingsError(QStringLiteral("Instance settings contain invalid values"),
                                   m_settingsFile);
            }
            m_javaRuntimeRedundant = redundant.toBool();
        }
    }

    bool InstanceSettings::hasJavaLaunchFlagsOverride() const
    {
        return m_hasJavaLaunchFlagsOverride;
    }

    QStringList InstanceSettings::javaLaunchFlagsOverride() const
    {
        return m_hasJavaLaunchFlagsOverride ? m_javaLaunchFlagsOverride : QStringList{};
    }

    QStringList InstanceSettings::effectiveJavaLaunchFlags(const QStringList& globalFlags) const
    {
        return m_hasJavaLaunchFlagsOverride ? m_javaLaunchFlagsOverride : globalFlags;
    }

    void InstanceSettings::setJavaLaunchFlagsOverride(const QStringList& flags)
    {
        const QStringList normalized = validateFlags(flags);
        persist(normalized, true, m_javaRuntimeMode, m_javaRuntimeComponent,
                m_hasJavaRuntimeComponent, m_javaRuntimeRedundant);
        m_javaLaunchFlagsOverride = normalized;
        m_hasJavaLaunchFlagsOverride = true;
    }

    void InstanceSettings::clearJavaLaunchFlagsOverride()
    {
        persist({}, false, m_javaRuntimeMode, m_javaRuntimeComponent, m_hasJavaRuntimeComponent,
                m_javaRuntimeRedundant);
        m_javaLaunchFlagsOverride.clear();
        m_hasJavaLaunchFlagsOverride = false;
    }

    InstanceSettings::JavaRuntimeMode InstanceSettings::javaRuntimeMode() const
    {
        return m_javaRuntimeMode;
    }

    void InstanceSettings::setJavaRuntimeMode(JavaRuntimeMode mode)
    {
        const QString modeName = runtimeModeName(mode);
        Q_UNUSED(modeName)
        persist(m_javaLaunchFlagsOverride, m_hasJavaLaunchFlagsOverride, mode,
                m_javaRuntimeComponent, m_hasJavaRuntimeComponent, m_javaRuntimeRedundant);
        m_javaRuntimeMode = mode;
    }

    QString InstanceSettings::javaRuntimeComponent() const
    {
        return m_hasJavaRuntimeComponent ? m_javaRuntimeComponent : QString();
    }

    bool InstanceSettings::javaRuntimeRedundant() const
    {
        return m_javaRuntimeRedundant;
    }

    void InstanceSettings::setJavaRuntimeRedundant(bool redundant)
    {
        persist(m_javaLaunchFlagsOverride, m_hasJavaLaunchFlagsOverride, m_javaRuntimeMode,
                m_javaRuntimeComponent, m_hasJavaRuntimeComponent, redundant);
        m_javaRuntimeRedundant = redundant;
    }

    void InstanceSettings::setJavaRuntimeComponent(const QString& component)
    {
        static const QRegularExpression componentPattern(QStringLiteral("^[a-z0-9-]+$"));
        if (!component.isEmpty() && !componentPattern.match(component).hasMatch())
        {
            throw std::invalid_argument("Invalid managed Java runtime identifier.");
        }
        const bool hasComponent = !component.isEmpty();
        persist(m_javaLaunchFlagsOverride, m_hasJavaLaunchFlagsOverride, m_javaRuntimeMode,
                component, hasComponent, m_javaRuntimeRedundant);
        m_javaRuntimeComponent = component;
        m_hasJavaRuntimeComponent = hasComponent;
    }

    QStringList InstanceSettings::validateFlags(const QStringList& flags)
    {
        QStringList normalized;
        for (const QString& flag : flags)
        {
            if (flag.trimmed().isEmpty())
            {
                continue;
            }
            const QString value = flag.trimmed();
            if (!value.startsWith(QLatin1Char('-')) || value.startsWith(QLatin1Char('@')) ||
                value == QStringLiteral("-cp") || value == QStringLiteral("-classpath") ||
                value == QStringLiteral("--class-path") || value == QStringLiteral("-jar") ||
                value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r')) ||
                value.contains(QChar::Null))
            {
                throw std::invalid_argument(
                    "Each Java launch flag must be one JVM option beginning with '-'.");
            }
            normalized.append(value);
        }

        static const QRegularExpression gcPattern(
            QStringLiteral("^-XX:([+-])(Use[A-Za-z0-9]+GC)$"));
        QStringList enabledCollectors;
        for (const QString& flag : normalized)
        {
            const auto match = gcPattern.match(flag);
            if (match.hasMatch() && match.captured(1) == QStringLiteral("+") &&
                !enabledCollectors.contains(match.captured(2)))
            {
                enabledCollectors.append(match.captured(2));
            }
        }
        if (enabledCollectors.size() > 1)
        {
            throw std::invalid_argument(
                "Select only one Java garbage collector in the launch flags.");
        }
        return normalized;
    }

    void InstanceSettings::persist(const QStringList& flags, bool hasFlags, JavaRuntimeMode mode,
                                   const QString& component, bool hasComponent,
                                   bool runtimeRedundant) const
    {
        const QString parentDirectory = QFileInfo(m_settingsFile).absolutePath();
        if (!QDir().mkpath(parentDirectory))
        {
            throwSettingsError(QStringLiteral("Unable to create instance settings directory"),
                               parentDirectory);
        }
        QJsonObject settings;
        if (hasFlags)
        {
            QJsonArray jsonFlags;
            for (const QString& flag : flags)
            {
                jsonFlags.append(flag);
            }
            settings.insert(QStringLiteral("javaLaunchFlagsOverride"), jsonFlags);
        }
        settings.insert(QStringLiteral("javaRuntimeMode"), runtimeModeName(mode));
        if (hasComponent)
        {
            settings.insert(QStringLiteral("javaRuntimeComponent"), component);
        }
        settings.insert(QStringLiteral("javaRuntimeRedundant"), runtimeRedundant);

        QSaveFile file(m_settingsFile);
        if (!file.open(QIODevice::WriteOnly))
        {
            throwSettingsError(QStringLiteral("Unable to write instance settings"), m_settingsFile,
                               file.errorString());
        }
        const QByteArray json = QJsonDocument(settings).toJson(QJsonDocument::Indented);
        if (file.write(json) != json.size() || !file.commit())
        {
            throwSettingsError(QStringLiteral("Unable to commit instance settings"), m_settingsFile,
                               file.errorString());
        }
    }
} // namespace strata
