#pragma once

#include <QObject>

class M6RuntimeTests : public QObject
{
    Q_OBJECT

private slots:
    void mapsMinecraftVersionsAndJavaMajors();
    void selectsPlatformAndNewestMojangRuntime();
    void parsesRuntimeManifestAndRejectsUnsafeEntries();
    void resolvesRuntimePathsAndPersistsPreferences();
    void resolvesManagedJavaAndVerifiesItsIntegrity();
    void extractsSafeArchivesAndRejectsTraversal();
    void inventoriesOwnersAndBlocksInUseDeletion();
};
