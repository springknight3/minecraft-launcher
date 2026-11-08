#pragma once

#include <QMap>
#include <QString>

namespace strata::settings
{
    class OptionsFile
    {
    public:
        static QMap<QString, QString> read(const QString& path);
        static QMap<QString, QString> parse(const QString& content);
        static void set(const QString& path, const QString& key, const QString& value);
        static void setAll(const QString& path, const QMap<QString, QString>& changes);
        static QString update(const QString& content, const QString& key, const QString& value);

    private:
        static void validateKey(const QString& key);
    };
} // namespace strata::settings
