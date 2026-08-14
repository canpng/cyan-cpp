import QtQuick
import QtQuick.Controls
import Cyan.Gui

ScrollBar {
    id: control
    policy: ScrollBar.AsNeeded
    implicitWidth: 8
    implicitHeight: 8
    padding: 2

    contentItem: Rectangle {
        implicitWidth: 4
        implicitHeight: 40
        radius: 2
        color: control.pressed ? Theme.accent
                               : control.hovered ? Theme.secondaryText : Theme.tertiaryText
        opacity: control.active || control.hovered ? 0.8 : 0.35
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
    }

    background: Item { }
}
