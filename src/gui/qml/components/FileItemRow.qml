import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Cyan.Gui

Rectangle {
    id: row
    property string fileName: ""
    property string filePath: ""
    property string fileType: ""
    property string targetLabel: ""
    property bool isMissing: false
    property bool reorderable: false
    signal removeRequested()
    signal moveUpRequested()
    signal moveDownRequested()

    implicitHeight: 42
    radius: Theme.controlRadius
    color: rowHover.hovered ? Theme.surfaceHover : Theme.surfaceAlt
    border.width: 1
    border.color: isMissing ? Theme.error : Theme.border

    HoverHandler { id: rowHover }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 7
        anchors.rightMargin: 4
        spacing: 6

        Rectangle {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            radius: 4
            color: row.isMissing ? Theme.errorSurface : Theme.accentSurface
            Text {
                anchors.centerIn: parent
                text: row.fileType === ".cyan" ? "C" : row.fileType === ".dylib" ? "D" : "B"
                color: row.isMissing ? Theme.error : Theme.accent
                font.pixelSize: 11
                font.weight: Font.Bold
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Text {
                    Layout.fillWidth: true
                    text: row.fileName
                    color: Theme.text
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }
                Text {
                    text: row.fileType + (row.targetLabel.length ? " · " + row.targetLabel : "")
                    color: Theme.tertiaryText
                    font.pixelSize: 10
                    visible: row.width >= 390
                }
            }
            Text {
                Layout.fillWidth: true
                text: row.isMissing ? "Dosya bulunamadı" : row.filePath
                color: row.isMissing ? Theme.error : Theme.tertiaryText
                font.pixelSize: 9
                elide: Text.ElideMiddle
                ToolTip.visible: pathHover.hovered
                ToolTip.text: row.filePath
                HoverHandler { id: pathHover }
            }
        }

        Column {
            visible: row.reorderable
            spacing: -2
            IconButton {
                glyph: "↑"
                implicitWidth: 24
                implicitHeight: 19
                onClicked: row.moveUpRequested()
                ToolTip.visible: hovered
                ToolTip.text: "Yukarı taşı"
            }
            IconButton {
                glyph: "↓"
                implicitWidth: 24
                implicitHeight: 19
                onClicked: row.moveDownRequested()
                ToolTip.visible: hovered
                ToolTip.text: "Aşağı taşı"
            }
        }
        IconButton {
            glyph: "×"
            danger: true
            implicitWidth: 30
            implicitHeight: 30
            Accessible.name: "Kaldır"
            onClicked: row.removeRequested()
            ToolTip.visible: hovered
            ToolTip.text: "Kaldır"
        }
    }
}
