import QtQuick
import QtQuick.Controls
import Cyan.Gui

Button {
    id: control
    property bool primary: false
    property bool danger: false
    property bool compact: false
    implicitHeight: compact ? 29 : 33
    implicitWidth: Math.max(compact ? 58 : 82, contentItem.implicitWidth + 20)
    leftPadding: 10
    rightPadding: 10
    focusPolicy: Qt.StrongFocus
    font.pixelSize: compact ? 11 : 12
    font.weight: primary ? Font.DemiBold : Font.Medium

    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? Theme.tertiaryText
              : control.primary ? "white"
              : control.danger ? Theme.error : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.controlRadius
        color: !control.enabled ? Theme.surfaceAlt
             : control.primary ? (control.hovered ? Theme.accentHover : Theme.accent)
             : control.danger ? (control.hovered ? Theme.errorSurface : "transparent")
             : control.down ? Theme.surfaceHover
             : control.hovered ? Theme.surfaceHover : Theme.surfaceAlt
        border.width: control.activeFocus ? 2 : (control.primary || control.danger ? 0 : 1)
        border.color: control.activeFocus ? Theme.accent
                    : control.danger ? Theme.error : Theme.border
        opacity: control.enabled ? 1.0 : 0.55
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    }
}
