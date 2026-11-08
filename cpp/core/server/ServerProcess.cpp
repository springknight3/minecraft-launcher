#include "ServerProcess.h"

#include <QFile>
#include <QMetaObject>
#include <QPointer>
#include <QRunnable>
#include <QThread>
#include <QThreadPool>

#include <algorithm>
#include <stdexcept>

#ifdef Q_OS_LINUX
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <psapi.h>
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <QRegularExpression>
#endif

namespace strata::server
{
    namespace
    {
        constexpr qsizetype maxConsoleLines = 2'000;
        constexpr qsizetype maxConsoleCharacters = 100'000;
        constexpr qsizetype maxOutputBuffer = 64 * 1024;
        constexpr int metricsIntervalMs = 1'000;

        struct ProcessMetrics
        {
            qint64 residentMemoryBytes = -1;
            qint64 totalCpuMilliseconds = -1;
            double cpuPercent = -1.0;
        };

#ifdef Q_OS_LINUX
        ProcessMetrics readProcessMetrics(qint64 processId)
        {
            ProcessMetrics metrics;
            QFile statusFile(QStringLiteral("/proc/%1/status").arg(processId));
            if (statusFile.open(QIODevice::ReadOnly))
            {
                while (true)
                {
                    const QByteArray line = statusFile.readLine();
                    if (line.isEmpty())
                    {
                        break;
                    }
                    if (!line.startsWith("VmRSS:"))
                    {
                        continue;
                    }
                    const QList<QByteArray> fields =
                        line.mid(line.indexOf(':') + 1).simplified().split(' ');
                    if (!fields.isEmpty())
                    {
                        bool ok = false;
                        const qint64 kibibytes = fields.constFirst().toLongLong(&ok);
                        if (ok)
                        {
                            metrics.residentMemoryBytes = kibibytes * 1024;
                        }
                    }
                    break;
                }
            }

            QFile statFile(QStringLiteral("/proc/%1/stat").arg(processId));
            if (statFile.open(QIODevice::ReadOnly))
            {
                const QByteArray stat = statFile.read(16 * 1024);
                const qsizetype closingParenthesis = stat.lastIndexOf(')');
                if (closingParenthesis >= 0)
                {
                    const QList<QByteArray> fields =
                        stat.mid(closingParenthesis + 1).simplified().split(' ');
                    bool userOk = false;
                    bool systemOk = false;
                    const qint64 userTicks =
                        fields.size() > 11 ? fields.at(11).toLongLong(&userOk) : 0;
                    const qint64 systemTicks =
                        fields.size() > 12 ? fields.at(12).toLongLong(&systemOk) : 0;
                    const long ticksPerSecond = sysconf(_SC_CLK_TCK);
                    if (userOk && systemOk && ticksPerSecond > 0)
                    {
                        metrics.totalCpuMilliseconds =
                            (userTicks + systemTicks) * 1'000 / ticksPerSecond;
                    }
                }
            }
            return metrics;
        }
#elif defined(Q_OS_WIN)
        ProcessMetrics readProcessMetrics(qint64 processId)
        {
            ProcessMetrics metrics;
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE,
                                         static_cast<DWORD>(processId));
            if (process == nullptr)
            {
                return metrics;
            }
            PROCESS_MEMORY_COUNTERS_EX memory{};
            memory.cb = sizeof(memory);
            if (GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                                     sizeof(memory)))
            {
                metrics.residentMemoryBytes = static_cast<qint64>(memory.WorkingSetSize);
            }
            FILETIME creationTime{};
            FILETIME exitTime{};
            FILETIME kernelTime{};
            FILETIME userTime{};
            if (GetProcessTimes(process, &creationTime, &exitTime, &kernelTime, &userTime))
            {
                ULARGE_INTEGER kernel{};
                ULARGE_INTEGER user{};
                kernel.LowPart = kernelTime.dwLowDateTime;
                kernel.HighPart = kernelTime.dwHighDateTime;
                user.LowPart = userTime.dwLowDateTime;
                user.HighPart = userTime.dwHighDateTime;
                metrics.totalCpuMilliseconds =
                    static_cast<qint64>((kernel.QuadPart + user.QuadPart) / 10'000);
            }
            CloseHandle(process);
            return metrics;
        }
#elif defined(Q_OS_MACOS)
        ProcessMetrics readProcessMetrics(qint64 processId)
        {
            ProcessMetrics metrics;
            QProcess process;
            process.start(QStringLiteral("/bin/ps"),
                          {QStringLiteral("-o"), QStringLiteral("rss=,time="), QStringLiteral("-p"),
                           QString::number(processId)});
            if (!process.waitForFinished(1'000))
            {
                process.kill();
                process.waitForFinished(1'000);
                return metrics;
            }
            const QStringList fields =
                QString::fromUtf8(process.readAllStandardOutput())
                    .trimmed()
                    .split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            if (fields.size() >= 2)
            {
                bool memoryOk = false;
                const qint64 kibibytes = fields.first().toLongLong(&memoryOk);
                if (memoryOk)
                {
                    metrics.residentMemoryBytes = kibibytes * 1024;
                }
                const QStringList timeParts = fields.at(1).split(QLatin1Char(':'));
                bool secondsOk = false;
                const qint64 seconds = timeParts.constLast().toLongLong(&secondsOk);
                if (secondsOk && timeParts.size() >= 2)
                {
                    bool minutesOk = false;
                    const qint64 minutes =
                        timeParts.at(timeParts.size() - 2).toLongLong(&minutesOk);
                    if (minutesOk)
                    {
                        const qint64 hours = timeParts.size() >= 3
                                                 ? timeParts.at(timeParts.size() - 3).toLongLong()
                                                 : 0;
                        metrics.totalCpuMilliseconds =
                            ((hours * 60 + minutes) * 60 + seconds) * 1'000;
                    }
                }
            }
            return metrics;
        }
#else
        ProcessMetrics readProcessMetrics(qint64)
        {
            return {};
        }
#endif
    } // namespace

    ServerProcess::ServerProcess(QString serverId, ServerLaunchPlan plan, QObject* parent)
        : QObject(parent), m_serverId(std::move(serverId))
    {
        m_process.setProcessChannelMode(QProcess::MergedChannels);
        m_process.setProgram(plan.program);
        m_process.setArguments(plan.arguments);
        m_process.setWorkingDirectory(plan.workingDirectory);

        m_metricsTimer.setInterval(metricsIntervalMs);
        connect(&m_metricsTimer, &QTimer::timeout, this, &ServerProcess::sampleMetrics);
        m_stopTimer.setSingleShot(true);
        m_killTimer.setSingleShot(true);
        connect(&m_stopTimer, &QTimer::timeout, this,
                [this]
                {
                    if (isRunning())
                    {
                        m_process.terminate();
                        m_killTimer.start(3'000);
                    }
                });
        connect(&m_killTimer, &QTimer::timeout, &m_process, &QProcess::kill);
        connect(&m_process, &QProcess::started, this,
                [this]
                {
                    m_uptime.start();
                    m_metricsElapsed.start();
                    m_metricsTimer.start();
                    setStatus(QStringLiteral("starting"));
                    if (m_stopRequested)
                    {
                        stop();
                    }
                    publishSnapshot();
                });
        connect(&m_process, &QProcess::readyReadStandardOutput, this, &ServerProcess::readOutput);
        connect(&m_process, &QProcess::errorOccurred, this,
                [this](QProcess::ProcessError error)
                {
                    if (error == QProcess::FailedToStart)
                    {
                        setStatus(QStringLiteral("crashed"));
                        emit finished(m_serverId, -1, true);
                        publishSnapshot();
                    }
                });
        connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                [this](int exitCode, QProcess::ExitStatus exitStatus)
                {
                    m_stopTimer.stop();
                    m_killTimer.stop();
                    m_metricsTimer.stop();
                    readOutput();
                    if (!m_outputBuffer.isEmpty())
                    {
                        appendLine(QString::fromUtf8(m_outputBuffer));
                        m_outputBuffer.clear();
                    }
                    const bool crashed =
                        !m_stopRequested && (exitStatus != QProcess::NormalExit || exitCode != 0);
                    setStatus(crashed ? QStringLiteral("crashed") : QStringLiteral("stopped"));
                    emit finished(m_serverId, exitCode, crashed);
                    publishSnapshot();
                });
    }

    QString ServerProcess::serverId() const
    {
        return m_serverId;
    }

    QString ServerProcess::status() const
    {
        return m_status;
    }

    bool ServerProcess::isRunning() const
    {
        return m_process.state() != QProcess::NotRunning;
    }

    QStringList ServerProcess::consoleHistory() const
    {
        return m_consoleHistory;
    }

    void ServerProcess::start()
    {
        if (m_process.state() != QProcess::NotRunning)
        {
            return;
        }
        m_stopRequested = false;
        m_outputBuffer.clear();
        m_consoleHistory.clear();
        m_consoleCharacters = 0;
        m_residentMemoryBytes = -1;
        m_totalCpuMilliseconds = -1;
        m_previousCpuMilliseconds = -1;
        m_cpuPercent = -1.0;
        setStatus(QStringLiteral("starting"));
        m_process.start();
    }

    void ServerProcess::stop()
    {
        if (!isRunning())
        {
            return;
        }
        m_stopRequested = true;
        if (m_process.state() == QProcess::Starting)
        {
            return;
        }
        sendCommand(QStringLiteral("stop"));
        m_stopTimer.start(10'000);
    }

    void ServerProcess::terminate()
    {
        if (!isRunning())
        {
            return;
        }
        m_stopRequested = true;
        m_process.kill();
    }

    void ServerProcess::sendCommand(const QString& command)
    {
        if (command.trimmed().isEmpty() || command.contains(QLatin1Char('\n')) ||
            command.contains(QLatin1Char('\r')) || command.contains(QChar::Null))
        {
            throw std::invalid_argument("Enter one non-empty server command per line.");
        }
        if (m_process.state() != QProcess::Running)
        {
            throw std::runtime_error("The selected server is not running.");
        }
        const QByteArray bytes = command.toUtf8() + '\n';
        if (m_process.write(bytes) != bytes.size())
        {
            throw std::runtime_error("Could not send command to the server process.");
        }
    }

    void ServerProcess::setStatus(QString status)
    {
        if (m_status == status)
        {
            return;
        }
        m_status = std::move(status);
        emit statusChanged(m_serverId, m_status);
        publishSnapshot();
    }

    void ServerProcess::readOutput()
    {
        m_outputBuffer.append(m_process.readAllStandardOutput());
        qsizetype newline = -1;
        while ((newline = m_outputBuffer.indexOf('\n')) >= 0)
        {
            QByteArray line = m_outputBuffer.left(newline);
            m_outputBuffer.remove(0, newline + 1);
            if (line.endsWith('\r'))
            {
                line.chop(1);
            }
            appendLine(QString::fromUtf8(line));
        }
        while (m_outputBuffer.size() > maxOutputBuffer)
        {
            const QByteArray line = m_outputBuffer.left(maxOutputBuffer);
            m_outputBuffer.remove(0, maxOutputBuffer);
            appendLine(QStringLiteral("[output truncated] ") + QString::fromUtf8(line));
        }
    }

    void ServerProcess::appendLine(QString line)
    {
        if (line.size() > 8'192)
        {
            line.truncate(8'192);
            line.append(QStringLiteral(" [line truncated]"));
        }
        m_consoleHistory.append(line);
        m_consoleCharacters += line.size() + 1;
        while (m_consoleHistory.size() > maxConsoleLines ||
               m_consoleCharacters > maxConsoleCharacters)
        {
            m_consoleCharacters -= m_consoleHistory.constFirst().size() + 1;
            m_consoleHistory.removeFirst();
        }
        emit outputReceived(m_serverId, line);
        if (m_status == QStringLiteral("starting") &&
            (line.contains(QStringLiteral("Done (")) ||
             line.contains(QStringLiteral("For help, type \"help\""))))
        {
            setStatus(QStringLiteral("running"));
        }
    }

    void ServerProcess::sampleMetrics()
    {
        if (!isRunning() || m_metricsPending)
        {
            return;
        }
        m_metricsPending = true;
        const qint64 processId = m_process.processId();
        QPointer<ServerProcess> self(this);
        QThreadPool::globalInstance()->start(QRunnable::create(
            [self, processId]
            {
                const ProcessMetrics metrics = readProcessMetrics(processId);
                if (self.isNull())
                {
                    return;
                }
                QMetaObject::invokeMethod(
                    self,
                    [self, metrics]
                    {
                        if (self.isNull())
                        {
                            return;
                        }
                        self->m_metricsPending = false;
                        self->m_residentMemoryBytes = metrics.residentMemoryBytes;
                        const qint64 elapsed = self->m_metricsElapsed.restart();
                        if (metrics.cpuPercent >= 0.0)
                        {
                            self->m_cpuPercent = metrics.cpuPercent;
                        }
                        else if (metrics.totalCpuMilliseconds >= 0 &&
                                 self->m_previousCpuMilliseconds >= 0 && elapsed > 0)
                        {
                            const qint64 cpuDelta =
                                metrics.totalCpuMilliseconds - self->m_previousCpuMilliseconds;
                            self->m_cpuPercent =
                                std::clamp(static_cast<double>(cpuDelta) * 100.0 /
                                               (static_cast<double>(elapsed) *
                                                std::max(1, QThread::idealThreadCount())),
                                           0.0, 100.0);
                        }
                        if (metrics.totalCpuMilliseconds >= 0)
                        {
                            self->m_totalCpuMilliseconds = metrics.totalCpuMilliseconds;
                            self->m_previousCpuMilliseconds = metrics.totalCpuMilliseconds;
                        }
                        self->publishSnapshot();
                    },
                    Qt::QueuedConnection);
            }));
    }

    void ServerProcess::publishSnapshot()
    {
        const qint64 uptime = m_uptime.isValid() ? m_uptime.elapsed() : 0;
        emit snapshotChanged(m_serverId, m_status, isRunning(), uptime, m_residentMemoryBytes,
                             m_cpuPercent, m_totalCpuMilliseconds);
    }
} // namespace strata::server
