#pragma once

#include <QObject>

class M9ModrinthTests : public QObject
{
    Q_OBJECT

private slots:
    void contentTypesMapToCompatibleDirectories();
    void modSearchAppliesMinecraftLoaderAndPaginationFilters();
    void parsesVersionsAndRejectsUnsafeDownloads();
    void resolvesNestedRequiredDependenciesInInstallOrder();
    void readsLegacyStateAndTracksEnableAndRemoval();
    void requiredDependenciesBlockInstallation();
};
