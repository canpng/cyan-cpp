import QtQuick
import QtQuick.Layouts
import Cyan.Gui

Rectangle {
    id: card
    property string title: ""
    property string description: ""
    default property alias contentData: body.data
    implicitHeight: content.implicitHeight + 40
    radius: Theme.radius
    color: Theme.surface
    border.width: 1
    border.color: Theme.border

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            visible: card.title.length > 0 || card.description.length > 0
            Text {
                text: card.title
                color: Theme.text
                font.pixelSize: 17
                font.weight: Font.DemiBold
                visible: text.length > 0
            }
            Text {
                Layout.fillWidth: true
                text: card.description
                color: Theme.secondaryText
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                visible: text.length > 0
            }
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: 12
        }
    }
}
