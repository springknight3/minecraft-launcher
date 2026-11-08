#pragma once

#include <QString>

namespace strata::modrinth
{
    enum class ContentType
    {
        Mod,
        ResourcePack,
        DataPack,
        Shader
    };

    QString projectType(ContentType type);
    QString displayName(ContentType type);
    QString directoryName(ContentType type);
    ContentType parseContentType(const QString& value);

    enum class ServerContentType
    {
        Mod,
        Plugin
    };

    QString projectType(ServerContentType type);
} // namespace strata::modrinth
