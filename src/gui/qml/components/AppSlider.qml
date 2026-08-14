import QtQuick
import QtQuick.Controls
import Cyan.Gui

Slider {
    id: control
    implicitHeight: 28
    focusPolicy: Qt.StrongFocus

    background: Item {
        x: control.leftPadding
        y: control.topPadding
        width: control.availableWidth
        height: control.availableHeight

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: 4
            radius: 2
            color: Theme.strongBorder
            opacity: control.enabled ? 0.7 : 0.35
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: control.visualPosition * parent.width
            height: 4
            radius: 2
            color: Theme.accent
            opacity: control.enabled ? 1.0 : 0.45
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.pressed ? 20 : 18
        height: width
        radius: width / 2
        color: "#ffffff"
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? Theme.accent : Theme.border
        Behavior on width { NumberAnimation { duration: Theme.motionFast } }
    }
}
