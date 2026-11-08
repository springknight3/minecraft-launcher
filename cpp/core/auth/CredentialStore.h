#pragma once

#include <QMutex>
#include <QString>

namespace strata::auth
{
    class CredentialStore
    {
    public:
        virtual ~CredentialStore() = default;
        virtual void put(const QString& accountId, const QString& secret) = 0;
        virtual QString get(const QString& accountId) const = 0;
        virtual void remove(const QString& accountId) = 0;
    };

    class NativeCredentialStore final : public CredentialStore
    {
    public:
        void put(const QString& accountId, const QString& secret) override;
        QString get(const QString& accountId) const override;
        void remove(const QString& accountId) override;

    private:
        mutable QMutex m_mutex;
    };
} // namespace strata::auth
