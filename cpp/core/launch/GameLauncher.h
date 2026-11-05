#pragma once

#include "../auth/OfflineAccountStore.h"
#include "../instance/Instance.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QStringList>

namespace strata::launch
{
    struct LaunchPlan
    {
        QString program;
        QStringList arguments;
        QString workingDirectory;
        QString logFile;
    };

    class GameLauncher
    {
    public:
        static LaunchPlan buildPlan(const strata::Instance& instance,
                                    const auth::OfflineAccount& account,
                                    const QString& dataDirectory,
                                    const QString& javaExecutable = QString(),
                                    const QStringList& globalJavaArguments = {});
        static bool isAllowedByRules(const QJsonObject& item, const QString& osName,
                                     const QString& architecture, const QJsonObject& features = {});
        static QStringList resolveArguments(const QJsonArray& arguments,
                                            const QMap<QString, QString>& substitutions,
                                            const QString& osName, const QString& architecture);
        static QStringList composeJvmArguments(const QStringList& metadataArguments,
                                               const QStringList& customArguments);
        static bool isGameReadyMarker(const QByteArray& line);
        static void extractNativeArchives(const QStringList& archives, const QString& destination);
    };
} // namespace strata::launch
