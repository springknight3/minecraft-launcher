#include "NbtFile.h"

#include <QBuffer>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <zlib.h>

#include <array>
#include <bit>
#include <limits>
#include <stdexcept>

namespace strata::settings
{
    namespace
    {
        constexpr qint64 maxNbtBytes = 64 * 1024 * 1024;
        constexpr qint32 maxNbtCollectionSize = 2'000'000;
        constexpr int maxNbtDepth = 64;

        [[noreturn]] void throwNbtError(const QString& message, const QString& path = {},
                                        const QString& detail = {})
        {
            QString fullMessage = message;
            if (!path.isEmpty())
            {
                fullMessage += QStringLiteral(": ") + path;
            }
            if (!detail.isEmpty())
            {
                fullMessage += QStringLiteral(" (") + detail + QLatin1Char(')');
            }
            throw std::runtime_error(fullMessage.toStdString());
        }

        void ensureStreamOk(const QDataStream& stream, const QString& message)
        {
            if (stream.status() != QDataStream::Ok)
            {
                throwNbtError(message);
            }
        }

        QString readUtf(QDataStream& input)
        {
            quint16 length = 0;
            input >> length;
            ensureStreamOk(input, QStringLiteral("NBT contains a truncated string length"));
            QByteArray bytes(length, Qt::Uninitialized);
            if (length > 0 && input.readRawData(bytes.data(), length) != length)
            {
                throwNbtError(QStringLiteral("NBT contains a truncated string"));
            }
            QString value;
            value.reserve(length);
            qsizetype index = 0;
            while (index < bytes.size())
            {
                const quint8 first = static_cast<quint8>(bytes.at(index++));
                if ((first & 0x80U) == 0)
                {
                    value.append(QChar(first));
                    continue;
                }
                if ((first & 0xe0U) == 0xc0U)
                {
                    if (index >= bytes.size())
                    {
                        throwNbtError(QStringLiteral("NBT contains invalid modified UTF-8"));
                    }
                    const quint8 second = static_cast<quint8>(bytes.at(index++));
                    if ((second & 0xc0U) != 0x80U)
                    {
                        throwNbtError(QStringLiteral("NBT contains invalid modified UTF-8"));
                    }
                    value.append(
                        QChar(static_cast<ushort>(((first & 0x1fU) << 6U) | (second & 0x3fU))));
                    continue;
                }
                if ((first & 0xf0U) == 0xe0U)
                {
                    if (index + 1 >= bytes.size())
                    {
                        throwNbtError(QStringLiteral("NBT contains invalid modified UTF-8"));
                    }
                    const quint8 second = static_cast<quint8>(bytes.at(index++));
                    const quint8 third = static_cast<quint8>(bytes.at(index++));
                    if ((second & 0xc0U) != 0x80U || (third & 0xc0U) != 0x80U)
                    {
                        throwNbtError(QStringLiteral("NBT contains invalid modified UTF-8"));
                    }
                    value.append(QChar(static_cast<ushort>(
                        ((first & 0x0fU) << 12U) | ((second & 0x3fU) << 6U) | (third & 0x3fU))));
                    continue;
                }
                throwNbtError(QStringLiteral("NBT contains invalid modified UTF-8"));
            }
            return value;
        }

        void writeUtf(QDataStream& output, const QString& value)
        {
            QByteArray bytes;
            bytes.reserve(value.size() * 3);
            for (const QChar character : value)
            {
                const quint16 codeUnit = character.unicode();
                if (codeUnit >= 0x0001U && codeUnit <= 0x007fU)
                {
                    bytes.append(static_cast<char>(codeUnit));
                }
                else if (codeUnit <= 0x07ffU)
                {
                    bytes.append(static_cast<char>(0xc0U | (codeUnit >> 6U)));
                    bytes.append(static_cast<char>(0x80U | (codeUnit & 0x3fU)));
                }
                else
                {
                    bytes.append(static_cast<char>(0xe0U | (codeUnit >> 12U)));
                    bytes.append(static_cast<char>(0x80U | ((codeUnit >> 6U) & 0x3fU)));
                    bytes.append(static_cast<char>(0x80U | (codeUnit & 0x3fU)));
                }
            }
            if (bytes.size() > std::numeric_limits<quint16>::max())
            {
                throwNbtError(QStringLiteral("NBT string exceeds the supported size limit"));
            }
            output << static_cast<quint16>(bytes.size());
            if (!bytes.isEmpty())
            {
                output.writeRawData(bytes.constData(), bytes.size());
            }
            ensureStreamOk(output, QStringLiteral("Unable to write NBT string"));
        }

