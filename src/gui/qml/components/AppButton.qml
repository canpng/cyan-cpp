import QtQuick
import QtQuick.Controls
import Cyan.Gui

Button {
    id: control
    property bool primary: false
    property bool danger: false
    property bool compact: false
    implicitHeight: compact ? 36 : 42
    implicitWidth: Math.max(compact ? 76 : 104, contentItem.implicitWidth + 28)
    leftPadding: 14
    rightPadding: 14
    focusPolicy: Qt.StrongFocus
    font.pixelSize: 14
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
        radius: 9
        color: !control.enabled ? Theme.surfaceAlt
             : control.primary ? (control.hovered ? Theme.accentHover : Theme.accent)
             : control.danger ? (control.hovered ? Theme.errorSurface : "transparent")
             : control.hovered ? Theme.surfaceAlt : Theme.surface
        border.width: control.activeFocus ? 2 : (control.primary ? 0 : 1)
        border.color: control.activeFocus ? Theme.accent
                    : control.danger ? Theme.error : Theme.border
    }
}
