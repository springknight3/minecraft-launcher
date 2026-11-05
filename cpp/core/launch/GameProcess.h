#pragma once

#include "GameLauncher.h"

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QTimer>

namespace strata::launch
{
    class GameProcess : public QObject
    {
        Q_OBJECT

    public:
        GameProcess(QString instanceId, LaunchPlan plan, QObject* parent = nullptr);

        QString instanceId() const;
        QString status() const;
        bool isRunning() const;

        Q_INVOKABLE void start();
        Q_INVOKABLE void stop();

    signals:
        void statusChanged(const QString& instanceId, const QString& status);
        void outputReceived(const QString& instanceId, const QString& text);
        void logOutputReceived(const QString& logFile, const QByteArray& output);
        void finished(const QString& instanceId, int exitCode, bool crashed);

    private:
        QString m_instanceId;
        LaunchPlan m_plan;
        QProcess m_process;
        QTimer m_killTimer;
        QString m_status = QStringLiteral("stopped");
        QByteArray m_readinessBuffer;
        bool m_stopRequested = false;

        void setStatus(QString status);
        void readOutput();
    };
} // namespace strata::launch
