import QtQuick
import QtQuick.Layouts
import Cyan.Gui

Rectangle {
    id: message
    property string text: ""
    property string kind: "error"
    implicitHeight: row.implicitHeight + 10
    radius: Theme.controlRadius
    color: kind === "warning" ? Theme.warningSurface : Theme.errorSurface
    visible: text.length > 0

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: 5
        spacing: 6
        Text {
            text: "!"
            color: message.kind === "warning" ? Theme.warning : Theme.error
            font.weight: Font.Bold
        }
        Text {
            Layout.fillWidth: true
            text: message.text
            color: message.kind === "warning" ? Theme.warning : Theme.error
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }
    }
}
