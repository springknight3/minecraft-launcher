#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <stdexcept>

#include "LauncherController.h"
#include "ThemeSettings.h"
#include "settings/LauncherSettings.h"

class ThemeSettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip();
    void launcherSettingsRoundTrip();
    void invalidGlobalJavaFlagsAreRejected();
    void migratesLegacyIniPreferences();
    void bundledAssetsAreAvailable();
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

void ThemeSettingsTest::launcherSettingsRoundTrip()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    strata::settings::LauncherSettings settings(tempDir.path());
    QCOMPARE(settings.globalJavaLaunchFlags(),
             strata::settings::LauncherSettings::recommendedJavaFlags());
    settings.setGlobalJavaLaunchFlags({QStringLiteral("-Xmx4G")});
    settings.setLastLaunchedInstanceId(QStringLiteral("instance-id"));
    settings.recordInstanceLaunch(QStringLiteral("second-instance"));
    settings.setTrackInstanceLaunches(false);
    settings.setRuntimePreferences(QStringLiteral("TEMURIN"), true);
    settings.setDiagnosticsPreferences(false, true);

    strata::settings::LauncherSettings reloaded(tempDir.path());
    QCOMPARE(reloaded.globalJavaLaunchFlags(), QStringList{QStringLiteral("-Xmx4G")});
    QCOMPARE(reloaded.lastLaunchedInstanceId(), QStringLiteral("second-instance"));
    QCOMPARE(reloaded.recentLaunchedInstanceIds(),
             (QStringList{QStringLiteral("second-instance"), QStringLiteral("instance-id")}));
    QVERIFY(!reloaded.trackInstanceLaunches());
    QCOMPARE(reloaded.runtimeVendor(), QStringLiteral("TEMURIN"));
    QVERIFY(reloaded.redundantJavaRuntimes());
    QVERIFY(!reloaded.vsyncEnabled());
    QVERIFY(reloaded.frameTimeOverlayEnabled());
}

void ThemeSettingsTest::invalidGlobalJavaFlagsAreRejected()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    strata::settings::LauncherSettings settings(tempDir.path());
    QVERIFY_EXCEPTION_THROWN(settings.setGlobalJavaLaunchFlags({QStringLiteral("not-a-flag")}),
                             std::invalid_argument);
}

void ThemeSettingsTest::migratesLegacyIniPreferences()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QSettings legacy(tempDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    legacy.setValue(QStringLiteral("theme"), QStringLiteral("light"));
    legacy.setValue(QStringLiteral("reduceMotion"), true);
    legacy.sync();

    strata::settings::LauncherSettings settings(tempDir.path());
    QVERIFY(!settings.darkMode());
    QVERIFY(settings.reduceMotion());
    QVERIFY(QFile::exists(tempDir.filePath(QStringLiteral("settings.json"))));
}

