import QtQuick
import QtQuick.Controls
import Cyan.Gui

ToolButton {
    id: control
    property string glyph: text
    property bool danger: false
    implicitWidth: 28
    implicitHeight: 28
    focusPolicy: Qt.StrongFocus

    contentItem: Text {
        text: control.glyph
        color: !control.enabled ? Theme.tertiaryText
              : control.danger ? Theme.error
              : control.hovered || control.activeFocus ? Theme.text : Theme.secondaryText
        font.pixelSize: 14
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: Theme.controlRadius
        color: control.down ? Theme.surfaceAlt
                            : control.hovered || control.activeFocus
                              ? (control.danger ? Theme.errorSurface : Theme.surfaceHover)
                              : "transparent"
        border.width: 0
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    }
}
