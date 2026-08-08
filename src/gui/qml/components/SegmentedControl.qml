import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Cyan.Gui

Rectangle {
    id: control
    property var options: []
    property int currentIndex: 0
    implicitHeight: 38
    implicitWidth: 300
    radius: 9
    color: Theme.surfaceAlt
    border.width: 1
    border.color: Theme.border

    RowLayout {
        anchors.fill: parent
        anchors.margins: 3
        spacing: 3
        Repeater {
            model: control.options
            Button {
                required property int index
                required property string modelData
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: modelData
                font.pixelSize: 13
                font.weight: control.currentIndex === index ? Font.DemiBold : Font.Normal
                background: Rectangle {
                    radius: 7
                    color: control.currentIndex === index ? Theme.surface : "transparent"
                    border.width: control.currentIndex === index ? 1 : 0
                    border.color: Theme.border
                }
                contentItem: Text {
                    text: parent.text
                    color: Theme.text
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: control.currentIndex = index
            }
        }
    }
}
