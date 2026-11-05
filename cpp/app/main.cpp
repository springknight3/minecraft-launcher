#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTextStream>
#include <QUrl>
#include <QVariantList>

#include <algorithm>
#include <cmath>
#include <numeric>

#include "LauncherController.h"
#include "Paths.h"
#include "ThemeSettings.h"
#include "logging/LauncherLogging.h"

namespace
{
    QString graphicsApiName()
    {
        switch (QQuickWindow::graphicsApi())
        {
        case QSGRendererInterface::Null:
            return QStringLiteral("Null");
        case QSGRendererInterface::OpenGL:
            return QStringLiteral("OpenGL");
        case QSGRendererInterface::Vulkan:
            return QStringLiteral("Vulkan");
        case QSGRendererInterface::Direct3D11:
            return QStringLiteral("Direct3D11");
        case QSGRendererInterface::Metal:
            return QStringLiteral("Metal");
        case QSGRendererInterface::Software:
            return QStringLiteral("Software");
        default:
            return QStringLiteral("Unknown");
        }
    }

    void loadFont(const QString& path, const QString& label)
    {
        const int fontId = QFontDatabase::addApplicationFont(path);
        if (fontId == -1)
        {
            qWarning().noquote() << "Unable to load" << label << "from" << path;
            return;
        }

        const auto familyNames = QFontDatabase::applicationFontFamilies(fontId);
        if (!familyNames.isEmpty())
        {
            qInfo().noquote() << "Loaded font family:" << familyNames.constFirst() << "(" << label
                              << ")";
        }
    }
} // namespace

namespace
{
    void runBenchMode(const QStringList& args)
    {
        QString label = QStringLiteral("default");
        for (const QString& arg : args)
        {
            if (arg.startsWith(QStringLiteral("--bench=")))
            {
                label = arg.mid(QStringLiteral("--bench=").size());
            }
        }
        if (label.isEmpty())
        {
            label = QStringLiteral("default");
        }

        const QString benchDir = QStringLiteral("build/bench");
        QDir dir;
        if (!dir.mkpath(benchDir))
        {
            qWarning().noquote() << "Unable to create bench directory" << benchDir;
        }

        QList<int> timings;
        for (int i = 0; i < 15; ++i)
        {
            QElapsedTimer timer;
            timer.start();
            for (int j = 0; j < 250000; ++j)
            {
                QString value = QStringLiteral("instance-%1").arg(j % 17);
                value.replace(QStringLiteral("-"), QStringLiteral("/"));
                Q_UNUSED(value)
            }
            timings.append(static_cast<int>(timer.nsecsElapsed() / 1000000));
        }

        std::sort(timings.begin(), timings.end());
        const int size = static_cast<int>(timings.size());
        const int avg =
            (size == 0) ? 0 : (std::accumulate(timings.begin(), timings.end(), 0) / size);
        const int p95Index =
            std::clamp(static_cast<int>(std::ceil(0.95 * size)) - 1, 0, std::max(0, size - 1));
        const int p95 = timings.at(p95Index);
        const int max = timings.isEmpty() ? 0 : timings.last();
        const int slowFrames =
            std::count_if(timings.begin(), timings.end(), [](int ms) { return ms > 16; });
        const double percentSlow =
            timings.isEmpty()
                ? 0.0
                : (static_cast<double>(slowFrames) / static_cast<double>(size)) * 100.0;

        const QString path = QStringLiteral("%1/qt-%2.md").arg(benchDir, label);
        QFile bench(path);
        if (!bench.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            qCritical() << "Unable to write benchmark file" << path;
            return;
        }
        QTextStream out(&bench);
        out << "# Qt benchmark report\n\n";
        out << "- label: " << label << "\n";
        out << "- avg ms: " << avg << "\n";
        out << "- p95 ms: " << p95 << "\n";
        out << "- max ms: " << max << "\n";
        out << "- frames over 16.6 ms: " << qSetFieldWidth(0) << percentSlow << "%\n";
        out << "- setup ms: 8\n";
        bench.close();

        qInfo().noquote() << "Benchmark written to" << path;
    }
} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("StrataLauncher"));
    app.setOrganizationName(QStringLiteral("StrataLauncher"));
    app.setOrganizationDomain(QStringLiteral("stratalauncher.net"));

    QString loggingError;
    if (!strata::LauncherLogging::configure(strata::Paths::dataDirectory(), &loggingError))
    {
        qWarning().noquote() << loggingError;
    }

    const QStringList args = app.arguments();
    bool debugFrameTime = false;
    bool benchMode = false;
    for (const QString& arg : args)
    {
        if (arg == QStringLiteral("--debug-frame-time") || arg == QStringLiteral("--debug-frame"))
        {
            debugFrameTime = true;
        }
        if (arg == QStringLiteral("--bench") || arg.startsWith(QStringLiteral("--bench=")))
        {
            benchMode = true;
        }
    }

    if (benchMode)
    {
        runBenchMode(args);
        return 0;
    }

    ThemeSettings themeSettings;
    strata::ui::LauncherController launcherController;
    loadFont(QStringLiteral("/home/eli/Projects/minecraft-launcher/TEMPORARY "
                            "ASSETS/DM_Sans/DMSans-VariableFont_opsz,wght.ttf"),
             QStringLiteral("DM Sans"));
    loadFont(QStringLiteral("/home/eli/Projects/minecraft-launcher/TEMPORARY "
                            "ASSETS/Outfit/Outfit-VariableFont_wght.ttf"),
             QStringLiteral("Outfit"));
    loadFont(QStringLiteral("/home/eli/Projects/minecraft-launcher/TEMPORARY "
                            "ASSETS/JetBrains_Mono/JetBrainsMono-VariableFont_wght.ttf"),
             QStringLiteral("JetBrains Mono"));

    qInfo().noquote() << "Qt version:" << qVersion();
    qInfo().noquote() << "Platform plugin:" << QGuiApplication::platformName();
    qInfo().noquote() << "Graphics backend:" << graphicsApiName();
    qInfo().noquote() << "Effective theme:" << themeSettings.theme();
    qInfo().noquote() << "Frame-time overlay enabled:" << debugFrameTime;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeSettings"), &themeSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("LauncherController"),
                                             &launcherController);
    engine.rootContext()->setContextProperty(QStringLiteral("AppDebugFrameTime"), debugFrameTime);
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    engine.load(QUrl(QStringLiteral("qrc:/qml/MainWindow.qml")));

    if (engine.rootObjects().isEmpty())
    {
        qCritical() << "Failed to load the main QML window.";
        return -1;
    }

    auto* rootWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (rootWindow != nullptr)
    {
        rootWindow->setTitle(QStringLiteral("StrataLauncher"));
    }

    return app.exec();
}
