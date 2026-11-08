#include "OptionsFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

#include <stdexcept>

namespace strata::settings
{
    namespace
    {
        constexpr qint64 maxOptionsBytes = 16 * 1024 * 1024;

        [[noreturn]] void throwFileError(const QString& action, const QString& path,
                                         const QString& detail = {})
        {
            const QString message = detail.isEmpty()
                                        ? QStringLiteral("%1: %2").arg(action, path)
                                        : QStringLiteral("%1: %2 (%3)").arg(action, path, detail);
            throw std::runtime_error(message.toStdString());
        }

        qsizetype nextLineBreak(const QString& content, qsizetype from)
        {
            const qsizetype lineFeed = content.indexOf(QLatin1Char('\n'), from);
            const qsizetype carriageReturn = content.indexOf(QLatin1Char('\r'), from);
            if (lineFeed < 0)
            {
                return carriageReturn;
            }
            return carriageReturn < 0 ? lineFeed : qMin(lineFeed, carriageReturn);
        }

        QString firstLineEnding(const QString& content)
        {
            const qsizetype index = nextLineBreak(content, 0);
            if (index < 0)
            {
                return QStringLiteral("\n");
            }
            if (content.at(index) == QLatin1Char('\r') && index + 1 < content.size() &&
                content.at(index + 1) == QLatin1Char('\n'))
            {
                return QStringLiteral("\r\n");
            }
            return content.mid(index, 1);
        }

        bool endsWithLineBreak(const QString& content)
        {
            return content.endsWith(QLatin1Char('\n')) || content.endsWith(QLatin1Char('\r'));
        }
    } // namespace

    QMap<QString, QString> OptionsFile::read(const QString& path)
    {
        QFile file(path);
        if (!file.exists())
        {
            return {};
        }
        if (!file.open(QIODevice::ReadOnly))
        {
            throwFileError(QStringLiteral("Unable to read Minecraft options"), path,
                           file.errorString());
        }
        if (file.size() > maxOptionsBytes)
        {
            throwFileError(QStringLiteral("Minecraft options exceed the supported size limit"),
                           path);
        }
        return parse(QString::fromUtf8(file.readAll()));
    }

    QMap<QString, QString> OptionsFile::parse(const QString& content)
    {
        QMap<QString, QString> values;
        const QStringList lines =
            content.split(QRegularExpression(QStringLiteral("\\r\\n|\\n|\\r")));
        for (const QString& line : lines)
        {
            const qsizetype separator = line.indexOf(QLatin1Char(':'));
            if (separator > 0)
            {
                values.insert(line.left(separator), line.mid(separator + 1));
            }
        }
        return values;
    }

    void OptionsFile::set(const QString& path, const QString& key, const QString& value)
    {
        setAll(path, {{key, value}});
    }

    void OptionsFile::setAll(const QString& path, const QMap<QString, QString>& changes)
    {
        QFile file(path);
        QString content;
        if (file.exists())
        {
            if (!file.open(QIODevice::ReadOnly))
            {
                throwFileError(QStringLiteral("Unable to read Minecraft options"), path,
                               file.errorString());
            }
            if (file.size() > maxOptionsBytes)
            {
                throwFileError(QStringLiteral("Minecraft options exceed the supported size limit"),
                               path);
            }
            content = QString::fromUtf8(file.readAll());
        }

        for (auto change = changes.cbegin(); change != changes.cend(); ++change)
        {
            content = update(content, change.key(), change.value());
        }

        const QString parentDirectory = QFileInfo(path).absolutePath();
        if (!QDir().mkpath(parentDirectory))
        {
            throwFileError(QStringLiteral("Unable to create Minecraft options directory"),
                           parentDirectory);
        }
        QSaveFile output(path);
        if (!output.open(QIODevice::WriteOnly))
        {
            throwFileError(QStringLiteral("Unable to write Minecraft options"), path,
                           output.errorString());
        }
        const QByteArray bytes = content.toUtf8();
        if (output.write(bytes) != bytes.size() || !output.commit())
        {
            throwFileError(QStringLiteral("Unable to commit Minecraft options"), path,
                           output.errorString());
        }
    }

    QString OptionsFile::update(const QString& content, const QString& key, const QString& value)
    {
        validateKey(key);
        if (value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r')))
        {
            throw std::invalid_argument("Option values must be a single line.");
        }

        qsizetype matchStart = -1;
        qsizetype matchEnd = -1;
        qsizetype matchSeparator = -1;
        qsizetype lineStart = 0;
        while (lineStart < content.size())
        {
            const qsizetype lineBreak = nextLineBreak(content, lineStart);
            qsizetype lineEndingLength = 0;
            if (lineBreak >= 0)
            {
                lineEndingLength = content.at(lineBreak) == QLatin1Char('\r') &&
                                           lineBreak + 1 < content.size() &&
                                           content.at(lineBreak + 1) == QLatin1Char('\n')
                                       ? 2
                                       : 1;
            }
            const qsizetype lineEnd = lineBreak < 0 ? content.size() : lineBreak + lineEndingLength;
            const qsizetype textEnd = lineBreak < 0 ? lineEnd : lineBreak;
            const qsizetype separator = content.indexOf(QLatin1Char(':'), lineStart);
            if (separator >= lineStart && separator < textEnd &&
                content.mid(lineStart, separator - lineStart) == key)
            {
                matchStart = lineStart;
                matchEnd = lineEnd;
                matchSeparator = separator;
            }
            lineStart = lineEnd;
        }

        if (matchStart >= 0)
        {
            const qsizetype lineBreak = nextLineBreak(content, matchStart);
            const QString lineEnding =
                lineBreak < 0 ? QString() : content.mid(lineBreak, matchEnd - lineBreak);
            return content.left(matchSeparator + 1) + value + lineEnding + content.mid(matchEnd);
        }

        const QString newline = firstLineEnding(content);
        const QString prefix =
            content.isEmpty() || endsWithLineBreak(content) ? content : content + newline;
        return prefix + key + QLatin1Char(':') + value + newline;
    }

    void OptionsFile::validateKey(const QString& key)
    {
        if (key.trimmed().isEmpty() || key.contains(QLatin1Char(':')) ||
            key.contains(QLatin1Char('\n')) || key.contains(QLatin1Char('\r')))
        {
            throw std::invalid_argument(
                "Option keys must be non-empty and cannot contain colons or line breaks.");
        }
    }
} // namespace strata::settings
