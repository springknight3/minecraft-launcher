#pragma once

#include <QByteArray>
#include <QObject>

#include <future>

class M4InstallTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesManifestAndLatestRelease();
    void atomicallyCachesManifestAndUsesItWhenOffline();
    void downloadsConcurrentlyRetriesAndVerifiesChecksums();
    void rejectsChecksumMismatchWithoutPartialFile();
    void installsMinecraftMetadataAndClientJar();

private:
    static QByteArray sha1(const QByteArray& value);
    static void waitFor(std::future<void>& future);
};
