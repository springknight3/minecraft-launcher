#pragma once

#include "../download/DownloadManager.h"
#include "../instance/Instance.h"
#include "MojangManifestClient.h"

namespace strata::mojang
{
    class MinecraftInstaller
    {
    public:
        explicit MinecraftInstaller(const download::DownloadManager& downloadManager,
                                    QString dataDirectory = QString());

        void install(const strata::Instance& instance, const Version& version,
                     download::DownloadManager::ProgressCallback progress = {}) const;

    private:
        const download::DownloadManager& m_downloadManager;
        QString m_dataDirectory;
    };
} // namespace strata::mojang
