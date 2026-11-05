#include "DownloadManager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QRunnable>
#include <QSaveFile>
#include <QSet>
#include <QThread>
#include <QThreadPool>
#include <QTimer>

#include <atomic>
#include <exception>
#include <stdexcept>

namespace strata::download
{
    namespace
    {
        constexpr int requestTimeoutMs = 180'000;
        constexpr qint64 maxChunkBytes = 1024 * 1024;

        [[noreturn]] void throwDownloadError(const QString& message)
        {
            throw std::runtime_error(message.toStdString());
        }

        QByteArray sha1ForFile(const QString& path)
        {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                throwDownloadError(QStringLiteral("Unable to read existing download %1: %2")
                                       .arg(path, file.errorString()));
            }
            QCryptographicHash hash(QCryptographicHash::Sha1);
            while (!file.atEnd())
            {
                const QByteArray chunk = file.read(maxChunkBytes);
                if (chunk.isEmpty() && file.error() != QFileDevice::NoError)
                {
                    throwDownloadError(QStringLiteral("Unable to hash existing download %1: %2")
                                           .arg(path, file.errorString()));
                }
                hash.addData(chunk);
            }
            return hash.result().toHex();
        }

        void validateTask(const DownloadTask& task)
        {
            if (!task.source.isValid() || (task.source.scheme() != QStringLiteral("https") &&
                                           task.source.scheme() != QStringLiteral("http")))
            {
                throw std::invalid_argument("Download source must use HTTP or HTTPS");
            }
            static const QRegularExpression checksumPattern(QStringLiteral("^[0-9a-fA-F]{40}$"));
            if (!task.expectedSha1.isEmpty() &&
                !checksumPattern.match(task.expectedSha1).hasMatch())
            {
                throw std::invalid_argument(
                    "Expected SHA-1 must contain 40 hexadecimal characters");
            }
            if (task.destination.trimmed().isEmpty() || task.description.trimmed().isEmpty())
            {
                throw std::invalid_argument(
                    "Download destination and description must not be blank");
            }
            if (task.maxBytes <= 0)
            {
                throw std::invalid_argument("Download maximum size must be positive");
            }
        }

