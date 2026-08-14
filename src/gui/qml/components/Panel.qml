import QtQuick
import QtQuick.Layouts
import Cyan.Gui

Rectangle {
    id: panel
    property string title: ""
    property string subtitle: ""
    property string badgeText: ""
    property bool headerVisible: true
    property int padding: Theme.panelPadding
    property int contentSpacing: Theme.panelSpacing
    default property alias contentData: body.data

    radius: Theme.panelRadius
    color: Theme.surface
    border.width: 1
    border.color: Theme.dark ? "#303239" : Theme.border

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: panel.padding
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: panel.headerVisible ? 18 : 0
            visible: panel.headerVisible
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: panel.title.toUpperCase()
                color: Theme.secondaryText
                font.pixelSize: 10
                font.weight: Font.DemiBold
                font.letterSpacing: 0.55
                elide: Text.ElideRight
            }
            Text {
                text: panel.badgeText
                color: Theme.tertiaryText
                font.pixelSize: 10
                visible: text.length > 0
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            visible: panel.headerVisible && panel.subtitle.length > 0
            text: panel.subtitle
            color: Theme.tertiaryText
            font.pixelSize: 9
            elide: Text.ElideRight
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: panel.contentSpacing
        }
    }
}
