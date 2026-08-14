import QtQuick
import QtQuick.Controls
import Cyan.Gui

TextField {
    id: control
    implicitHeight: Theme.controlHeight
    leftPadding: 8
    rightPadding: 8
    color: Theme.text
    placeholderTextColor: Theme.tertiaryText
    selectionColor: Theme.accent
    selectedTextColor: "white"
    font.pixelSize: 12
    background: Rectangle {
        radius: Theme.controlRadius
        color: control.hovered && !control.activeFocus ? Theme.surfaceHover : Theme.surfaceAlt
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent
                                          : control.hovered ? Theme.strongBorder : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
    }
}
