import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Cyan.Gui

RowLayout {
    id: row
    property string label: ""
    property string description: ""
    property bool checked: false
    signal toggled(bool checked)
    implicitHeight: Math.max(44, labels.implicitHeight)
    spacing: 16

    ColumnLayout {
        id: labels
        Layout.fillWidth: true
        spacing: 2
        Text {
            Layout.fillWidth: true
            text: row.label
            color: Theme.text
            font.pixelSize: 14
            font.weight: Font.Medium
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            text: row.description
            color: Theme.secondaryText
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            visible: text.length > 0
        }
    }
    Switch {
        checked: row.checked
        focusPolicy: Qt.StrongFocus
        onToggled: row.toggled(checked)
    }
}
