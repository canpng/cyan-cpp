pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Cyan.Gui

Rectangle {
    id: control
    property var options: []
    property int currentIndex: 0
    implicitHeight: 30
    implicitWidth: 300
    radius: 8
    color: Theme.surfaceAlt
    border.width: 1
    border.color: Theme.border

    Rectangle {
        x: 3 + control.currentIndex * width
        y: 3
        width: control.options.length > 0 ? (control.width - 6) / control.options.length : 0
        height: control.height - 6
        radius: 6
        color: Theme.surfaceElevated
        border.width: 1
        border.color: Theme.strongBorder
        Behavior on x {
            NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 3
        spacing: 3
        Repeater {
            model: control.options
            Button {
                id: segmentButton
                required property int index
                required property string modelData
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: modelData
                font.pixelSize: 11
                font.weight: control.currentIndex === index ? Font.DemiBold : Font.Normal
                background: Item { }
                contentItem: Text {
                    text: segmentButton.text
                    color: control.currentIndex === index ? Theme.text : Theme.secondaryText
                    font: segmentButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: control.currentIndex = index
            }
        }
    }
}
