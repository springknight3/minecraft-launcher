#pragma once

#include "DownloadTask.h"

#include <QList>
#include <QString>

#include <atomic>
#include <functional>

namespace strata::download
{
    struct Progress
    {
        int completed = 0;
        int total = 0;
        QString currentItem;

        double fraction() const;
    };

    class DownloadManager
    {
    public:
        using ProgressCallback = std::function<void(const Progress&)>;

        explicit DownloadManager(int concurrency = 6, int retries = 3);

        void downloadAll(const QList<DownloadTask>& tasks, ProgressCallback progress = {}) const;

    private:
        int m_concurrency;
        int m_retries;

        void downloadOne(const DownloadTask& task, const std::atomic_bool& cancelled) const;
    };
} // namespace strata::download
