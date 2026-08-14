import QtQuick
import QtQuick.Controls
import Cyan.Gui

CheckBox {
    id: control

    implicitHeight: 26
    spacing: 6
    leftPadding: 0
    rightPadding: 0
    focusPolicy: Qt.StrongFocus
    font.pixelSize: 11

    indicator: Rectangle {
        x: control.leftPadding
        y: (control.height - height) / 2
        width: 17
        height: 17
        radius: 5
        color: control.checked ? Theme.accent
                               : control.hovered ? Theme.surfaceHover : Theme.surfaceAlt
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus || control.hovered ? Theme.accent : Theme.strongBorder
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }

        Text {
            anchors.centerIn: parent
            text: "✓"
            color: "white"
            font.pixelSize: 12
            font.weight: Font.Bold
            visible: control.checked
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        color: control.enabled ? Theme.text : Theme.tertiaryText
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
