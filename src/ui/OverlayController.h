#pragma once
#include <QObject>
#include <QPointF>
#include <QPointer>

class QQuickWindow;

// Configures the caption QQuickWindow as a floating, always-on-top overlay.
// On Wayland compositors with wlr-layer-shell, such as KWin, Sway, and Hyprland,
// uses LayerShellQt's overlay layer with no keyboard focus or exclusive zone.
// Otherwise, uses a frameless stays-on-top window.
class OverlayController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool clickThrough READ clickThrough WRITE setClickThrough NOTIFY clickThroughChanged)
    Q_PROPERTY(bool layerShell READ layerShell CONSTANT)
    // Temporarily allow keyboard focus for text selection, Ctrl+C, and
    // Wayland clipboard ownership.
    Q_PROPERTY(bool keyboardFocus READ keyboardFocus WRITE setKeyboardFocus NOTIFY keyboardFocusChanged)

public:
    explicit OverlayController(QObject *parent = nullptr);

    // Must be called before the window is shown for the first time.
    void attach(QQuickWindow *window);

    bool clickThrough() const { return m_clickThrough; }
    void setClickThrough(bool on);
    bool layerShell() const { return m_layerShell; }

    bool keyboardFocus() const { return m_keyboardFocus; }
    void setKeyboardFocus(bool on);

    // Interactive move in window-local logical pixels. A layer-shell client
    // has no access to its global position.
    Q_INVOKABLE void beginDrag(QPointF windowPos);
    Q_INVOKABLE void beginResize(int edges, QPointF windowPos);
    Q_INVOKABLE void dragTo(QPointF windowPos);
    Q_INVOKABLE void endDrag();
    Q_INVOKABLE void resetPosition();

    Q_INVOKABLE void copyToClipboard(const QString &text);

signals:
    void clickThroughChanged();
    void keyboardFocusChanged();

private:
    void applyMargins();
    void savePosition();
    void onFrameSwapped();

    QPointer<QQuickWindow> m_window;
    bool m_layerShell = false;
    bool m_clickThrough = false;
    bool m_keyboardFocus = false;

    // Keep the pressed point under the pointer by moving the surface by the
    // difference between the current and pressed local positions. A move takes
    // effect when the compositor applies the next commit. Earlier pointer events
    // still use the old position, so allow only one pending move and discard
    // events until the compositor processes it (see onFrameSwapped).
    bool m_dragging = false;
    Qt::Edges m_resizeEdges;
    QPointF m_pressPos;
    int m_framesUntilSettled = 0;
    int m_left = -1; // -1 = centered
    int m_bottom = 64;
};
