#include <QGuiApplication>
#include <QQuickWindow>
#include <QTest>
#include <QQmlApplicationEngine>

class QmlSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void smokeLoadMainWindow();
};

void QmlSmokeTest::smokeLoadMainWindow()
{
    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    engine.load(QUrl(QStringLiteral("qrc:/qml/MainWindow.qml")));

    QVERIFY2(!engine.rootObjects().isEmpty(), "The main QML window should load without errors.");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY2(window != nullptr, "The root object should be a QQuickWindow.");
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QmlSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "QmlSmokeTest.moc"
