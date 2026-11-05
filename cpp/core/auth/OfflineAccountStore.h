#pragma once

#include <QString>

namespace strata::auth
{
struct OfflineAccount
{
    QString name;
    QString uuid;
};

class OfflineAccountStore
{
public:
    explicit OfflineAccountStore(QString dataDirectory);

    QString username() const;
    OfflineAccount setUsername(const QString &name) const;
    static QString validateUsername(const QString &name);
    static QString offlineUuid(const QString &name);

private:
    QString m_accountsFile;
};
} // namespace strata::auth
