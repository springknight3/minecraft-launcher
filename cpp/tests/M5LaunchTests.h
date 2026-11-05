#pragma once

#include <QObject>

class M5LaunchTests : public QObject
{
    Q_OBJECT

private slots:
    void offlineAccountPersistsJavaCompatibleUuid();
    void rulesAndArgumentsRespectConditionsAndBoundaries();
    void launchPlanUsesOfflineSubstitutionsAndArgumentLists();
    void nativeExtractionRejectsTraversal();
    void processTracksReadyExitCrashAndStop();
};
