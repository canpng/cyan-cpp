import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Cyan.Gui

Rectangle {
    id: zone
    property string title: ""
    property string subtitle: ""
    property string buttonText: "Gözat…"
    signal browseRequested()
    signal filesDropped(var urls)
    implicitHeight: 142
    radius: 12
    color: dropArea.containsDrag ? Theme.accentSurface : Theme.surfaceAlt
    border.width: dropArea.containsDrag ? 2 : 1
    border.color: dropArea.containsDrag ? Theme.accent : Theme.border

    Behavior on color { ColorAnimation { duration: 140 } }

    DropArea {
        id: dropArea
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: function(drop) {
            if (drop.hasUrls) {
                zone.filesDropped(drop.urls)
                drop.acceptProposedAction()
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 440)
        spacing: 6
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "＋"
            color: Theme.accent
            font.pixelSize: 25
        }
        Text {
            Layout.fillWidth: true
            text: zone.title
            color: Theme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
        }
        Text {
            Layout.fillWidth: true
            text: zone.subtitle
            color: Theme.secondaryText
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        AppButton {
            Layout.alignment: Qt.AlignHCenter
            text: zone.buttonText
            compact: true
            onClicked: zone.browseRequested()
        }
    }
}
