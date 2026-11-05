#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>

#include "LauncherController.h"
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

    const QString settingsPath = tempDir.path() + QStringLiteral("/settings.ini");
    ThemeSettings settings(settingsPath);
    settings.setTheme(QStringLiteral("light"));
    settings.setReduceMotion(true);
    settings.save();

    ThemeSettings reloaded(settingsPath);
    QCOMPARE(reloaded.theme(), QStringLiteral("light"));
    QVERIFY(reloaded.reduceMotion());
}

class MotionCurveTest : public QObject
{
    Q_OBJECT

private slots:
    void curveConstants();
};

void MotionCurveTest::curveConstants()
{
    QFile motionFile(QStringLiteral(":/qml/Motion.qml"));
    QVERIFY2(motionFile.open(QIODevice::ReadOnly | QIODevice::Text),
             "Motion.qml should be registered in the Qt resource system.");

    const QByteArray content = motionFile.readAll();
    QVERIFY2(content.contains("standardCurve"), "Motion.qml should define the standard curve.");
    QVERIFY2(content.contains("emphasisCurve"), "Motion.qml should define the emphasis curve.");
    QVERIFY2(content.contains("expressCurve"), "Motion.qml should define the express curve.");
    QVERIFY2(content.contains("reduceMotion"), "Motion.qml should expose reduceMotion.");
    QVERIFY2(content.contains("0.25"), "Motion.qml should keep the standard curve constants.");
    QVERIFY2(content.contains("0.22"), "Motion.qml should keep the emphasis curve constants.");
    QVERIFY2(content.contains("0.2"), "Motion.qml should keep the express curve constants.");
}

class QmlSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void smokeLoadMainWindow();
};

void QmlSmokeTest::smokeLoadMainWindow()
{
    QQmlApplicationEngine engine;
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    ThemeSettings themeSettings(temporaryDirectory.filePath(QStringLiteral("test-settings.ini")));
    strata::ui::LauncherController launcherController;
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeSettings"), &themeSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("LauncherController"),
                                             &launcherController);
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    engine.load(QUrl(QStringLiteral("qrc:/qml/MainWindow.qml")));

    QVERIFY2(!engine.rootObjects().isEmpty(), "The main QML window should load without errors.");
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    QVERIFY2(window != nullptr, "The root object should be a QQuickWindow.");
}

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    ThemeSettingsTest themeSettingsTest;
    MotionCurveTest motionCurveTest;
    QmlSmokeTest qmlSmokeTest;
    return QTest::qExec(&themeSettingsTest, argc, argv) ||
           QTest::qExec(&motionCurveTest, argc, argv) || QTest::qExec(&qmlSmokeTest, argc, argv);
}

#include "LauncherQtTests.moc"
