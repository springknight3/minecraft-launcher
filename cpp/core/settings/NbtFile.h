#pragma once

#include <QByteArray>
#include <QPair>
#include <QString>
#include <QVector>

#include <memory>
#include <variant>

namespace strata::settings
{
    struct NbtList;
    struct NbtCompound;

    struct NbtTag
    {
        using Value = std::variant<std::monostate, qint8, qint16, qint32, qint64, float, double,
                                   QByteArray, QString, std::shared_ptr<NbtList>,
                                   std::shared_ptr<NbtCompound>, QVector<qint32>, QVector<qint64>>;

        quint8 type = 0;
        Value value;
    };

    struct NbtList
    {
        quint8 elementType = 0;
        QVector<NbtTag> elements;
    };

    struct NbtCompound
    {
        QVector<QPair<QString, NbtTag>> entries;

        const NbtTag* find(const QString& name) const;
        NbtTag* find(const QString& name);
        void insert(const QString& name, const NbtTag& tag);
    };

    struct NbtDocument
    {
        QString rootName;
        NbtCompound root;
    };

    class NbtFile
    {
    public:
        static NbtDocument read(const QString& path);
        static void write(const QString& path, const NbtDocument& document, bool compressed);
        static bool isCompressed(const QString& path);
    };
} // namespace strata::settings
