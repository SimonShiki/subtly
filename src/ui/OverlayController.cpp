#include "ui/OverlayController.h"

#include <LayerShellQt/Window>
#include <QClipboard>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>
#include <QSettings>
#include <QTimer>
#include <memory>

OverlayController::OverlayController(QObject *parent) : QObject(parent) {
    QSettings s;
    m_left = s.value("overlay/left", -1).toInt();
    m_bottom = s.value("overlay/bottom", 64).toInt();
    // SUBTLY_NO_LAYERSHELL=1 forces the plain-window fallback (e.g. GNOME/Mutter).
    m_layerShell = QGuiApplication::platformName().startsWith("wayland") &&
                   qEnvironmentVariableIntValue("SUBTLY_NO_LAYERSHELL") == 0;
}

void OverlayController::attach(QQuickWindow *window) {
    m_window = window;
    window->setColor(Qt::transparent);
    window->setFlag(Qt::FramelessWindowHint);
    if (const auto *screen = window->screen()) {
        window->resize(window->size().boundedTo(screen->geometry().size()));
        m_left = std::min(m_left, screen->geometry().width() - window->width());
        m_bottom = std::clamp(m_bottom, 0, screen->geometry().height() - window->height());
    }

    if (m_layerShell) {
        auto *ls = LayerShellQt::Window::get(window);
        ls->setScope("subtly");
        ls->setLayer(LayerShellQt::Window::LayerOverlay); // stay above fullscreen windows.
        ls->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        ls->setExclusiveZone(0); // overlay windows without reserving screen space.
        // These options require newer LayerShellQt releases. Debian 13 ships 6.3,
        // where the keyboard interactivity setting already prevents focus stealing.
        []<typename Window>(Window *surface) {
            if constexpr (requires { surface->setActivateOnShow(false); })
                surface->setActivateOnShow(false);
            if constexpr (requires { surface->setWantsToBeOnActiveScreen(true); })
                surface->setWantsToBeOnActiveScreen(true);
        }(ls);
        applyMargins();
        connect(window, &QQuickWindow::frameSwapped, this, &OverlayController::onFrameSwapped);
    } else {
        window->setFlag(Qt::WindowStaysOnTopHint);
        window->setFlag(Qt::Tool); // hide the overlay from the taskbar.
    }
    setClickThrough(QSettings().value("overlay/clickThrough", false).toBool());
}

void OverlayController::applyMargins() {
    if (!m_window || !m_layerShell)
        return;
    auto *ls = LayerShellQt::Window::get(m_window);
    LayerShellQt::Window::Anchors anchors = LayerShellQt::Window::AnchorBottom;
    if (m_left >= 0)
        anchors |= LayerShellQt::Window::AnchorLeft; // otherwise, the compositor centers horizontally.
    ls->setAnchors(anchors);
    ls->setMargins(QMargins(std::max(m_left, 0), 0, 0, m_bottom));
}

void OverlayController::setClickThrough(bool on) {
    if (on == m_clickThrough && m_window && m_window->flags().testFlag(Qt::WindowTransparentForInput) == on)
        return;
    m_clickThrough = on;
    if (m_window) // Qt sets an empty wl_surface input region for this flag
        m_window->setFlag(Qt::WindowTransparentForInput, on);
    QSettings().setValue("overlay/clickThrough", on);
    emit clickThroughChanged();
}

void OverlayController::setKeyboardFocus(bool on) {
    if (on == m_keyboardFocus)
        return;
    m_keyboardFocus = on;
    if (m_window && m_layerShell) {
        LayerShellQt::Window::get(m_window)->setKeyboardInteractivity(
            on ? LayerShellQt::Window::KeyboardInteractivityOnDemand
               : LayerShellQt::Window::KeyboardInteractivityNone);
        m_window->update(); // layer-shell state is applied on the next commit
    }
    if (on && m_window)
        m_window->requestActivate();
    emit keyboardFocusChanged();
}

void OverlayController::copyToClipboard(const QString &text) {
    if (text.isEmpty() || !m_window)
        return;
    // Wayland compositors accept a new selection only from the client with
    // keyboard focus. Temporarily request focus for the normally unfocused overlay.
    if (!m_layerShell || m_window->isActive()) {
        QGuiApplication::clipboard()->setText(text);
        return;
    }
    const bool hadFocus = m_keyboardFocus;
    setKeyboardFocus(true);
    auto done = std::make_shared<bool>(false);
    auto finish = [this, text, hadFocus, done] {
        if (*done)
            return;
        *done = true;
        QGuiApplication::clipboard()->setText(text);
        if (!hadFocus)
            setKeyboardFocus(false);
    };
    connect(m_window, &QWindow::activeChanged, this, [this, finish] {
        if (m_window && m_window->isActive())
            finish();
    }, Qt::SingleShotConnection);
    QTimer::singleShot(500, this, finish); // Attempt the copy even if focus is not granted.
}

