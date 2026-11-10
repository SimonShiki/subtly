#include "ui/Tray.h"

#include <QApplication>
#include <QMenu>
#include <QSystemTrayIcon>

#include "ui/CaptionModel.h"
#include "ui/OverlayController.h"

Tray::Tray(CaptionModel *captions, OverlayController *overlay, QObject *parent)
    : QObject(parent), m_icon(new QSystemTrayIcon(this)), m_menu(new QMenu) {
    m_icon->setIcon(QIcon::fromTheme("subtly", QIcon(":/icons/subtly.svg")));
    m_icon->setToolTip(QStringLiteral("subtly"));

    auto *run = m_menu->addAction(tr("Captioning"));
    run->setCheckable(true);
    run->setChecked(captions->running());
    connect(run, &QAction::toggled, captions, &CaptionModel::setRunning);
    connect(captions, &CaptionModel::runningChanged, run, [run, captions] { run->setChecked(captions->running()); });

    auto *ct = m_menu->addAction(tr("Click-through (lock overlay)"));
    ct->setCheckable(true);
    ct->setChecked(overlay->clickThrough());
    connect(ct, &QAction::toggled, overlay, &OverlayController::setClickThrough);
    connect(overlay, &OverlayController::clickThroughChanged, ct, [ct, overlay] { ct->setChecked(overlay->clickThrough()); });

    auto *srcMenu = m_menu->addMenu(tr("Audio source"));
    auto *sys = srcMenu->addAction(tr("System audio"));
    auto *mic = srcMenu->addAction(tr("Microphone"));
    for (auto *a : {sys, mic})
        a->setCheckable(true);
    auto syncSrc = [=] { sys->setChecked(captions->source() == 0); mic->setChecked(captions->source() == 1); };
    syncSrc();
    connect(sys, &QAction::triggered, captions, [captions] { captions->setSource(0); });
    connect(mic, &QAction::triggered, captions, [captions] { captions->setSource(1); });
    connect(captions, &CaptionModel::sourceChanged, this, syncSrc);

    m_menu->addAction(tr("Show / hide captions"), this, &Tray::toggleOverlayRequested);
    m_menu->addAction(tr("Reset position"), overlay, &OverlayController::resetPosition);
    m_menu->addAction(tr("Clear captions"), captions, &CaptionModel::clear);
    m_menu->addAction(tr("Settings…"), this, &Tray::settingsRequested);
    m_menu->addSeparator();
    m_menu->addAction(tr("Quit"), qApp, &QCoreApplication::quit);

    m_icon->setContextMenu(m_menu);
    connect(m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger)
            emit settingsRequested();
    });
    m_icon->show();
}

Tray::~Tray() { delete m_menu; }
