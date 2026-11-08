#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "ThemeSettings.h"

class ThemeSettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip();
};

void ThemeSettingsTest::roundTrip()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary settings directory should be valid.");

    ThemeSettings settings(tempDir.path());
    settings.setTheme(QStringLiteral("light"));
    settings.setReduceMotion(true);
    settings.save();
    const QString settingsFile = tempDir.filePath(QStringLiteral("settings.json"));
    const auto appearanceSaved = [&settingsFile]()
    {
        QFile settingsReader(settingsFile);
        if (!settingsReader.open(QIODevice::ReadOnly))
        {
            return false;
        }
        const QJsonObject savedValues = QJsonDocument::fromJson(settingsReader.readAll()).object();
        return !savedValues.value(QStringLiteral("darkMode")).toBool() &&
               savedValues.value(QStringLiteral("reduceMotion")).toBool();
    };
    QTRY_VERIFY_WITH_TIMEOUT(appearanceSaved(), 2000);

    ThemeSettings reloaded(tempDir.path());
    QCOMPARE(reloaded.theme(), QStringLiteral("light"));
    QVERIFY(reloaded.reduceMotion());
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    ThemeSettingsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThemeSettingsTest.moc"
