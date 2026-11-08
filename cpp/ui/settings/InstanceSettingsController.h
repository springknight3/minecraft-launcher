#pragma once

#include "../core/instance/Instance.h"

#include <QMutex>
#include <QObject>
#include <QThreadPool>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <optional>

namespace strata::ui
{
    class InstanceSettingsController : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(bool busy READ busy NOTIFY changed)
        Q_PROPERTY(QString status READ status NOTIFY changed)
        Q_PROPERTY(QString instanceName READ instanceName NOTIFY changed)
        Q_PROPERTY(QString instanceVersion READ instanceVersion NOTIFY changed)
        Q_PROPERTY(QVariantMap commonOptions READ commonOptions NOTIFY changed)
        Q_PROPERTY(QVariantList controls READ controls NOTIFY changed)
        Q_PROPERTY(QVariantList servers READ servers NOTIFY changed)
        Q_PROPERTY(QString javaFlags READ javaFlags NOTIFY changed)
        Q_PROPERTY(bool javaFlagsOverridden READ javaFlagsOverridden NOTIFY changed)

    public:
        InstanceSettingsController(QMutex& instanceMutex, const QList<strata::Instance>& instances,
                                   QObject* parent = nullptr);
        ~InstanceSettingsController() override;

        bool busy() const;
        QString status() const;
        QString instanceName() const;
        QString instanceVersion() const;
        QVariantMap commonOptions() const;
        QVariantList controls() const;
        QVariantList servers() const;
        QString javaFlags() const;
        bool javaFlagsOverridden() const;

        Q_INVOKABLE void loadInstance(const QString& instanceId);
        Q_INVOKABLE void saveCommonOptions(const QVariantMap& options);
        Q_INVOKABLE void saveOption(const QString& key, const QString& value);
        Q_INVOKABLE void saveControls(const QVariantMap& bindings);
        Q_INVOKABLE void addServer(const QString& name, const QString& address);
        Q_INVOKABLE void updateServer(int index, const QString& name, const QString& address);
        Q_INVOKABLE void removeServer(int index);
        Q_INVOKABLE void saveJavaFlags(bool overridden, const QString& text);

    signals:
        void changed();

    private:
        QMutex& m_instanceMutex;
        const QList<strata::Instance>& m_instances;
        QThreadPool m_workers;
        std::optional<strata::Instance> m_instance;
        QVariantMap m_commonOptions;
        QVariantList m_controls;
        QVariantList m_servers;
        QString m_status;
        QString m_javaFlags;
        QString m_instanceName;
        QString m_instanceVersion;
        quint64 m_operationId = 0;
        bool m_busy = false;
        bool m_javaFlagsOverridden = false;

        void runAsync(const strata::Instance& instance, const QString& status,
                      std::function<QVariant()> action,
                      std::function<void(InstanceSettingsController&, const QVariant&)> success);
        bool ensureEditable();
        static QVariantMap readState(const strata::Instance& instance);
        static QVariantMap commonOptions(const strata::Instance& instance,
                                         const QMap<QString, QString>& options);
        static QVariantList readServers(const strata::Instance& instance);
        static QStringList parseFlags(const QString& text);
        void applyState(const QVariantMap& state);
    };
} // namespace strata::ui