        qint32 readCollectionSize(QDataStream& input)
        {
            qint32 size = 0;
            input >> size;
            ensureStreamOk(input, QStringLiteral("NBT contains a truncated collection size"));
            if (size < 0 || size > maxNbtCollectionSize)
            {
                throwNbtError(QStringLiteral("NBT contains an invalid or oversized collection"));
            }
            return size;
        }

        NbtTag readPayload(QDataStream& input, quint8 type, int depth);

        std::shared_ptr<NbtList> readList(QDataStream& input, int depth)
        {
            auto list = std::make_shared<NbtList>();
            input >> list->elementType;
            const qint32 size = readCollectionSize(input);
            if (list->elementType == 0 && size != 0)
            {
                throwNbtError(QStringLiteral("Non-empty NBT lists cannot contain end tags"));
            }
            list->elements.reserve(size);
            for (qint32 index = 0; index < size; ++index)
            {
                list->elements.append(readPayload(input, list->elementType, depth + 1));
            }
            return list;
        }

        std::shared_ptr<NbtCompound> readCompound(QDataStream& input, int depth)
        {
            auto compound = std::make_shared<NbtCompound>();
            while (true)
            {
                quint8 type = 0;
                input >> type;
                ensureStreamOk(input, QStringLiteral("NBT contains a truncated compound"));
                if (type == 0)
                {
                    return compound;
                }
                if (type > 12 || compound->entries.size() >= maxNbtCollectionSize)
                {
                    throwNbtError(QStringLiteral("NBT contains an invalid or oversized compound"));
                }
                const QString name = readUtf(input);
                compound->insert(name, readPayload(input, type, depth + 1));
            }
        }

        NbtTag readPayload(QDataStream& input, quint8 type, int depth)
        {
            if (depth > maxNbtDepth)
            {
                throwNbtError(QStringLiteral("NBT nesting is too deep"));
            }
            NbtTag tag;
            tag.type = type;
            switch (type)
            {
            case 1:
            {
                qint8 value = 0;
                input >> value;
                tag.value = value;
                break;
            }
            case 2:
            {
                qint16 value = 0;
                input >> value;
                tag.value = value;
                break;
            }
            case 3:
            {
                qint32 value = 0;
                input >> value;
                tag.value = value;
                break;
            }
            case 4:
            {
                qint64 value = 0;
                input >> value;
                tag.value = value;
                break;
            }
            case 5:
            {
                quint32 bits = 0;
                input >> bits;
                tag.value = std::bit_cast<float>(bits);
                break;
            }
            case 6:
            {
                quint64 bits = 0;
                input >> bits;
                tag.value = std::bit_cast<double>(bits);
                break;
            }
            case 7:
            {
                const qint32 size = readCollectionSize(input);
                QByteArray value(size, Qt::Uninitialized);
                if (size > 0 && input.readRawData(value.data(), size) != size)
                {
                    throwNbtError(QStringLiteral("NBT contains a truncated byte array"));
                }
                tag.value = value;
                break;
            }
            case 8:
                tag.value = readUtf(input);
                break;
            case 9:
                tag.value = readList(input, depth);
                break;
            case 10:
                tag.value = readCompound(input, depth);
                break;
            case 11:
            {
                const qint32 size = readCollectionSize(input);
                QVector<qint32> values;
                values.reserve(size);
                for (qint32 index = 0; index < size; ++index)
                {
                    qint32 value = 0;
                    input >> value;
                    values.append(value);
                }
                tag.value = values;
                break;
            }
            case 12:
            {
                const qint32 size = readCollectionSize(input);
                QVector<qint64> values;
                values.reserve(size);
                for (qint32 index = 0; index < size; ++index)
                {
                    qint64 value = 0;
                    input >> value;
                    values.append(value);
                }
                tag.value = values;
                break;
            }
            default:
                throwNbtError(QStringLiteral("Unsupported NBT tag type: %1").arg(type));
            }
            ensureStreamOk(input, QStringLiteral("NBT contains truncated tag data"));
            return tag;
        }

        void writePayload(QDataStream& output, const NbtTag& tag, int depth);

        void writeList(QDataStream& output, const NbtList& list, int depth)
        {
            if (list.elements.size() > maxNbtCollectionSize)
            {
                throwNbtError(QStringLiteral("NBT list exceeds the supported size limit"));
            }
            output << list.elementType << static_cast<qint32>(list.elements.size());
            for (const NbtTag& element : list.elements)
            {
                if (element.type != list.elementType)
                {
                    throwNbtError(QStringLiteral("NBT list elements have inconsistent types"));
                }
                writePayload(output, element, depth + 1);
            }
        }

