#pragma once

#include <QObject>

class M8LoaderTests : public QObject
{
    Q_OBJECT

private slots:
    void loaderTypesAreValidated();
    void neoForgeVersionsMapToCompatibleMavenPrefixes();
    void mavenMetadataFiltersAndSortsCompatibleVersions();
    void mavenMetadataRejectsDtds();
    void loaderProfilesValidateMinecraftInheritance();
    void loaderMetadataMergesLibrariesAndArguments();
    void libraryPathsCannotEscapeTheirRoot();
    void pendingLoaderStateCannotBeLaunched();
    void launchPlanRetainsLoaderJvmAndGameArguments();
};
