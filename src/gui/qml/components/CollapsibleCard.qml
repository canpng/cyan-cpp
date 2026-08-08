import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Cyan.Gui

Rectangle {
    id: card
    property string title: ""
    property string description: ""
    property bool expanded: false
    default property alias contentData: body.data
    implicitHeight: layout.implicitHeight + 2
    radius: Theme.radius
    color: Theme.surface
    border.width: 1
    border.color: Theme.border

    ColumnLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        spacing: 0

        Button {
            Layout.fillWidth: true
            implicitHeight: 62
            leftPadding: 18
            rightPadding: 18
            background: Rectangle { color: "transparent"; radius: Theme.radius }
            contentItem: RowLayout {
                spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: card.title
                        color: Theme.text
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }
                    Text {
                        Layout.fillWidth: true
                        text: card.description
                        color: Theme.secondaryText
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        visible: text.length > 0
                    }
                }
                Text {
                    text: card.expanded ? "⌃" : "⌄"
                    color: Theme.secondaryText
                    font.pixelSize: 18
                }
            }
            onClicked: card.expanded = !card.expanded
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
            visible: card.expanded
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.margins: 18
            spacing: 12
            visible: card.expanded
        }
    }
}
