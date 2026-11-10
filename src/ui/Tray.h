#pragma once
#include <QObject>

class QSystemTrayIcon;
class QMenu;
class CaptionModel;
class OverlayController;

// System tray menu for pausing, unlocking, opening Settings, or quitting.
// Remains accessible when the overlay is in click-through mode.
class Tray : public QObject {
    Q_OBJECT
public:
    Tray(CaptionModel *captions, OverlayController *overlay, QObject *parent = nullptr);
    ~Tray() override;

signals:
    void settingsRequested();
    void toggleOverlayRequested();

private:
    QSystemTrayIcon *m_icon;
    QMenu *m_menu;
};
