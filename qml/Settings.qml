import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: win
    title: qsTr("Subtly Settings")
    width: 520
    height: 380
    property var appearance

    onVisibleChanged: if (visible) models.refresh()

    function modelIndex() {
        return models.catalog.findIndex(entry => entry.name === captions.model)
    }

    function confirm(entry, deleting) {
        confirmation.entry = entry
        confirmation.deleting = deleting
        confirmation.open()
    }

    Dialog {
        id: confirmation
        property var entry: ({})
        property bool deleting: false
        anchors.centerIn: parent
        width: Math.min(win.width - 32, 460)
        title: deleting ? qsTr("Delete model?") : qsTr("Download model?")
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            width: parent.width
            wrapMode: Text.WrapAnywhere
            textFormat: Text.PlainText
            text: (confirmation.entry.name || "")
                  + (confirmation.entry.size ? "\n" + (confirmation.entry.size / 1048576).toFixed(1) + qsTr(" MiB") : "")
                  + (confirmation.deleting && confirmation.entry.name === captions.model
                     ? qsTr("\nRemoving this model will pause captioning.") : "")
        }
        onAccepted: {
            if (deleting) models.remove(entry.name)
            else models.download(entry.name)
        }
    }

    Dialog {
        id: downloadProgress
        anchors.centerIn: parent
        width: Math.min(win.width - 32, 460)
        title: qsTr("Downloading model")
        modal: true
        closePolicy: Popup.NoAutoClose
        standardButtons: Dialog.Cancel
        ColumnLayout {
            width: parent.width
            Label {
                Layout.fillWidth: true
                text: models.downloading
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
            }
            ProgressBar { Layout.fillWidth: true; value: models.progress }
            Label { text: Math.floor(models.progress * 100) + "%" }
        }
        onRejected: models.cancel()
    }

    Dialog {
        id: errorDialog
        property string message
        anchors.centerIn: parent
        width: Math.min(win.width - 32, 460)
        title: qsTr("Model management")
        modal: true
        standardButtons: Dialog.Ok
        Label {
            width: parent.width
            text: errorDialog.message
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }
    }

    Connections {
        target: models
        function onDownloadChanged() {
            if (models.downloading.length) downloadProgress.open()
            else downloadProgress.close()
        }
        function onError(message) {
            errorDialog.message = message
            errorDialog.open()
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        GridLayout {
            width: parent.width - 32
            x: 16; y: 16
            columns: 2
            columnSpacing: 12
            rowSpacing: 10

            Label { text: qsTr("Model") }
            ComboBox {
                id: modelPicker
                Layout.fillWidth: true
                model: models.catalog
                textRole: "label"
                currentIndex: win.modelIndex()
                displayText: currentIndex >= 0 ? currentText : qsTr("Select or download a model…")
                delegate: ItemDelegate {
                    required property var modelData
                    required property int index
                    width: modelPicker.width
                    highlighted: modelPicker.highlightedIndex === index
                    contentItem: RowLayout {
                        Label {
                            Layout.fillWidth: true
                            text: modelData.label
                            elide: Text.ElideRight
                            opacity: modelData.supported ? 1 : 0.5
                        }
                        ToolButton {
                            text: modelData.installed ? "×" : (modelData.supported ? "↓" : "⊘")
                            font.pixelSize: 20
                            enabled: modelData.installed || modelData.supported
                            Accessible.name: modelData.installed ? qsTr("Delete model") : qsTr("Download model")
                            ToolTip.visible: hovered
                            ToolTip.text: modelData.installed ? qsTr("Delete model") : (modelData.supported
                                ? qsTr("Download model") : qsTr("Unavailable in this build"))
                            onClicked: {
                                modelPicker.popup.close()
                                win.confirm(modelData, modelData.installed)
                            }
                        }
                    }
                }
                onActivated: (i) => {
                    const entry = model[i]
                    currentIndex = Qt.binding(() => win.modelIndex())
                    if (!entry.supported) return
                    if (entry.installed) captions.model = entry.name
                    else win.confirm(entry, false)
                }
            }
            Label {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                opacity: 0.7
                font.pointSize: 8
                text: qsTr("Models folder: %1").arg(captions.modelsDir)
            }

            Label { text: qsTr("Audio source") }
            ComboBox {
                Layout.fillWidth: true
                model: [qsTr("System audio"), qsTr("Microphone")]
                currentIndex: captions.source
                onActivated: (i) => captions.source = i
            }

            Label { text: qsTr("Captioning") }
            Switch { checked: captions.running; onToggled: captions.running = checked }

            Label { text: qsTr("Click-through") }
            Switch {
                checked: overlay.clickThrough
                onToggled: overlay.clickThrough = checked
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Mouse input passes through the captions. Unlock the overlay from the tray menu.")
            }

            Label { text: qsTr("Font size") }
            SpinBox {
                from: 12; to: 64
                value: win.appearance ? win.appearance.fontSize : 22
                onValueModified: win.appearance.fontSize = value
            }

            Label { text: qsTr("Background") }
            Slider {
                Layout.fillWidth: true
                from: 0; to: 1
                value: win.appearance ? win.appearance.opacity : 0.75
                onMoved: win.appearance.opacity = value
            }

            Label {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: captions.status
                opacity: 0.8
            }
        }
    }
}
