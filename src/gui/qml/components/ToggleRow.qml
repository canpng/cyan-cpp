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
    implicitHeight: Math.max(30, labels.implicitHeight)
    spacing: 8

    ColumnLayout {
        id: labels
        Layout.fillWidth: true
        spacing: 2
        Text {
            Layout.fillWidth: true
            text: row.label
            color: Theme.text
            font.pixelSize: 12
            font.weight: Font.Medium
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            text: row.description
            color: Theme.secondaryText
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            visible: text.length > 0
        }
    }
    AppSwitch {
        checked: row.checked
        onToggled: row.toggled(checked)
    }
}
