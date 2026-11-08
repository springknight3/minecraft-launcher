#include "CredentialStore.h"

#include <qt6keychain/keychain.h>

#include <QEventLoop>
#include <QMutexLocker>

#include <stdexcept>

namespace strata::auth
{
    namespace
    {
        constexpr auto serviceName = "StrataLauncher/Microsoft";

        void waitForJob(QKeychain::Job& job)
        {
            QEventLoop loop;
            job.setAutoDelete(false);
            QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
            job.start();
            loop.exec();
            if (job.error() != QKeychain::NoError)
            {
                throw std::runtime_error(QStringLiteral("Operating-system credential store: %1")
                                             .arg(job.errorString())
                                             .toStdString());
            }
        }
    } // namespace

    void NativeCredentialStore::put(const QString& accountId, const QString& secret)
    {
        QMutexLocker lock(&m_mutex);
        QKeychain::WritePasswordJob job(QString::fromLatin1(serviceName));
        job.setKey(accountId);
        job.setTextData(secret);
        waitForJob(job);
    }

    QString NativeCredentialStore::get(const QString& accountId) const
    {
        QMutexLocker lock(&m_mutex);
        QKeychain::ReadPasswordJob job(QString::fromLatin1(serviceName));
        job.setKey(accountId);
        try
        {
            waitForJob(job);
        }
        catch (const std::runtime_error&)
        {
            if (job.error() == QKeychain::EntryNotFound)
            {
                return {};
            }
            throw;
        }
        return job.textData();
    }

    void NativeCredentialStore::remove(const QString& accountId)
    {
        QMutexLocker lock(&m_mutex);
        QKeychain::DeletePasswordJob job(QString::fromLatin1(serviceName));
        job.setKey(accountId);
        try
        {
            waitForJob(job);
        }
        catch (const std::runtime_error&)
        {
            if (job.error() != QKeychain::EntryNotFound)
            {
                throw;
            }
        }
    }
} // namespace strata::auth
