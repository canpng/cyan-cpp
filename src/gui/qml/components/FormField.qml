import QtQuick
import QtQuick.Controls
import Cyan.Gui

TextField {
    id: control
    implicitHeight: 42
    leftPadding: 12
    rightPadding: 12
    color: Theme.text
    placeholderTextColor: Theme.tertiaryText
    selectionColor: Theme.accent
    selectedTextColor: "white"
    font.pixelSize: 14
    background: Rectangle {
        radius: 9
        color: Theme.surfaceAlt
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
}
