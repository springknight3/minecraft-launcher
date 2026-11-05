#include <QQuickWindow>
#include <QTest>
#include <QQmlComponent>
#include <QQmlEngine>

class MotionCurveTest : public QObject
{
    Q_OBJECT

private slots:
    void curveConstants();
};

void MotionCurveTest::curveConstants()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(QUrl(QStringLiteral("qrc:/qml/Motion.qml")));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QObject *motion = component.create();
    QVERIFY2(motion != nullptr, "Motion singleton should instantiate.");

    const QVariantList standardCurve = motion->property("standardCurve").toList();
    const QVariantList emphasisCurve = motion->property("emphasisCurve").toList();
    const QVariantList expressCurve = motion->property("expressCurve").toList();

    QCOMPARE(standardCurve.size(), 6);
    QCOMPARE(emphasisCurve.size(), 6);
    QCOMPARE(expressCurve.size(), 6);
    QCOMPARE(standardCurve.at(0).toDouble(), 0.25);
    QCOMPARE(standardCurve.at(1).toDouble(), 0.1);
    QCOMPARE(emphasisCurve.at(2).toDouble(), 0.36);
    QCOMPARE(expressCurve.at(0).toDouble(), 0.2);
    QCOMPARE(expressCurve.at(3).toDouble(), 1.0);
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    MotionCurveTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MotionCurveTest.moc"
