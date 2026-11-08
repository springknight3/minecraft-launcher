#pragma once

#include <QMap>
#include <QStringList>

namespace strata::settings
{
    class MinecraftControls
    {
    public:
        static QMap<QString, QString> defaultBindings();
        static QMap<QString, QString> bindings(const QMap<QString, QString>& options);
        static QMap<QString, QStringList> conflicts(const QMap<QString, QString>& bindings);
        static QString actionName(const QString& optionKey);
        static QString displayKey(const QString& value);
        static bool isBindingKey(const QString& key);
    };
} // namespace strata::settings
