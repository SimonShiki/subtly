import QtQuick
import QtQuick.Controls

Rectangle {
    id: bar
    property var lines: []
    property string partial: ""
    property string status: ""
    property int fontSize: 22
    property real backgroundOpacity: 0.75
    property bool running: true

    signal dragStarted(point windowPos)
    signal resizeStarted(int edges, point windowPos)
    signal dragged(point windowPos)
    signal dragFinished()
    signal copyRequested(string text)
    signal clearRequested()
    signal settingsClicked()
    signal pauseClicked()
    signal lockClicked()

    readonly property real lineHeight: fontSize * 1.35

    radius: 12
    color: Qt.rgba(0, 0, 0, backgroundOpacity)

    readonly property string fullText: historyText()

    // Return the retained history for copying and select mode.
    function historyText() {
        const all = lines.slice()
        if (partial.length) all.push(partial)
        return all.join(" ")
    }

    // Select mode freezes the displayed text to preserve the selection as
    // recognition continues.
    property bool selecting: false
    property string snapshot: ""
    function startSelecting() {
        snapshot = historyText()
        selecting = true
        selectView.forceActiveFocus()
        selectView.selectAll()
        // Start at the newest text, like the live view.
        Qt.callLater(() => flick.contentY = Math.max(0, flick.contentHeight - flick.height))
    }
    function stopSelecting() {
        selecting = false
        selectView.deselect()
    }

    Item {
        id: viewport
        anchors.fill: parent
        anchors.margins: 12
        anchors.rightMargin: 40
        clip: true

        Text {
            id: caption
            visible: !bar.selecting
            width: viewport.width
            anchors.bottom: parent.bottom
            text: bar.fullText.length ? bar.fullText : bar.status
            color: bar.fullText.length ? "white" : "#bbbbbb"
            font.pixelSize: bar.fontSize
            lineHeightMode: Text.FixedHeight
            lineHeight: bar.lineHeight
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }

        Flickable {
            id: flick
            anchors.fill: parent
            visible: bar.selecting
            contentWidth: width
            contentHeight: selectView.contentHeight
            boundsBehavior: Flickable.StopAtBounds
            interactive: false // mouse drags select text; wheel still scrolls
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            TextEdit {
                id: selectView
                width: flick.width
                text: bar.snapshot
                readOnly: true
                selectByMouse: true
                persistentSelection: true
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.PlainText
                color: "white"
                selectionColor: "#3d7fd9"
                selectedTextColor: "white"
                font: caption.font
                Keys.onEscapePressed: bar.stopSelecting()
                Keys.onPressed: (e) => {
                    if (e.matches(StandardKey.Copy)) {
                        bar.copyRequested(selectedText.length ? selectedText : text)
                        e.accepted = true
                    }
                }
                onCursorRectangleChanged: { // keep keyboard-extended selections visible
                    if (cursorRectangle.y < flick.contentY)
                        flick.contentY = cursorRectangle.y
                    else if (cursorRectangle.y + cursorRectangle.height > flick.contentY + flick.height)
                        flick.contentY = cursorRectangle.y + cursorRectangle.height - flick.height
                }
            }
        }
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: pressed && pressedButtons & Qt.LeftButton ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        z: -1
        onPressed: (m) => {
            if (m.button === Qt.RightButton) {
                menu.popup()
                return
            }
            bar.dragStarted(mapToItem(null, m.x, m.y))
        }
        onPositionChanged: (m) => {
            if (pressedButtons & Qt.LeftButton)
                bar.dragged(mapToItem(null, m.x, m.y))
        }
        onReleased: (m) => { if (m.button === Qt.LeftButton) bar.dragFinished() }
        onCanceled: bar.dragFinished()
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        enabled: bar.selecting
        onTapped: menu.popup()
    }

    HoverHandler { id: hover }

    Menu {
        id: menu
        popupType: Popup.Window
        MenuItem {
            text: qsTr("Select...")
            onTriggered: bar.startSelecting()
        }
        MenuItem {
            text: qsTr("Copy All")
            enabled: bar.selecting ? bar.snapshot.length > 0 : bar.fullText.length > 0
            onTriggered: bar.copyRequested(bar.selecting ? bar.snapshot : bar.historyText())
        }
        MenuItem {
            text: qsTr("Clear")
            onTriggered: {
                bar.stopSelecting()
                bar.clearRequested()
            }
        }
    }

    Column {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 6
        spacing: 2
        opacity: hover.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 150 } }

        component IconButton: ToolButton {
            id: button
            width: 26
            height: 26
            padding: 4
            display: AbstractButton.IconOnly
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: text
            // KDE's native ToolButton paints the icon in its background.
            // Provide a content item to retain the icon when replacing the background.
            contentItem: Image {
                source: button.icon.source
                sourceSize: Qt.size(36, 36)
                fillMode: Image.PreserveAspectFit
            }
            background: Rectangle {
                radius: 6
                color: button.hovered ? "#44000000" : "transparent"
            }
        }
        IconButton {
            icon.source: "qrc:/icons/settings.svg"
            text: qsTr("Settings")
            onClicked: bar.settingsClicked()
        }
        IconButton {
            icon.source: bar.running ? "qrc:/icons/pause.svg" : "qrc:/icons/play.svg"
            text: bar.running ? qsTr("Pause captioning") : qsTr("Resume captioning")
            onClicked: bar.pauseClicked()
        }
        IconButton {
            icon.source: "qrc:/icons/lock.svg"
            text: qsTr("Lock overlay")
            onClicked: { bar.stopSelecting(); bar.lockClicked() }
        }
    }

    Repeater {
        model: [Qt.LeftEdge, Qt.RightEdge, Qt.TopEdge, Qt.BottomEdge,
                Qt.TopEdge | Qt.LeftEdge, Qt.TopEdge | Qt.RightEdge,
                Qt.BottomEdge | Qt.LeftEdge, Qt.BottomEdge | Qt.RightEdge]
        MouseArea {
            required property int modelData
            readonly property bool atLeft: modelData & Qt.LeftEdge
            readonly property bool atRight: modelData & Qt.RightEdge
            readonly property bool atTop: modelData & Qt.TopEdge
            readonly property bool atBottom: modelData & Qt.BottomEdge
            x: atRight ? bar.width - width : (atLeft ? 0 : 8)
            y: atBottom ? bar.height - height : (atTop ? 0 : 8)
            width: atLeft || atRight ? 8 : bar.width - 16
            height: atTop || atBottom ? 8 : bar.height - 16
            z: 2
            cursorShape: (atLeft || atRight) && (atTop || atBottom)
                         ? ((atLeft && atTop) || (atRight && atBottom) ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor)
                         : (atLeft || atRight ? Qt.SizeHorCursor : Qt.SizeVerCursor)
            onPressed: (m) => bar.resizeStarted(modelData, mapToItem(null, m.x, m.y))
            onPositionChanged: (m) => { if (pressed) bar.dragged(mapToItem(null, m.x, m.y)) }
            onReleased: bar.dragFinished()
            onCanceled: bar.dragFinished()
        }
    }
}
