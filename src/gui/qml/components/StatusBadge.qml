import QtQuick
import Cyan.Gui

Rectangle {
    id: badge
    property string text: ""
    property int status: 0
    implicitWidth: label.implicitWidth + 18
    implicitHeight: 26
    radius: 8
    color: status === 1 ? Theme.accentSurface
         : status === 2 ? (Theme.dark ? "#203d2d" : "#eaf7ef")
         : status === 3 ? Theme.errorSurface : Theme.surfaceAlt
    border.width: 1
    border.color: status === 1 ? Theme.accent
                : status === 2 ? Theme.success
                : status === 3 ? Theme.error : Theme.border
    Text {
        id: label
        anchors.centerIn: parent
        text: badge.text
        color: status === 1 ? Theme.accent
             : status === 2 ? Theme.success
             : status === 3 ? Theme.error : Theme.secondaryText
        font.pixelSize: 11
        font.weight: Font.DemiBold
    }
}
