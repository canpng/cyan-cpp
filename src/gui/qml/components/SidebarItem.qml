import QtQuick
import QtQuick.Controls
import Cyan.Gui

Button {
    id: control
    property string iconText: ""
    property bool selected: false
    implicitHeight: 44
    leftPadding: 12
    rightPadding: 12
    focusPolicy: Qt.StrongFocus

    contentItem: Row {
        spacing: 11
        Text {
            width: 22
            anchors.verticalCenter: parent.verticalCenter
            text: control.iconText
            color: control.selected ? Theme.accent : Theme.secondaryText
            font.pixelSize: 17
            horizontalAlignment: Text.AlignHCenter
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: Theme.text
            font.pixelSize: 14
            font.weight: control.selected ? Font.DemiBold : Font.Normal
        }
    }

    background: Rectangle {
        radius: 9
        color: control.selected ? Theme.accentSurface
              : control.hovered ? Theme.surfaceAlt : "transparent"
        border.width: control.activeFocus ? 1 : 0
        border.color: Theme.accent
    }
}