        void writeCompound(QDataStream& output, const NbtCompound& compound, int depth)
        {
            if (compound.entries.size() > maxNbtCollectionSize)
            {
                throwNbtError(QStringLiteral("NBT compound exceeds the supported size limit"));
            }
            for (const auto& entry : compound.entries)
            {
                output << entry.second.type;
                writeUtf(output, entry.first);
                writePayload(output, entry.second, depth + 1);
            }
            output << quint8(0);
        }

        void writePayload(QDataStream& output, const NbtTag& tag, int depth)
        {
            if (depth > maxNbtDepth)
            {
                throwNbtError(QStringLiteral("NBT nesting is too deep"));
            }
            switch (tag.type)
            {
            case 1:
                output << std::get<qint8>(tag.value);
                break;
            case 2:
                output << std::get<qint16>(tag.value);
                break;
            case 3:
                output << std::get<qint32>(tag.value);
                break;
            case 4:
                output << std::get<qint64>(tag.value);
                break;
            case 5:
                output << std::bit_cast<quint32>(std::get<float>(tag.value));
                break;
            case 6:
                output << std::bit_cast<quint64>(std::get<double>(tag.value));
                break;
            case 7:
            {
                const QByteArray& value = std::get<QByteArray>(tag.value);
                if (value.size() > maxNbtCollectionSize)
                {
                    throwNbtError(
                        QStringLiteral("NBT byte array exceeds the supported size limit"));
                }
                output << static_cast<qint32>(value.size());
                if (!value.isEmpty())
                {
                    output.writeRawData(value.constData(), value.size());
                }
                break;
            }
            case 8:
                writeUtf(output, std::get<QString>(tag.value));
                break;
            case 9:
            {
                const auto& list = std::get<std::shared_ptr<NbtList>>(tag.value);
                if (!list)
                {
                    throwNbtError(QStringLiteral("NBT list is null"));
                }
                writeList(output, *list, depth);
                break;
            }
            case 10:
            {
                const auto& compound = std::get<std::shared_ptr<NbtCompound>>(tag.value);
                if (!compound)
                {
                    throwNbtError(QStringLiteral("NBT compound is null"));
                }
                writeCompound(output, *compound, depth);
                break;
            }
            case 11:
            {
                const QVector<qint32>& values = std::get<QVector<qint32>>(tag.value);
                if (values.size() > maxNbtCollectionSize)
                {
                    throwNbtError(QStringLiteral("NBT int array exceeds the supported size limit"));
                }
                output << static_cast<qint32>(values.size());
                for (qint32 value : values)
                {
                    output << value;
                }
                break;
            }
            case 12:
            {
                const QVector<qint64>& values = std::get<QVector<qint64>>(tag.value);
                if (values.size() > maxNbtCollectionSize)
                {
                    throwNbtError(
                        QStringLiteral("NBT long array exceeds the supported size limit"));
                }
                output << static_cast<qint32>(values.size());
                for (qint64 value : values)
                {
                    output << value;
                }
                break;
            }
            default:
                throwNbtError(QStringLiteral("Unsupported NBT tag type: %1").arg(tag.type));
            }
            ensureStreamOk(output, QStringLiteral("Unable to write NBT tag data"));
        }