        void writeResponseChunk(QNetworkReply* reply, QSaveFile& file, QCryptographicHash& hash,
                                qint64 maxBytes, qint64& written, QString& writeError)
        {
            while (reply->bytesAvailable() > 0)
            {
                const QByteArray chunk = reply->read(maxChunkBytes);
                if (chunk.isEmpty())
                {
                    break;
                }
                if (written > maxBytes - chunk.size())
                {
                    writeError = QStringLiteral("Download exceeded size limit");
                    reply->abort();
                    return;
                }
                if (file.write(chunk) != chunk.size())
                {
                    writeError =
                        QStringLiteral("Unable to write download: %1").arg(file.errorString());
                    reply->abort();
                    return;
                }
                hash.addData(chunk);
                written += chunk.size();
            }
        }
    } // namespace

    double Progress::fraction() const
    {
        return total == 0 ? 1.0 : static_cast<double>(completed) / static_cast<double>(total);
    }

    DownloadManager::DownloadManager(int concurrency, int retries)
        : m_concurrency(concurrency), m_retries(retries)
    {
        if (m_concurrency < 1)
        {
            throw std::invalid_argument("Download concurrency must be at least one");
        }
        if (m_retries < 1)
        {
            throw std::invalid_argument("Download retries must be at least one");
        }
    }

    void DownloadManager::downloadAll(const QList<DownloadTask>& tasks,
                                      ProgressCallback progress) const
    {
        QSet<QString> destinations;
        for (const DownloadTask& task : tasks)
        {
            validateTask(task);
            const QString destination =
                QDir::cleanPath(QFileInfo(task.destination).absoluteFilePath());
            if (destinations.contains(destination))
            {
                throw std::invalid_argument(QStringLiteral("Multiple download tasks target %1")
                                                .arg(destination)
                                                .toStdString());
            }
            destinations.insert(destination);
        }
        if (tasks.isEmpty())
        {
            if (progress)
            {
                progress({0, 0, QStringLiteral("Nothing to download")});
            }
            return;
        }

        QThreadPool pool;
        pool.setMaxThreadCount(m_concurrency);
        std::atomic_bool cancelled{false};
        std::atomic_int completed{0};
        QMutex progressMutex;
        QMutex failureMutex;
        std::exception_ptr failure;
        for (const DownloadTask& task : tasks)
        {
            pool.start(QRunnable::create(
                [this, task, total = static_cast<int>(tasks.size()), &progress, &cancelled,
                 &completed, &progressMutex, &failureMutex, &failure]()
                {
                    if (cancelled.load())
                    {
                        return;
                    }
                    try
                    {
                        if (progress)
                        {
                            QMutexLocker lock(&progressMutex);
                            progress({completed.load(), total, task.description});
                        }
                        downloadOne(task, cancelled);
                        if (progress)
                        {
                            QMutexLocker lock(&progressMutex);
                            const int count = completed.fetch_add(1) + 1;
                            progress({count, total, task.description});
                        }
                        else
                        {
                            completed.fetch_add(1);
                        }
                    }
                    catch (...)
                    {
                        cancelled.store(true);
                        QMutexLocker lock(&failureMutex);
                        if (!failure)
                        {
                            failure = std::current_exception();
                        }
                    }
                }));
        }
        pool.waitForDone();
        if (failure)
        {
            std::rethrow_exception(failure);
        }
    }

    void DownloadManager::downloadOne(const DownloadTask& task,
                                      const std::atomic_bool& cancelled) const
    {
        const QString destination = QDir::cleanPath(QFileInfo(task.destination).absoluteFilePath());
        const QFileInfo existing(destination);
        if (existing.isFile() &&
            (task.expectedSha1.isEmpty() ||
             sha1ForFile(destination).compare(task.expectedSha1.toLatin1(), Qt::CaseInsensitive) ==
                 0))
        {
            return;
        }
        const QString parentDirectory = QFileInfo(destination).absolutePath();
        if (!QDir().mkpath(parentDirectory))
        {
            throwDownloadError(
                QStringLiteral("Unable to create download directory %1").arg(parentDirectory));
        }

        QString lastError;
        for (int attempt = 1; attempt <= m_retries; ++attempt)
        {
            if (cancelled.load())
            {
                throwDownloadError(QStringLiteral("Download cancelled after another task failed"));
            }
            QSaveFile output(destination);
            output.setDirectWriteFallback(false);
            if (!output.open(QIODevice::WriteOnly))
            {
                throwDownloadError(QStringLiteral("Unable to open download destination %1: %2")
                                       .arg(destination, output.errorString()));
            }
            QNetworkAccessManager manager;
            QNetworkRequest request(task.source);
            request.setHeader(QNetworkRequest::UserAgentHeader,
                              QStringLiteral("StrataLauncher/0.1.0"));
            request.setRawHeader("Accept", "*/*");
            request.setTransferTimeout(requestTimeoutMs);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);
            QNetworkReply* reply = manager.get(request);
            QCryptographicHash hash(QCryptographicHash::Sha1);
            qint64 written = 0;
            QString writeError;
            QTimer cancellationTimer;
            cancellationTimer.setInterval(100);
            QObject::connect(&cancellationTimer, &QTimer::timeout, reply,
                             [&]()
                             {
                                 if (cancelled.load())
                                 {
                                     reply->abort();
                                 }
                             });
            cancellationTimer.start();
            QObject::connect(
                reply, &QIODevice::readyRead, reply, [&]()
                { writeResponseChunk(reply, output, hash, task.maxBytes, written, writeError); });
            QEventLoop eventLoop;
            QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
            eventLoop.exec();
            cancellationTimer.stop();
            writeResponseChunk(reply, output, hash, task.maxBytes, written, writeError);
            const int statusCode =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QString networkError = reply->errorString();
            const bool networkFailed = reply->error() != QNetworkReply::NoError;
            reply->deleteLater();

            if (cancelled.load())
            {
                output.cancelWriting();
                throwDownloadError(QStringLiteral("Download cancelled after another task failed"));
            }
            if (writeError.isEmpty() && !networkFailed && statusCode >= 200 && statusCode < 300 &&
                (task.expectedSha1.isEmpty() ||
                 hash.result().toHex().compare(task.expectedSha1.toLatin1(), Qt::CaseInsensitive) ==
                     0) &&
                output.commit())
            {
                return;
            }
            lastError = !writeError.isEmpty() ? writeError
                        : networkFailed       ? networkError
                        : statusCode < 200 || statusCode >= 300
                            ? QStringLiteral("Server returned HTTP %1").arg(statusCode)
                        : !task.expectedSha1.isEmpty() &&
                                hash.result().toHex().compare(task.expectedSha1.toLatin1(),
                                                              Qt::CaseInsensitive) != 0
                            ? QStringLiteral("SHA-1 checksum mismatch")
                            : output.errorString();
            output.cancelWriting();
            if (attempt < m_retries)
            {
                QThread::msleep(static_cast<unsigned long>(250 * attempt));
            }
        }
        throwDownloadError(QStringLiteral("Download failed after %1 attempts for %2: %3")
                               .arg(m_retries)
                               .arg(task.source.toString(), lastError));
    }
} // namespace strata::download
