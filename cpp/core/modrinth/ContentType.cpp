#include "ContentType.h"

#include <stdexcept>

namespace strata::modrinth
{
    namespace
    {
        [[noreturn]] void fail(const QString& message)
        {
            throw std::invalid_argument(message.toStdString());
        }
    } // namespace

    QString projectType(ContentType type)
    {
        switch (type)
        {
        case ContentType::Mod:
            return QStringLiteral("mod");
        case ContentType::ResourcePack:
            return QStringLiteral("resourcepack");
        case ContentType::DataPack:
            return QStringLiteral("datapack");
        case ContentType::Shader:
            return QStringLiteral("shader");
        }
        fail(QStringLiteral("Unknown content type."));
    }

    QString displayName(ContentType type)
    {
        switch (type)
        {
        case ContentType::Mod:
            return QStringLiteral("Mods");
        case ContentType::ResourcePack:
            return QStringLiteral("Resource packs");
        case ContentType::DataPack:
            return QStringLiteral("Data packs");
        case ContentType::Shader:
            return QStringLiteral("Shader packs");
        }
        fail(QStringLiteral("Unknown content type."));
    }

    QString directoryName(ContentType type)
    {
        switch (type)
        {
        case ContentType::Mod:
            return QStringLiteral("mods");
        case ContentType::ResourcePack:
            return QStringLiteral("resourcepacks");
        case ContentType::DataPack:
            return QStringLiteral("datapacks");
        case ContentType::Shader:
            return QStringLiteral("shaderpacks");
        }
        fail(QStringLiteral("Unknown content type."));
    }

    ContentType parseContentType(const QString& value)
    {
        const QString normalized = value.trimmed().toUpper();
        if (normalized == QStringLiteral("MOD"))
        {
            return ContentType::Mod;
        }
        if (normalized == QStringLiteral("RESOURCE_PACK") ||
            normalized == QStringLiteral("RESOURCEPACK"))
        {
            return ContentType::ResourcePack;
        }
        if (normalized == QStringLiteral("DATA_PACK") || normalized == QStringLiteral("DATAPACK"))
        {
            return ContentType::DataPack;
        }
        if (normalized == QStringLiteral("SHADER"))
        {
            return ContentType::Shader;
        }
        fail(QStringLiteral("Unknown content type: %1").arg(value));
    }

    QString projectType(ServerContentType type)
    {
        switch (type)
        {
        case ServerContentType::Mod:
            return QStringLiteral("mod");
        case ServerContentType::Plugin:
            return QStringLiteral("plugin");
        }
        fail(QStringLiteral("Unknown server content type."));
    }
} // namespace strata::modrinth
