#pragma once

#include <QString>
#include <QStringList>

namespace strata
{
    class InstanceSettings
    {
    public:
        enum class JavaRuntimeMode
        {
            System,
            Managed
        };

        explicit InstanceSettings(const QString& instanceDirectory);

        bool hasJavaLaunchFlagsOverride() const;
        QStringList javaLaunchFlagsOverride() const;
        QStringList effectiveJavaLaunchFlags(const QStringList& globalFlags) const;
        void setJavaLaunchFlagsOverride(const QStringList& flags);
        void clearJavaLaunchFlagsOverride();

        JavaRuntimeMode javaRuntimeMode() const;
        void setJavaRuntimeMode(JavaRuntimeMode mode);
        QString javaRuntimeComponent() const;
        void setJavaRuntimeComponent(const QString& component);
        bool javaRuntimeRedundant() const;
        void setJavaRuntimeRedundant(bool redundant);

        static QStringList validateFlags(const QStringList& flags);

    private:
        QString m_settingsFile;
        QStringList m_javaLaunchFlagsOverride;
        bool m_hasJavaLaunchFlagsOverride = false;
        JavaRuntimeMode m_javaRuntimeMode = JavaRuntimeMode::System;
        QString m_javaRuntimeComponent;
        bool m_hasJavaRuntimeComponent = false;
        bool m_javaRuntimeRedundant = false;

        void persist(const QStringList& flags, bool hasFlags, JavaRuntimeMode mode,
                     const QString& component, bool hasComponent, bool runtimeRedundant) const;
    };
} // namespace strata
