import QtQuick
import QtQuick.Window
import QtCore

Window {
    id: root
    width: Math.min(Screen.width * 0.6, 1100)
    height: 96
    minimumWidth: 240
    minimumHeight: 96
    visible: false // will show the window after configuring the overlay.
    title: "Subtly"
    color: "transparent"

    function openSettings() {
        settingsLoader.active = true
        settingsLoader.item.show()
        settingsLoader.item.raise()
        settingsLoader.item.requestActivate()
    }

    Settings {
        id: appearance
        category: "appearance"
        property int fontSize: 22
        property real opacity: 0.75
    }

    Settings {
        category: "overlay"
        property alias width: root.width
        property alias height: root.height
    }

    CaptionBar {
        id: bar
        anchors.fill: parent
        lines: captions.lines
        partial: captions.partial
        status: captions.running ? (captions.lines.length || captions.partial ? "" : captions.status)
                                 : qsTr("Paused")
        fontSize: appearance.fontSize
        backgroundOpacity: appearance.opacity

        onDragStarted: (p) => overlay.beginDrag(p)
        onResizeStarted: (edges, p) => overlay.beginResize(edges, p)
        onDragged: (p) => overlay.dragTo(p)
        onDragFinished: overlay.endDrag()
        onCopyRequested: (text) => overlay.copyToClipboard(text)
        onClearRequested: captions.clear()
        onSelectingChanged: overlay.keyboardFocus = selecting
        onSettingsClicked: root.openSettings()
        onPauseClicked: captions.running = !captions.running
        onLockClicked: overlay.clickThrough = true
        running: captions.running
    }

    Loader {
        id: settingsLoader
        active: false
        source: "Settings.qml"
        onLoaded: item.appearance = appearance
    }
}