        QByteArray decompress(const QByteArray& input)
        {
            z_stream stream{};
            if (inflateInit2(&stream, MAX_WBITS + 16) != Z_OK)
            {
                throwNbtError(QStringLiteral("Unable to initialize NBT gzip decompression"));
            }
            stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.constData()));
            stream.avail_in = static_cast<uInt>(input.size());
            QByteArray output;
            std::array<char, 64 * 1024> buffer{};
            int result = Z_OK;
            while (result == Z_OK)
            {
                stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
                stream.avail_out = static_cast<uInt>(buffer.size());
                result = inflate(&stream, Z_NO_FLUSH);
                const qsizetype produced = static_cast<qsizetype>(buffer.size() - stream.avail_out);
                if (output.size() + produced > maxNbtBytes)
                {
                    inflateEnd(&stream);
                    throwNbtError(
                        QStringLiteral("Decompressed NBT exceeds the supported size limit"));
                }
                output.append(buffer.data(), produced);
            }
            const QString error =
                stream.msg == nullptr ? QString() : QString::fromLatin1(stream.msg);
            inflateEnd(&stream);
            if (result != Z_STREAM_END)
            {
                throwNbtError(QStringLiteral("Unable to decompress NBT"), {}, error);
            }
            return output;
        }

        QByteArray compress(const QByteArray& input)
        {
            z_stream stream{};
            if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8,
                             Z_DEFAULT_STRATEGY) != Z_OK)
            {
                throwNbtError(QStringLiteral("Unable to initialize NBT gzip compression"));
            }
            stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.constData()));
            stream.avail_in = static_cast<uInt>(input.size());
            QByteArray output;
            std::array<char, 64 * 1024> buffer{};
            int result = Z_OK;
            while (result != Z_STREAM_END)
            {
                stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
                stream.avail_out = static_cast<uInt>(buffer.size());
                result = deflate(&stream, Z_FINISH);
                if (result != Z_OK && result != Z_STREAM_END)
                {
                    deflateEnd(&stream);
                    throwNbtError(QStringLiteral("Unable to compress NBT"));
                }
                output.append(buffer.data(),
                              static_cast<qsizetype>(buffer.size() - stream.avail_out));
            }
            deflateEnd(&stream);
            return output;
        }

        QByteArray serialize(const NbtDocument& document)
        {
            QByteArray bytes;
            QBuffer buffer(&bytes);
            if (!buffer.open(QIODevice::WriteOnly))
            {
                throwNbtError(QStringLiteral("Unable to create NBT buffer"));
            }
            QDataStream output(&buffer);
            output.setByteOrder(QDataStream::BigEndian);
            output << quint8(10);
            writeUtf(output, document.rootName);
            writeCompound(output, document.root, 0);
            if (bytes.size() > maxNbtBytes)
            {
                throwNbtError(QStringLiteral("NBT exceeds the supported size limit"));
            }
            return bytes;
        }
    } // namespace

    const NbtTag* NbtCompound::find(const QString& name) const
    {
        for (const auto& entry : entries)
        {
            if (entry.first == name)
            {
                return &entry.second;
            }
        }
        return nullptr;
    }

    NbtTag* NbtCompound::find(const QString& name)
    {
        for (auto& entry : entries)
        {
            if (entry.first == name)
            {
                return &entry.second;
            }
        }
        return nullptr;
    }

    void NbtCompound::insert(const QString& name, const NbtTag& tag)
    {
        if (NbtTag* existing = find(name))
        {
            *existing = tag;
            return;
        }
        entries.append(qMakePair(name, tag));
    }

    NbtDocument NbtFile::read(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            throwNbtError(QStringLiteral("Unable to read NBT file"), path, file.errorString());
        }
        if (file.size() > maxNbtBytes)
        {
            throwNbtError(QStringLiteral("NBT file exceeds the supported size limit"), path);
        }
        QByteArray bytes = file.readAll();
        if (bytes.size() >= 2 && static_cast<quint8>(bytes.at(0)) == 0x1f &&
            static_cast<quint8>(bytes.at(1)) == 0x8b)
        {
            bytes = decompress(bytes);
        }
        QBuffer buffer(&bytes);
        if (!buffer.open(QIODevice::ReadOnly))
        {
            throwNbtError(QStringLiteral("Unable to open NBT data"), path);
        }
        QDataStream input(&buffer);
        input.setByteOrder(QDataStream::BigEndian);
        quint8 rootType = 0;
        input >> rootType;
        ensureStreamOk(input, QStringLiteral("NBT root is truncated"));
        if (rootType != 10)
        {
            throwNbtError(QStringLiteral("NBT root must be a compound"), path);
        }
        NbtDocument document;
        document.rootName = readUtf(input);
        const NbtTag root = readPayload(input, rootType, 0);
        document.root = *std::get<std::shared_ptr<NbtCompound>>(root.value);
        if (!buffer.atEnd())
        {
            throwNbtError(QStringLiteral("NBT file contains trailing data"), path);
        }
        return document;
    }

    void NbtFile::write(const QString& path, const NbtDocument& document, bool compressed)
    {
        QByteArray bytes = serialize(document);
        if (compressed)
        {
            bytes = compress(bytes);
        }
        if (bytes.size() > maxNbtBytes)
        {
            throwNbtError(QStringLiteral("NBT exceeds the supported size limit"), path);
        }
        const QString parentDirectory = QFileInfo(path).absolutePath();
        if (!QDir().mkpath(parentDirectory))
        {
            throwNbtError(QStringLiteral("Unable to create NBT directory"), parentDirectory);
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly))
        {
            throwNbtError(QStringLiteral("Unable to write NBT file"), path, file.errorString());
        }
        if (file.write(bytes) != bytes.size() || !file.commit())
        {
            throwNbtError(QStringLiteral("Unable to commit NBT file"), path, file.errorString());
        }
    }

    bool NbtFile::isCompressed(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            throwNbtError(QStringLiteral("Unable to read NBT file"), path, file.errorString());
        }
        const QByteArray signature = file.read(2);
        return signature.size() == 2 && static_cast<quint8>(signature.at(0)) == 0x1f &&
               static_cast<quint8>(signature.at(1)) == 0x8b;
    }
} // namespace strata::settings
