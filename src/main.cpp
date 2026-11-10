#include <QApplication>
#include <QIcon>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>

#include "ui/CaptionModel.h"
#include "ui/ModelManager.h"
#include "ui/OverlayController.h"
#include "ui/Tray.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("subtly");
    QApplication::setApplicationName("subtly");
    QApplication::setApplicationDisplayName("Subtly");
    QApplication::setWindowIcon(QIcon(":/icons/subtly.svg"));
    QApplication::setDesktopFileName("subtly");
    QApplication::setQuitOnLastWindowClosed(false);
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        const bool kdeStyle = QFileInfo::exists(
            QLibraryInfo::path(QLibraryInfo::QmlImportsPath) + "/org/kde/desktop/qmldir");
        QQuickStyle::setStyle(kdeStyle ? "org.kde.desktop" : "Fusion");
        QQuickStyle::setFallbackStyle("Fusion");
    }

    CaptionModel captions;
    ModelManager models(&captions);
    OverlayController overlay;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("captions", &captions);
    engine.rootContext()->setContextProperty("models", &models);
    engine.rootContext()->setContextProperty("overlay", &overlay);
    engine.load(QUrl("qrc:/qml/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    overlay.attach(window);
    window->show();

    Tray tray(&captions, &overlay);
    QObject::connect(&tray, &Tray::settingsRequested, window, [window] {
        QMetaObject::invokeMethod(window, "openSettings");
    });
    QObject::connect(&tray, &Tray::toggleOverlayRequested, window, [window] {
        window->setVisible(!window->isVisible());
    });

    return app.exec();
}
