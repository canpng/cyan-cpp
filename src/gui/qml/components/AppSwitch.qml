import QtQuick
import QtQuick.Controls
import Cyan.Gui

Switch {
    id: control
    implicitHeight: 24
    implicitWidth: text.length > 0 ? 38 + spacing + contentItem.implicitWidth : 38
    spacing: 8
    focusPolicy: Qt.StrongFocus
    font.pixelSize: 11

    indicator: Rectangle {
        x: 0
        y: (control.height - height) / 2
        width: 38
        height: 22
        radius: 11
        color: control.checked ? Theme.accent : Theme.strongBorder
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accentHover
        opacity: control.enabled ? 1.0 : 0.5
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }

        Rectangle {
            x: control.checked ? parent.width - width - 2 : 2
            y: 2
            width: 18
            height: 18
            radius: 9
            color: "#ffffff"
            border.width: 1
            border.color: control.checked ? Theme.accentHover : Theme.border
            Behavior on x {
                NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic }
            }
        }
    }

    contentItem: Text {
        leftPadding: control.text.length > 0 ? control.indicator.width + control.spacing : 0
        text: control.text
        color: control.enabled ? Theme.text : Theme.tertiaryText
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
