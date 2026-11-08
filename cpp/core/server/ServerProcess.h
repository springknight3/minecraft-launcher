#pragma once

#include "DevServerManager.h"

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTimer>

namespace strata::server
{
    class ServerProcess : public QObject
    {
        Q_OBJECT

    public:
        ServerProcess(QString serverId, ServerLaunchPlan plan, QObject* parent = nullptr);

        QString serverId() const;
        QString status() const;
        bool isRunning() const;
        QStringList consoleHistory() const;

        void start();
        void stop();
        void terminate();
        void sendCommand(const QString& command);

    signals:
        void outputReceived(const QString& serverId, const QString& line);
        void statusChanged(const QString& serverId, const QString& status);
        void snapshotChanged(const QString& serverId, const QString& status, bool running,
                             qint64 uptimeMilliseconds, qint64 residentMemoryBytes,
                             double cpuPercent, qint64 totalCpuMilliseconds);
        void finished(const QString& serverId, int exitCode, bool crashed);

    private:
        QString m_serverId;
        QTimer m_metricsTimer;
        QTimer m_stopTimer;
        QTimer m_killTimer;
        QElapsedTimer m_uptime;
        QElapsedTimer m_metricsElapsed;
        QString m_status = QStringLiteral("stopped");
        QByteArray m_outputBuffer;
        QStringList m_consoleHistory;
        qsizetype m_consoleCharacters = 0;
        qint64 m_residentMemoryBytes = -1;
        qint64 m_totalCpuMilliseconds = -1;
        qint64 m_previousCpuMilliseconds = -1;
        double m_cpuPercent = -1.0;
        bool m_stopRequested = false;
        bool m_metricsPending = false;
        QProcess m_process;

        void setStatus(QString status);
        void readOutput();
        void appendLine(QString line);
        void sampleMetrics();
        void publishSnapshot();
    };
} // namespace strata::server