void OverlayController::beginDrag(QPointF windowPos) {
    if (!m_window)
        return;
    if (!m_layerShell) { // the compositor moves regular toplevel windows.
        m_window->startSystemMove();
        return;
    }
    m_dragging = true;
    m_resizeEdges = {};
    m_pressPos = windowPos;
    m_framesUntilSettled = 0;
}

void OverlayController::beginResize(int edges, QPointF windowPos) {
    if (!m_window)
        return;
    if (!m_layerShell) {
        m_window->startSystemResize(Qt::Edges(edges));
        return;
    }
    beginDrag(windowPos);
    m_resizeEdges = Qt::Edges(edges);
}

void OverlayController::dragTo(QPointF windowPos) {
    if (!m_dragging || !m_window || m_framesUntilSettled > 0)
        return; // until the previous move is applied, coordinates use the old position.

    const QPoint delta = (windowPos - m_pressPos).toPoint();
    if (delta.isNull())
        return;

    const QScreen *scr = m_window->screen();
    const QSize screen = scr ? scr->geometry().size() : QSize(1920, 1080);
    if (m_resizeEdges) {
        const int width = m_window->width(), height = m_window->height();
        const int left = m_left < 0 ? (screen.width() - width) / 2 : m_left;
        QRect rect(left, screen.height() - m_bottom - height, width, height);
        const QPoint origin = rect.topLeft();
        if (m_resizeEdges & Qt::LeftEdge)
            rect.setLeft(std::clamp(rect.left() + delta.x(), 0, rect.right() + 1 - m_window->minimumWidth()));
        if (m_resizeEdges & Qt::RightEdge)
            rect.setRight(std::clamp(rect.right() + delta.x(), rect.left() + m_window->minimumWidth() - 1, screen.width() - 1));
        if (m_resizeEdges & Qt::TopEdge)
            rect.setTop(std::clamp(rect.top() + delta.y(), 0, rect.bottom() + 1 - m_window->minimumHeight()));
        if (m_resizeEdges & Qt::BottomEdge)
            rect.setBottom(std::clamp(rect.bottom() + delta.y(), rect.top() + m_window->minimumHeight() - 1, screen.height() - 1));
        m_left = rect.left();
        m_bottom = screen.height() - rect.bottom() - 1;
        m_pressPos = windowPos - (rect.topLeft() - origin);
        m_window->resize(rect.size());
        applyMargins();
        m_framesUntilSettled = 2;
        m_window->update();
        return;
    }
    const int maxLeft = std::max(0, screen.width() - m_window->width());
    const int maxBottom = std::max(0, screen.height() - m_window->height());

    const int left = m_left < 0 ? (screen.width() - m_window->width()) / 2 : m_left;
    const int newLeft = std::clamp(left + delta.x(), 0, maxLeft);
    const int newBottom = std::clamp(m_bottom - delta.y(), 0, maxBottom);
    if (newLeft == m_left && newBottom == m_bottom)
        return; // pinned against a screen edge

    m_left = newLeft;
    m_bottom = newBottom;
    applyMargins();
    // the next frame commits the new margins. Pointer events sent before the
    // compositor processes that commit still use the old surface position.
    // wait one additional frame before using pointer positions again; Qt waits
    // for the compositor's frame callback before presenting that frame.
    m_framesUntilSettled = 2;
    m_window->update();
}

void OverlayController::endDrag() {
    if (!m_dragging)
        return;
    m_dragging = false;
    m_framesUntilSettled = 0;
    savePosition(); // Save once when the drag ends.
}

void OverlayController::onFrameSwapped() {
    if (m_framesUntilSettled > 0 && --m_framesUntilSettled > 0 && m_window)
        m_window->update();
}

void OverlayController::resetPosition() {
    m_left = -1;
    m_bottom = 64;
    applyMargins();
    savePosition();
}

void OverlayController::savePosition() {
    QSettings s;
    s.setValue("overlay/left", m_left);
    s.setValue("overlay/bottom", m_bottom);
}
