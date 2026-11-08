#include "InstanceSettingsController.h"

#include "../../core/settings/InstanceSettings.h"
#include "../../core/settings/MinecraftControls.h"
#include "../../core/settings/MinecraftOptionValues.h"
#include "../../core/settings/OptionsFile.h"
#include "../../core/settings/ServerListStore.h"

#include <QDir>
#include <QMetaObject>
#include <QMutexLocker>
#include <QPointer>
#include <QRunnable>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace strata::ui
{
    namespace
    {
        QString optionsPath(const strata::Instance& instance)
        {
            return QDir(instance.gameDirectory()).filePath(QStringLiteral("options.txt"));
        }

        QString serversPath(const strata::Instance& instance)
        {
            return QDir(instance.gameDirectory()).filePath(QStringLiteral("servers.dat"));
        }

        int integerValue(const QVariantMap& values, const QString& key)
        {
            bool ok = false;
            const int value = values.value(key).toString().toInt(&ok);
            if (!ok)
            {
                throw std::invalid_argument(
                    QStringLiteral("%1 must be an integer.").arg(key).toStdString());
            }
            return value;
        }

        double doubleValue(const QVariantMap& values, const QString& key)
        {
            bool ok = false;
            const double value = values.value(key).toString().toDouble(&ok);
            if (!ok || !std::isfinite(value))
            {
                throw std::invalid_argument(
                    QStringLiteral("%1 must be a finite number.").arg(key).toStdString());
            }
            return value;
        }

        bool booleanValue(const QVariantMap& values, const QString& key)
        {
            const QVariant value = values.value(key);
            if (!value.isValid() || value.metaType().id() != QMetaType::Bool)
            {
                throw std::invalid_argument(
                    QStringLiteral("%1 must be a boolean.").arg(key).toStdString());
            }
            return value.toBool();
        }

        void requireRange(int value, int minimum, int maximum, const QString& label)
        {
            if (value < minimum || value > maximum)
            {
                throw std::invalid_argument(QStringLiteral("%1 must be between %2 and %3.")
                                                .arg(label)
                                                .arg(minimum)
                                                .arg(maximum)
                                                .toStdString());
            }
        }
    } // namespace

    InstanceSettingsController::InstanceSettingsController(QMutex& instanceMutex,
                                                           const QList<strata::Instance>& instances,
                                                           QObject* parent)
        : QObject(parent), m_instanceMutex(instanceMutex), m_instances(instances)
    {
        m_workers.setMaxThreadCount(2);
    }

    InstanceSettingsController::~InstanceSettingsController()
    {
        m_workers.waitForDone();
    }

    bool InstanceSettingsController::busy() const
    {
        return m_busy;
    }

    QString InstanceSettingsController::status() const
    {
        return m_status;
    }

    QString InstanceSettingsController::instanceName() const
    {
        return m_instanceName;
    }

    QString InstanceSettingsController::instanceVersion() const
    {
        return m_instanceVersion;
    }

    QVariantMap InstanceSettingsController::commonOptions() const
    {
        return m_commonOptions;
    }

    QVariantList InstanceSettingsController::controls() const
    {
        return m_controls;
    }

    QVariantList InstanceSettingsController::servers() const
    {
        return m_servers;
    }

    QString InstanceSettingsController::javaFlags() const
    {
        return m_javaFlags;
    }

    bool InstanceSettingsController::javaFlagsOverridden() const
    {
        return m_javaFlagsOverridden;
    }

    void InstanceSettingsController::loadInstance(const QString& instanceId)
    {
        if (m_busy)
        {
            m_status = QStringLiteral(
                "Wait for the current settings operation to finish before changing instances.");
            emit changed();
            return;
        }
        strata::Instance selected;
        {
            QMutexLocker lock(&m_instanceMutex);
            const auto found = std::find_if(m_instances.cbegin(), m_instances.cend(),
                                            [&instanceId](const strata::Instance& instance)
                                            { return instance.id == instanceId; });
            if (found == m_instances.cend())
            {
                m_instance.reset();
                m_instanceName.clear();
                m_instanceVersion.clear();
                m_commonOptions.clear();
                m_servers.clear();
                m_javaFlags.clear();
                m_javaFlagsOverridden = false;
                m_status = QStringLiteral("Select an available instance before editing settings.");
                m_busy = false;
                emit changed();
                return;
            }
            selected = *found;
        }
        m_instance = selected;
        m_instanceName = selected.name;
        m_instanceVersion = selected.versionId;
        m_commonOptions.clear();
        m_servers.clear();
        runAsync(
            selected, QStringLiteral("Loading instance settings…"),
            [selected] { return readState(selected); },
            [](InstanceSettingsController& self, const QVariant& result)
            { self.applyState(result.toMap()); });
    }

    void InstanceSettingsController::saveCommonOptions(const QVariantMap& options)
    {
        if (!ensureEditable())
        {
            return;
        }
        const strata::Instance selected = *m_instance;
        runAsync(
            selected, QStringLiteral("Saving game options…"),
            [selected, options]
            {
                const int fov = integerValue(options, QStringLiteral("fov"));
                const double gamma = doubleValue(options, QStringLiteral("gamma"));
                const int renderDistance = integerValue(options, QStringLiteral("renderDistance"));
                const int simulationDistance =
                    integerValue(options, QStringLiteral("simulationDistance"));
                const int guiScale = integerValue(options, QStringLiteral("guiScale"));
                const bool unlimited = booleanValue(options, QStringLiteral("unlimitedFps"));
                const int maximumFps = integerValue(options, QStringLiteral("maxFps"));
                requireRange(fov, 30, 110, QStringLiteral("FOV"));
                requireRange(renderDistance, 2, 128, QStringLiteral("Render distance"));
                requireRange(simulationDistance, 2, 128, QStringLiteral("Simulation distance"));
                requireRange(guiScale, 0, 16, QStringLiteral("GUI scale"));
                requireRange(maximumFps, 10, 500, QStringLiteral("Maximum FPS"));
                if (gamma < 0.0 || gamma > 1.0)
                {
                    throw std::invalid_argument("Gamma must be between 0 and 1.");
                }

                QMap<QString, QString> changes{
                    {QStringLiteral("fov"),
                     strata::settings::MinecraftOptionValues::toStoredValue(
                         selected.versionId, QStringLiteral("fov"), QString::number(fov))},
                    {QStringLiteral("gamma"), QString::number(gamma, 'f', 2)},
                    {QStringLiteral("renderDistance"), QString::number(renderDistance)},
                    {QStringLiteral("simulationDistance"), QString::number(simulationDistance)},
                    {QStringLiteral("maxFps"),
                     unlimited ? QStringLiteral("260") : QString::number(maximumFps)},
                    {QStringLiteral("guiScale"), QString::number(guiScale)},
                    {QStringLiteral("fullscreen"),
                     booleanValue(options, QStringLiteral("fullscreen")) ? QStringLiteral("true")
                                                                         : QStringLiteral("false")},
                    {QStringLiteral("enableVsync"),
                     booleanValue(options, QStringLiteral("enableVsync"))
                         ? QStringLiteral("true")
                         : QStringLiteral("false")}};
                strata::settings::OptionsFile::setAll(optionsPath(selected), changes);
                return readState(selected);
            },
            [](InstanceSettingsController& self, const QVariant& result)
            {
                self.applyState(result.toMap());
                self.m_status = QStringLiteral("Game options saved.");
            });
    }

    void InstanceSettingsController::saveOption(const QString& key, const QString& value)
    {
        if (!ensureEditable())
        {
            return;
        }
        const strata::Instance selected = *m_instance;
        runAsync(
            selected, QStringLiteral("Saving advanced option…"),
            [selected, key, value]
            {
                strata::settings::OptionsFile::set(optionsPath(selected), key, value);
                return readState(selected);
            },
            [](InstanceSettingsController& self, const QVariant& result)
            {
                self.applyState(result.toMap());
                self.m_status = QStringLiteral("Advanced Minecraft option saved.");
            });
    }

    void InstanceSettingsController::saveControls(const QVariantMap& bindings)
    {
        if (!ensureEditable())
        {
            return;
        }
        QMap<QString, QString> changes;
        for (auto binding = bindings.cbegin(); binding != bindings.cend(); ++binding)
        {
            if (!strata::settings::MinecraftControls::isBindingKey(binding.key()) ||
                binding.value().metaType().id() != QMetaType::QString)
            {
                throw std::invalid_argument("Minecraft control bindings are invalid.");
            }
            changes.insert(binding.key(), binding.value().toString());
        }
        const strata::Instance selected = *m_instance;
        runAsync(
            selected, QStringLiteral("Saving controls…"),
            [selected, changes]
            {
                strata::settings::OptionsFile::setAll(optionsPath(selected), changes);
                return readState(selected);
            },
            [](InstanceSettingsController& self, const QVariant& result)
            {
                self.applyState(result.toMap());
                self.m_status = QStringLiteral("Controls saved to options.txt.");
            });
    }

    void InstanceSettingsController::addServer(const QString& name, const QString& address)
    {
        if (!ensureEditable())
        {
            return;
        }
        const strata::Instance selected = *m_instance;
        runAsync(
            selected, QStringLiteral("Adding server…"),
            [selected, name, address]
            {
                strata::settings::ServerListStore store(serversPath(selected), selected.versionId);
                store.add(name, address);
                store.save();
                return readServers(selected);
            },
            [](InstanceSettingsController& self, const QVariant& result)
            {
                self.m_servers = result.toList();
                self.m_status = QStringLiteral("Server added.");
            });
    }

    void InstanceSettingsController::updateServer(int index, const QString& name,
                                                  const QString& address)
    {
        if (!ensureEditable())
        {
            return;
        }
        const strata::Instance selected = *m_instance;
        runAsync(
            selected, QStringLiteral("Saving server…"),
            [selected, index, name, address]
            {
                strata::settings::ServerListStore store(serversPath(selected), selected.versionId);
                store.update(index, name, address);
                store.save();
                return readServers(selected);
            },
            [](InstanceSettingsController& self, const QVariant& result)
            {
                self.m_servers = result.toList();
                self.m_status = QStringLiteral("Server updated.");
            });
    }

    void InstanceSettingsController::removeServer(int index)
    {
        if (!ensureEditable())
        {
            return;
        }
        const strata::Instance selected = *m_instance;
        runAsync(
            selected, QStringLiteral("Removing server…"),
            [selected, index]
            {
                strata::settings::ServerListStore store(serversPath(selected), selected.versionId);
                store.remove(index);
                store.save();
                return readServers(selected);
            },
            [](InstanceSettingsController& self, const QVariant& result)
            {
                self.m_servers = result.toList();
                self.m_status = QStringLiteral("Server removed.");
            });
    }

    void InstanceSettingsController::saveJavaFlags(bool overridden, const QString& text)
    {
        if (!ensureEditable())
        {
            return;
        }
        const strata::Instance selected = *m_instance;
        runAsync(
            selected, QStringLiteral("Saving Java launch flags…"),
            [selected, overridden, text]
            {
                strata::InstanceSettings settings(selected.directory);
                if (overridden)
                {
                    settings.setJavaLaunchFlagsOverride(parseFlags(text));
                }
                else
                {
                    settings.clearJavaLaunchFlagsOverride();
                }
                return readState(selected);
            },
            [](InstanceSettingsController& self, const QVariant& result)
            {
                self.applyState(result.toMap());
                self.m_status = self.m_javaFlagsOverridden
                                    ? QStringLiteral("Instance Java flags saved.")
                                    : QStringLiteral("Instance now inherits launcher Java flags.");
            });
    }

    void InstanceSettingsController::runAsync(
        const strata::Instance& instance, const QString& status, std::function<QVariant()> action,
        std::function<void(InstanceSettingsController&, const QVariant&)> success)
    {
        m_busy = true;
        m_status = status;
        emit changed();
        const quint64 operationId = ++m_operationId;
        QPointer<InstanceSettingsController> self(this);
        m_workers.start(QRunnable::create(
            [self, operationId, instance, action = std::move(action), success = std::move(success)]
            {
                QVariant result;
                QString error;
                try
                {
                    result = action();
                }
                catch (const std::exception& exception)
                {
                    error = QString::fromUtf8(exception.what());
                }
                catch (...)
                {
                    error = QStringLiteral("An unknown error occurred.");
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, operationId, instance, result, error, success = std::move(success)]
                    {
                        if (self.isNull() || operationId != self->m_operationId)
                        {
                            return;
                        }
                        self->m_busy = false;
                        if (!error.isEmpty())
                        {
                            self->m_status = error;
                        }
                        else
                        {
                            self->m_instance = instance;
                            success(*self, result);
                        }
                        emit self->changed();
                    },
                    Qt::QueuedConnection);
            }));
    }

    bool InstanceSettingsController::ensureEditable()
    {
        if (m_busy)
        {
            m_status = QStringLiteral(
                "Wait for the current settings operation to finish before making another change.");
            emit changed();
            return false;
        }
        if (!m_instance)
        {
            m_status = QStringLiteral("Select an instance before editing its settings.");
            emit changed();
            return false;
        }
        return true;
    }

    QVariantMap InstanceSettingsController::readState(const strata::Instance& instance)
    {
        const QMap<QString, QString> options =
            strata::settings::OptionsFile::read(optionsPath(instance));
        QVariantList controls;
        const auto bindings = strata::settings::MinecraftControls::bindings(options);
        for (auto binding = bindings.cbegin(); binding != bindings.cend(); ++binding)
        {
            controls.append(
                QVariantMap{{QStringLiteral("key"), binding.key()},
                            {QStringLiteral("name"),
                             strata::settings::MinecraftControls::actionName(binding.key())},
                            {QStringLiteral("value"), binding.value()},
                            {QStringLiteral("display"),
                             strata::settings::MinecraftControls::displayKey(binding.value())}});
        }
        const strata::InstanceSettings settings(instance.directory);
        const QStringList defaults{QStringLiteral("-Xms1G"), QStringLiteral("-Xmx4G"),
                                   QStringLiteral("-XX:+UseG1GC")};
        return {{QStringLiteral("commonOptions"), commonOptions(instance, options)},
                {QStringLiteral("controls"), controls},
                {QStringLiteral("servers"), readServers(instance)},
                {QStringLiteral("javaFlags"),
                 settings.effectiveJavaLaunchFlags(defaults).join(QLatin1Char('\n'))},
                {QStringLiteral("javaFlagsOverridden"), settings.hasJavaLaunchFlagsOverride()}};
    }

    QVariantMap InstanceSettingsController::commonOptions(const strata::Instance& instance,
                                                          const QMap<QString, QString>& options)
    {
        const int maxFps = options.value(QStringLiteral("maxFps"), QStringLiteral("120")).toInt();
        const QString defaultFov =
            strata::settings::MinecraftOptionValues::usesNormalizedFov(instance.versionId)
                ? QStringLiteral("0.0")
                : QStringLiteral("70");
        return {{QStringLiteral("fov"), strata::settings::MinecraftOptionValues::toDisplayValue(
                                            instance.versionId, QStringLiteral("fov"),
                                            options.value(QStringLiteral("fov"), defaultFov))},
                {QStringLiteral("gamma"),
                 options.value(QStringLiteral("gamma"), QStringLiteral("0.0"))},
                {QStringLiteral("renderDistance"),
                 options.value(QStringLiteral("renderDistance"), QStringLiteral("12"))},
                {QStringLiteral("simulationDistance"),
                 options.value(QStringLiteral("simulationDistance"), QStringLiteral("12"))},
                {QStringLiteral("maxFps"), maxFps >= 260 ? 120 : maxFps},
                {QStringLiteral("unlimitedFps"), maxFps >= 260},
                {QStringLiteral("guiScale"),
                 options.value(QStringLiteral("guiScale"), QStringLiteral("0"))},
                {QStringLiteral("fullscreen"),
                 options.value(QStringLiteral("fullscreen"), QStringLiteral("false")) ==
                     QStringLiteral("true")},
                {QStringLiteral("enableVsync"),
                 options.value(QStringLiteral("enableVsync"), QStringLiteral("true")) ==
                     QStringLiteral("true")}};
    }

    QVariantList InstanceSettingsController::readServers(const strata::Instance& instance)
    {
        const strata::settings::ServerListStore store(serversPath(instance), instance.versionId);
        QVariantList result;
        for (const strata::settings::ServerListStore::Server& server : store.servers())
        {
            result.append(QVariantMap{{QStringLiteral("name"), server.name},
                                      {QStringLiteral("address"), server.address}});
        }
        return result;
    }

    QStringList InstanceSettingsController::parseFlags(const QString& text)
    {
        QStringList flags;
        for (const QString& line : text.split(QLatin1Char('\n')))
        {
            if (!line.trimmed().isEmpty())
            {
                flags.append(line.trimmed());
            }
        }
        return strata::InstanceSettings::validateFlags(flags);
    }

    void InstanceSettingsController::applyState(const QVariantMap& state)
    {
        m_commonOptions = state.value(QStringLiteral("commonOptions")).toMap();
        m_controls = state.value(QStringLiteral("controls")).toList();
        m_servers = state.value(QStringLiteral("servers")).toList();
        m_javaFlags = state.value(QStringLiteral("javaFlags")).toString();
        m_javaFlagsOverridden = state.value(QStringLiteral("javaFlagsOverridden")).toBool();
        m_status = QStringLiteral("Editing settings for %1.").arg(m_instanceName);
    }
} // namespace strata::ui
