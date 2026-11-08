#pragma once

#include <QObject>

class M7AuthTests : public QObject
{
    Q_OBJECT

private slots:
    void persistsOfflineAccountsAndActiveSelection();
    void storesMicrosoftCredentialsOutsideAccountMetadata();
    void rejectsInvalidAccountsAndClientIds();
    void parsesMicrosoftDeviceCodeAndProfileResponses();
};
