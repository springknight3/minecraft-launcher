#include "GameProcess.h"

namespace strata::launch
{
    GameProcess::GameProcess(QString instanceId, LaunchPlan plan, QObject* parent)
        : QObject(parent), m_instanceId(std::move(instanceId)), m_plan(std::move(plan))
    {
        m_process.setProcessChannelMode(QProcess::MergedChannels);
        m_process.setProgram(m_plan.program);
        m_process.setArguments(m_plan.arguments);
        m_process.setWorkingDirectory(m_plan.workingDirectory);

        m_killTimer.setSingleShot(true);
        m_killTimer.setInterval(8'000);
        connect(&m_killTimer, &QTimer::timeout, &m_process, &QProcess::kill);
        connect(&m_process, &QProcess::readyReadStandardOutput, this, &GameProcess::readOutput);
        connect(&m_process, &QProcess::started, this,
                [this]()
                {
                    setStatus(QStringLiteral("starting"));
                    if (m_stopRequested)
                    {
                        m_process.terminate();
                        m_killTimer.start();
                    }
                });
        connect(&m_process, &QProcess::errorOccurred, this,
                [this](QProcess::ProcessError error)
                {
                    if (error == QProcess::FailedToStart)
                    {
                        setStatus(QStringLiteral("crashed"));
                        emit finished(m_instanceId, -1, true);
                    }
                });
        connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                [this](int exitCode, QProcess::ExitStatus exitStatus)
                {
                    m_killTimer.stop();
                    readOutput();
                    const bool crashed =
                        !m_stopRequested && (exitStatus != QProcess::NormalExit || exitCode != 0);
                    setStatus(crashed ? QStringLiteral("crashed") : QStringLiteral("stopped"));
                    emit finished(m_instanceId, exitCode, crashed);
                });
    }

    QString GameProcess::instanceId() const
    {
        return m_instanceId;
    }

    QString GameProcess::status() const
    {
        return m_status;
    }

    bool GameProcess::isRunning() const
    {
        return m_process.state() == QProcess::Running;
    }

    void GameProcess::start()
    {
        if (m_process.state() != QProcess::NotRunning)
        {
            return;
        }
        m_stopRequested = false;
        m_readinessBuffer.clear();
        setStatus(QStringLiteral("starting"));
        m_process.start();
    }

    void GameProcess::stop()
    {
        if (m_process.state() == QProcess::NotRunning)
        {
            return;
        }
        m_stopRequested = true;
        if (m_process.state() == QProcess::Starting)
        {
            return;
        }
        m_process.terminate();
        m_killTimer.start();
    }

    void GameProcess::setStatus(QString status)
    {
        if (m_status == status)
        {
            return;
        }
        m_status = std::move(status);
        emit statusChanged(m_instanceId, m_status);
    }

    void GameProcess::readOutput()
    {
        const QByteArray output = m_process.readAllStandardOutput();
        if (output.isEmpty())
        {
            return;
        }
        emit logOutputReceived(m_plan.logFile, output);
        emit outputReceived(m_instanceId, QString::fromUtf8(output));

        m_readinessBuffer.append(output);
        constexpr qsizetype maxReadinessBuffer = 64 * 1024;
        if (m_readinessBuffer.size() > maxReadinessBuffer)
        {
            m_readinessBuffer.remove(0, m_readinessBuffer.size() - maxReadinessBuffer);
        }
        qsizetype newline = -1;
        while ((newline = m_readinessBuffer.indexOf('\n')) >= 0)
        {
            const QByteArray line = m_readinessBuffer.left(newline);
            m_readinessBuffer.remove(0, newline + 1);
            if (GameLauncher::isGameReadyMarker(line))
            {
                setStatus(QStringLiteral("ready"));
            }
        }
        if (GameLauncher::isGameReadyMarker(m_readinessBuffer))
        {
            setStatus(QStringLiteral("ready"));
        }
    }

} // namespace strata::launch
