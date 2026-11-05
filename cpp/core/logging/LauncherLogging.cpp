#include "LauncherLogging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>

#include <cstdio>

namespace strata
{
    namespace
    {
        QMutex s_logMutex;
        QString s_logPath;
        constexpr qint64 maxLogBytes = 1'000'000;
        constexpr int retainedLogCount = 3;

        void rotateLogs()
        {
            const QFileInfo current(s_logPath);
            if (!current.exists() || current.size() < maxLogBytes)
            {
                return;
            }
            QFile::remove(s_logPath + QStringLiteral(".%1").arg(retainedLogCount));
            for (int index = retainedLogCount - 1; index >= 1; --index)
            {
                QFile::remove(s_logPath + QStringLiteral(".%1").arg(index + 1));
                QFile::rename(s_logPath + QStringLiteral(".%1").arg(index),
                              s_logPath + QStringLiteral(".%1").arg(index + 1));
            }
            QFile::rename(s_logPath, s_logPath + QStringLiteral(".1"));
        }

        void messageHandler(QtMsgType type, const QMessageLogContext& context,
                            const QString& message)
        {
            QString level;
            switch (type)
            {
            case QtDebugMsg:
                level = QStringLiteral("DEBUG");
                break;
            case QtInfoMsg:
                level = QStringLiteral("INFO");
                break;
            case QtWarningMsg:
                level = QStringLiteral("WARNING");
                break;
            case QtCriticalMsg:
                level = QStringLiteral("CRITICAL");
                break;
            case QtFatalMsg:
                level = QStringLiteral("FATAL");
                break;
            }
            const QString category = context.category == nullptr
                                         ? QStringLiteral("default")
                                         : QString::fromUtf8(context.category);
            const QByteArray line =
                QStringLiteral("%1 [%2] [%3] %4\n")
                    .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs), level,
                         category, LauncherLogging::redact(message))
                    .toUtf8();
            {
                QMutexLocker lock(&s_logMutex);
                if (!s_logPath.isEmpty())
                {
                    rotateLogs();
                    QFile file(s_logPath);
                    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
                    {
                        std::fprintf(stderr, "Unable to append launcher log: %s\n",
                                     qPrintable(file.errorString()));
                    }
                    else if (file.write(line) != line.size() || !file.flush())
                    {
                        std::fprintf(stderr, "Unable to write launcher log: %s\n",
                                     qPrintable(file.errorString()));
                    }
                }
            }
            if (type == QtFatalMsg)
            {
                abort();
            }
        }
    } // namespace

    bool LauncherLogging::configure(const QString& dataDirectory, QString* error)
    {
        const QString logDirectory =
            QDir(QFileInfo(dataDirectory).absoluteFilePath()).filePath(QStringLiteral("logs"));
        if (!QDir().mkpath(logDirectory))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("Unable to create log directory: %1").arg(logDirectory);
            }
            return false;
        }
        const QString logPath = QDir(logDirectory).filePath(QStringLiteral("stratalauncher.log"));
        QFile probe(logPath);
        if (!probe.open(QIODevice::WriteOnly | QIODevice::Append))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("Unable to open log file %1: %2")
                             .arg(logPath, probe.errorString());
            }
            return false;
        }
        probe.close();
        {
            QMutexLocker lock(&s_logMutex);
            s_logPath = logPath;
        }
        qInstallMessageHandler(messageHandler);
        return true;
    }

    QString LauncherLogging::redact(const QString& message)
    {
        static const QRegularExpression sensitiveValue(
            QStringLiteral(
                "((?:access[_-]?token|refresh[_-]?token|client[_-]?secret|api[_-]?key|password)"
                "\\b\\s*(?:=|:)\\s*|--(?:accessToken|refreshToken|clientSecret|apiKey|password)\\s+"
                ")"
                "[^\\s,;]+"),
            QRegularExpression::CaseInsensitiveOption);
        QString redacted = message;
        redacted.replace(sensitiveValue, QStringLiteral("\\1[REDACTED]"));
        return redacted;
    }
} // namespace strata
