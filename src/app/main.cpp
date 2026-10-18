#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QScreen>
#include <QVariantMap>

#include <cstdio>
#include <cstdlib>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("FRDM_IMX95_QT_DSW"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Suno Diagnostic Software"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Suno diagnostic home for FRDM i.MX95"));
    parser.addHelpOption();
    const QCommandLineOption fullscreenOption(
        QStringLiteral("fullscreen"), QStringLiteral("Start on the full display (default)."));
    const QCommandLineOption windowedOption(
        QStringLiteral("windowed"), QStringLiteral("Use a resizable window for desktop development."));
    parser.addOption(fullscreenOption);
    parser.addOption(windowedOption);
    parser.process(app);

    if (parser.isSet(fullscreenOption) && parser.isSet(windowedOption)) {
        std::fputs("Choose either --fullscreen or --windowed.\n", stderr);
        return EXIT_FAILURE;
    }
    const bool startFullscreen = !parser.isSet(windowedOption);
    QVariantMap initialProperties{
        {QStringLiteral("startFullscreen"), startFullscreen}
    };

    // QScreen reports logical pixels. Respect Windows display scaling and leave
    // room for the desktop and window frame instead of opening an oversized UI.
    if (!startFullscreen) {
        if (const QScreen *screen = app.primaryScreen()) {
            const QSize designSize(1920, 1080);
            const QSize availableSize = screen->availableGeometry().size() * 0.9;
            const QSize initialSize = designSize.scaled(
                availableSize.boundedTo(designSize), Qt::KeepAspectRatio);
            initialProperties.insert(QStringLiteral("width"), initialSize.width());
            initialProperties.insert(QStringLiteral("height"), initialSize.height());
        }
    }

    QQmlApplicationEngine engine;
    engine.setInitialProperties(initialProperties);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.loadFromModule("FRDM_IMX95_QT_DSW", "Main");

    return QGuiApplication::exec();
}
