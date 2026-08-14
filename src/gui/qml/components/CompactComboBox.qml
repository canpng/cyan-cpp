pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Cyan.Gui

ComboBox {
    id: control

    implicitHeight: Theme.controlHeight
    implicitWidth: 160
    leftPadding: 8
    rightPadding: 26
    focusPolicy: Qt.StrongFocus
    font.pixelSize: 11

    delegate: ItemDelegate {
        id: delegateItem
        required property int index
        width: control.width
        height: 28
        text: control.textAt(index)
        highlighted: control.highlightedIndex === index
        hoverEnabled: true
        font.pixelSize: 11
        contentItem: Text {
            text: delegateItem.text
            color: delegateItem.highlighted ? Theme.text : Theme.secondaryText
            font: delegateItem.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: delegateItem.highlighted ? Theme.surfaceHover : Theme.surface
        }
    }

    indicator: Text {
        x: control.width - width - 10
        y: (control.height - height) / 2
        text: "⌄"
        color: control.enabled ? Theme.secondaryText : Theme.tertiaryText
        font.pixelSize: 12
        verticalAlignment: Text.AlignVCenter
    }

    contentItem: Text {
        leftPadding: 0
        rightPadding: 0
        text: control.displayText
        color: control.enabled ? Theme.text : Theme.tertiaryText
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.controlRadius
        color: control.down || control.hovered ? Theme.surfaceHover : Theme.surfaceAlt
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent
                                          : control.hovered ? Theme.strongBorder : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    }

    popup: Popup {
        y: control.height + 3
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 2, 260)
        padding: 1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }
        background: Rectangle {
            radius: Theme.controlRadius
            color: Theme.surfaceElevated
            border.width: 1
            border.color: Theme.strongBorder
        }
    }
}