void ThemeSettingsTest::bundledAssetsAreAvailable()
{
    QVERIFY(QFile::exists(QStringLiteral(":/assets/fonts/DMSans-VariableFont_opsz,wght.ttf")));
    QVERIFY(QFile::exists(QStringLiteral(":/assets/fonts/Outfit-VariableFont_wght.ttf")));
    QVERIFY(QFile::exists(QStringLiteral(":/assets/fonts/JetBrainsMono-VariableFont_wght.ttf")));
    QVERIFY(QFile::exists(QStringLiteral(":/assets/icons/home-dark.svg")));
    QVERIFY(QFile::exists(QStringLiteral(":/assets/icons/home-light.svg")));
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
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    ThemeSettings themeSettings(temporaryDirectory.path());
    strata::ui::LauncherController launcherController;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeSettings"), &themeSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("LauncherController"),
                                             &launcherController);
    engine.rootContext()->setContextProperty(QStringLiteral("AppDebugFrameTime"), false);
    engine.rootContext()->setContextProperty(QStringLiteral("AppDebugFrameTimeForced"), false);
    engine.rootContext()->setContextProperty(QStringLiteral("AppVersion"), QStringLiteral("0.1.0"));
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    engine.load(QUrl(QStringLiteral("qrc:/qml/MainWindow.qml")));

    QVERIFY2(!engine.rootObjects().isEmpty(), "The main QML window should load without errors.");
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    QVERIFY2(window != nullptr, "The root object should be a QQuickWindow.");
    QObject* startupContentObject = window->findChild<QObject*>(QStringLiteral("startupContent"));
    auto* startupContent = qobject_cast<QQuickItem*>(startupContentObject);
    QVERIFY(startupContent != nullptr);
    window->show();
    QTRY_VERIFY(startupContent->isVisible());
    QTRY_VERIFY(startupContent->width() > 0);
    QTRY_VERIFY(startupContent->height() > 0);
    QObject* startupOverlayObject = window->findChild<QObject*>(QStringLiteral("startupOverlay"));
    auto* startupOverlay = qobject_cast<QQuickItem*>(startupOverlayObject);
    QVERIFY(startupOverlay != nullptr);
    const QPointF contentCenter = startupContent->mapToItem(
        startupOverlay, QPointF(startupContent->width() / 2, startupContent->height() / 2));
    QVERIFY(qAbs(contentCenter.x() - startupOverlay->width() / 2) < 1.0);
    QVERIFY(qAbs(contentCenter.y() - startupOverlay->height() / 2) < 1.0);
    QObject* shellObject = window->findChild<QObject*>(QStringLiteral("shell"));
    auto* shell = qobject_cast<QQuickItem*>(shellObject);
    QVERIFY(shell != nullptr);
    QVERIFY(shell->isVisible());
    QVERIFY(!shell->isEnabled());
    window->setProperty("startupVisible", false);
    QTRY_VERIFY(shell->isEnabled());
    QObject* homePageLoader = window->findChild<QObject*>(QStringLiteral("homePageLoader"));
    QVERIFY(homePageLoader != nullptr);
    QVERIFY(homePageLoader->property("visible").toBool());
    QObject* homePageObject = homePageLoader->property("item").value<QObject*>();
    auto* homePage = qobject_cast<QQuickItem*>(homePageObject);
    QVERIFY(homePage != nullptr);
    QVERIFY(homePage->isVisible());
    QVERIFY(homePage->width() > 0);
    QVERIFY(homePage->height() > 0);
    QObject* sidebarObject = window->findChild<QObject*>(QStringLiteral("sidebar"));
    QObject* mainPanelObject = window->findChild<QObject*>(QStringLiteral("mainPanel"));
    auto* sidebar = qobject_cast<QQuickItem*>(sidebarObject);
    auto* mainPanel = qobject_cast<QQuickItem*>(mainPanelObject);
    QVERIFY(startupOverlay != nullptr);
    QVERIFY(!startupOverlay->property("visible").toBool());
    QVERIFY(sidebar != nullptr);
    QVERIFY(mainPanel != nullptr);
    QVERIFY(sidebar->isVisible());
    QVERIFY(mainPanel->isVisible());
    QObject* sidebarToggleObject =
        window->findChild<QObject*>(QStringLiteral("sidebarToggleButton"));
    QVERIFY(sidebarToggleObject != nullptr);
    auto* sidebarToggle = qobject_cast<QQuickItem*>(sidebarToggleObject);
    QVERIFY(sidebarToggle != nullptr);
    QVERIFY(QMetaObject::invokeMethod(sidebarToggle, "clicked", Qt::DirectConnection));
    QTRY_VERIFY(qAbs(sidebar->width() - 64.0) < 1.0);
    QTRY_VERIFY(qAbs(sidebarToggle->width() - 32.0) < 1.0);
    QCOMPARE(sidebarToggle->property("leftPadding").toReal(), 5.0);
    QVERIFY(QMetaObject::invokeMethod(sidebarToggle, "clicked", Qt::DirectConnection));
    QTRY_VERIFY(qAbs(sidebar->width() - 250.0) < 1.0);
    QObject* settingsBackdropCapture =
        window->findChild<QObject*>(QStringLiteral("settingsBackdropCapture"));
    QVERIFY(settingsBackdropCapture != nullptr);
    QVERIFY(!settingsBackdropCapture->property("hideSource").toBool());
    window->setProperty("runtimeManagerVisible", true);
    QTRY_VERIFY(settingsBackdropCapture->property("hideSource").toBool());
    window->setProperty("runtimeManagerVisible", false);
    QTRY_VERIFY(!settingsBackdropCapture->property("hideSource").toBool());
    const auto iconImages = window->findChildren<QObject*>(QStringLiteral("themeAwareIconImage"));
    QVERIFY(!iconImages.isEmpty());
    const QString iconVariant = themeSettings.theme() == QStringLiteral("dark")
                                    ? QStringLiteral("-light.svg")
                                    : QStringLiteral("-dark.svg");
    for (const QObject* iconImage : iconImages)
    {
        QVERIFY(iconImage->property("source").toString().endsWith(iconVariant));
    }

    QVERIFY(QMetaObject::invokeMethod(window, "selectPage", Qt::DirectConnection,
                                      Q_ARG(QVariant, QStringLiteral("Servers"))));
    QCOMPARE(window->property("selectedPage").toString(), QStringLiteral("Servers"));
    QCoreApplication::processEvents();
    QObject* instancePage = window->findChild<QObject*>(QStringLiteral("instancePage"));
    auto* instancePageItem = qobject_cast<QQuickItem*>(instancePage);
    QVERIFY(instancePageItem != nullptr);
    QVERIFY(!instancePageItem->isVisible());
    QObject* serversPageLoader = window->findChild<QObject*>(QStringLiteral("serversPageLoader"));
    QVERIFY(serversPageLoader != nullptr);
    QVERIFY(serversPageLoader->property("visible").toBool());
    QObject* serverList = window->findChild<QObject*>(QStringLiteral("devServerList"));
    auto* serverListItem = qobject_cast<QQuickItem*>(serverList);
    QVERIFY(serverListItem != nullptr);
    QVERIFY(serverListItem->isVisible());
    QVERIFY(serverListItem->width() > 0);
    QVERIFY(serverListItem->height() > 0);
    QVERIFY(QMetaObject::invokeMethod(window, "selectPage", Qt::DirectConnection,
                                      Q_ARG(QVariant, QStringLiteral("About"))));
    QCoreApplication::processEvents();
    QObject* aboutPageLoader = window->findChild<QObject*>(QStringLiteral("aboutPageLoader"));
    QVERIFY(aboutPageLoader != nullptr);
    QVERIFY(aboutPageLoader->property("visible").toBool());
    QObject* aboutWarning = window->findChild<QObject*>(QStringLiteral("aboutWarning"));
    QVERIFY(aboutWarning != nullptr);
    QVERIFY(aboutWarning->property("text").toString().contains(QStringLiteral("hobby project")));

    QVERIFY(QMetaObject::invokeMethod(window, "selectPage", Qt::DirectConnection,
                                      Q_ARG(QVariant, QStringLiteral("Settings"))));
    QCoreApplication::processEvents();
    QObject* launcherSettingsPanel =
        window->findChild<QObject*>(QStringLiteral("launcherSettingsPanel"));
    QVERIFY(launcherSettingsPanel != nullptr);
    QVERIFY(launcherSettingsPanel->property("visible").toBool());
    QObject* globalJavaFlagsField =
        window->findChild<QObject*>(QStringLiteral("globalJavaFlagsField"));
    QVERIFY(globalJavaFlagsField != nullptr);
    QVERIFY(!globalJavaFlagsField->property("text").toString().isEmpty());
    const QString themeBeforeCancel = themeSettings.theme();
    window->setProperty("pendingDarkMode", themeBeforeCancel != QStringLiteral("dark"));
    QObject* settingsCancelButton =
        window->findChild<QObject*>(QStringLiteral("launcherSettingsCancelButton"));
    QVERIFY(settingsCancelButton != nullptr);
    QVERIFY(QMetaObject::invokeMethod(settingsCancelButton, "clicked", Qt::DirectConnection));
    QCOMPARE(window->property("selectedPage").toString(), QStringLiteral("About"));
    QCOMPARE(themeSettings.theme(), themeBeforeCancel);

    QVERIFY(QMetaObject::invokeMethod(window, "selectPage", Qt::DirectConnection,
                                      Q_ARG(QVariant, QStringLiteral("Settings"))));
    window->setProperty("debugFrameTime", true);
    QCoreApplication::processEvents();
    QObject* frameTimeOverlay = window->findChild<QObject*>(QStringLiteral("frameTimeOverlay"));
    QVERIFY(frameTimeOverlay != nullptr);
    QVERIFY(frameTimeOverlay->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(frameTimeOverlay, "recordFrame", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(frameTimeOverlay, "recordFrame", Qt::DirectConnection));
    QVERIFY(frameTimeOverlay->property("frameDeltas").toList().size() >= 1);
    QVERIFY(QMetaObject::invokeMethod(frameTimeOverlay, "clearSamples", Qt::DirectConnection));
    QCOMPARE(frameTimeOverlay->property("frameDeltas").toList().size(), 0);

    const QString consoleOutput = QStringLiteral("server log line\n").repeated(500);
    QObject* serverConsoleEditor =
        window->findChild<QObject*>(QStringLiteral("serverConsoleEditor"));
    QVERIFY(serverConsoleEditor != nullptr);
    QVERIFY(serverConsoleEditor->setProperty("text", consoleOutput));
    QCOMPARE(serverConsoleEditor->property("cursorPosition").toInt(),
             static_cast<int>(consoleOutput.size()));
    QObject* instanceConsoleEditor =
        window->findChild<QObject*>(QStringLiteral("instanceConsoleTextArea"));
    QVERIFY(instanceConsoleEditor != nullptr);
    QVERIFY(instanceConsoleEditor->setProperty("text", consoleOutput));
    QCOMPARE(instanceConsoleEditor->property("cursorPosition").toInt(),
             static_cast<int>(consoleOutput.size()));
    QObject* serverCreateButton = window->findChild<QObject*>(QStringLiteral("serverCreateButton"));
    QVERIFY(serverList != nullptr);
    QVERIFY(serverCreateButton != nullptr);
    QVERIFY(QMetaObject::invokeMethod(serverCreateButton, "clicked", Qt::DirectConnection));
    QObject* serverCreatePopup = window->findChild<QObject*>(QStringLiteral("serverCreatePopup"));
    QVERIFY(serverCreatePopup != nullptr);
    QTRY_VERIFY(serverCreatePopup->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(serverCreatePopup, "close", Qt::DirectConnection));

    window->setProperty("pendingServerDeletionId",
                        QStringLiteral("8de10e55-6b95-45ab-8c11-70b7d4a0f513"));
    QObject* serverDeleteInput =
        window->findChild<QObject*>(QStringLiteral("serverDeleteConfirmation"));
    QVERIFY(serverDeleteInput != nullptr);
    auto* serverDeleteInputItem = qobject_cast<QQuickItem*>(serverDeleteInput);
    QVERIFY(serverDeleteInputItem != nullptr);
    QTRY_VERIFY(serverDeleteInputItem->hasActiveFocus());
    QGuiApplication::clipboard()->setText(QStringLiteral("DELETE"));
    QTest::keyClick(window, Qt::Key_V, Qt::ControlModifier);
    QCOMPARE(serverDeleteInput->property("text").toString(), QString());
    window->setProperty("pendingServerDeletionId", QString());
    window->setProperty("selectedPage", QStringLiteral("Instances"));

    QObject* accountButton = window->findChild<QObject*>(QStringLiteral("accountButton"));
    QObject* accountFlyout = window->findChild<QObject*>(QStringLiteral("accountFlyout"));
    QVERIFY(accountButton != nullptr);
    QVERIFY(accountFlyout != nullptr);
    QVERIFY(QMetaObject::invokeMethod(accountButton, "clicked", Qt::DirectConnection));
    auto* flyoutItem = qobject_cast<QQuickItem*>(accountFlyout);
    QVERIFY(flyoutItem != nullptr);
    QTRY_VERIFY(flyoutItem->isVisible());
    QObject* deletePrompt =
        window->findChild<QObject*>(QStringLiteral("accountDeleteConfirmationPanel"));
    QVERIFY(deletePrompt != nullptr);
    QVERIFY(QMetaObject::invokeMethod(window, "requestAccountRemoval", Qt::DirectConnection,
                                      Q_ARG(QVariant, QStringLiteral("test-account"))));
    auto* deletePromptItem = qobject_cast<QQuickItem*>(deletePrompt);
    QVERIFY(deletePromptItem != nullptr);
    QTRY_VERIFY(deletePromptItem->isVisible());
    QObject* deleteInput = window->findChild<QObject*>(QStringLiteral("accountDeleteConfirmation"));
    QVERIFY(deleteInput != nullptr);
    auto* deleteInputItem = qobject_cast<QQuickItem*>(deleteInput);
    QVERIFY(deleteInputItem != nullptr);
    QTRY_VERIFY(deleteInputItem->hasActiveFocus());
    QGuiApplication::clipboard()->setText(QStringLiteral("DELETE"));
    QTest::keyClick(window, Qt::Key_V, Qt::ControlModifier);
    QCOMPARE(deleteInput->property("text").toString(), QString());
    if (QGuiApplication::clipboard()->supportsSelection())
    {
        QGuiApplication::clipboard()->setText(QStringLiteral("DELETE"), QClipboard::Selection);
        const QPoint inputCenter =
            deleteInputItem->mapToScene(deleteInputItem->boundingRect().center()).toPoint();
        QTest::mouseClick(window, Qt::MiddleButton, Qt::NoModifier, inputCenter);
        QCOMPARE(deleteInput->property("text").toString(), QString());
    }

    QObject* dependencyPopup =
        window->findChild<QObject*>(QStringLiteral("dependencyConfirmationPopup"));
    QVERIFY(dependencyPopup != nullptr);
    QVERIFY(dependencyPopup->property("width").toDouble() > 0);
    QVERIFY(dependencyPopup->property("height").toDouble() > 0);
    dependencyPopup->setProperty("visible", true);
    QCoreApplication::processEvents();
    QObject* dependencyBackground =
        window->findChild<QObject*>(QStringLiteral("dependencyConfirmationBackground"));
    QObject* dependencyTitle =
        window->findChild<QObject*>(QStringLiteral("dependencyConfirmationTitle"));
    QObject* dependencyCancel =
        window->findChild<QObject*>(QStringLiteral("dependencyConfirmationCancel"));
    QObject* dependencyInstall =
        window->findChild<QObject*>(QStringLiteral("dependencyConfirmationInstall"));
    QVERIFY(dependencyBackground != nullptr);
    QVERIFY(dependencyTitle != nullptr);
    QVERIFY(dependencyCancel != nullptr);
    QVERIFY(dependencyInstall != nullptr);
    auto* dependencyTitleItem = qobject_cast<QQuickItem*>(dependencyTitle);
    auto* dependencyCancelItem = qobject_cast<QQuickItem*>(dependencyCancel);
    auto* dependencyInstallItem = qobject_cast<QQuickItem*>(dependencyInstall);
    QVERIFY(dependencyTitleItem != nullptr);
    QVERIFY(dependencyCancelItem != nullptr);
    QVERIFY(dependencyInstallItem != nullptr);
    QTRY_VERIFY(dependencyTitleItem->isVisible());
    QVERIFY(dependencyTitleItem->width() > 0);
    const qreal titleY = dependencyTitleItem->mapToItem(window->contentItem(), QPointF(0, 0)).y();
    const qreal cancelY = dependencyCancelItem->mapToItem(window->contentItem(), QPointF(0, 0)).y();
    const qreal installY =
        dependencyInstallItem->mapToItem(window->contentItem(), QPointF(0, 0)).y();
    QVERIFY(cancelY > titleY);
    QVERIFY(installY > titleY);

    QObject* settingsPopup = window->findChild<QObject*>(QStringLiteral("instanceSettingsPopup"));
    QVERIFY(settingsPopup != nullptr);
    QVERIFY(settingsPopup->property("width").toDouble() > 0);
    QVERIFY(settingsPopup->property("height").toDouble() > 0);
    QVERIFY(QMetaObject::invokeMethod(settingsPopup, "open", Qt::DirectConnection));
    QTRY_VERIFY(settingsPopup->property("visible").toBool());
    QObject* settingsBackground =
        window->findChild<QObject*>(QStringLiteral("instanceSettingsBackground"));
    QVERIFY(settingsBackground != nullptr);
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
