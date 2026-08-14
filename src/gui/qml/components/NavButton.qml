import QtQuick
import QtQuick.Controls
import Cyan.Gui

Button {
    id: control
    property bool selected: false

    implicitHeight: 36
    implicitWidth: Math.max(62, label.implicitWidth + 20)
    leftPadding: 10
    rightPadding: 10
    focusPolicy: Qt.StrongFocus

    contentItem: Text {
        id: label
        text: control.text
        color: control.selected ? Theme.text : Theme.secondaryText
        font.pixelSize: 12
        font.weight: control.selected ? Font.DemiBold : Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.controlRadius
        color: control.down ? Theme.surfaceAlt
                            : control.hovered || control.activeFocus ? Theme.surfaceHover
                                                                    : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    }
}
