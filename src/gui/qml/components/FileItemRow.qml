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
    implicitHeight: 66
    radius: 10
    color: Theme.surfaceAlt
    border.width: 1
    border.color: isMissing ? Theme.error : Theme.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        spacing: 11

        Rectangle {
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            radius: 8
            color: row.isMissing ? Theme.errorSurface : Theme.accentSurface
            Text {
                anchors.centerIn: parent
                text: row.fileType === ".cyan" ? "C" : row.fileType === ".dylib" ? "D" : "◇"
                color: row.isMissing ? Theme.error : Theme.accent
                font.pixelSize: 13
                font.weight: Font.Bold
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    Layout.fillWidth: true
                    text: row.fileName
                    color: Theme.text
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }
                Text {
                    text: row.fileType + (row.targetLabel.length ? "  ·  " + row.targetLabel : "")
                    color: Theme.secondaryText
                    font.pixelSize: 11
                }
            }
            Text {
                Layout.fillWidth: true
                text: row.isMissing ? "Dosya bulunamadı" : row.filePath
                color: row.isMissing ? Theme.error : Theme.tertiaryText
                font.pixelSize: 11
                elide: Text.ElideMiddle
                ToolTip.visible: pathHover.hovered
                ToolTip.text: row.filePath
                HoverHandler { id: pathHover }
            }
        }

        Column {
            visible: row.reorderable
            spacing: -4
            ToolButton {
                text: "⌃"
                implicitWidth: 28; implicitHeight: 25
                onClicked: row.moveUpRequested()
                ToolTip.visible: hovered; ToolTip.text: "Yukarı taşı"
            }
            ToolButton {
                text: "⌄"
                implicitWidth: 28; implicitHeight: 25
                onClicked: row.moveDownRequested()
                ToolTip.visible: hovered; ToolTip.text: "Aşağı taşı"
            }
        }
        ToolButton {
            text: "×"
            implicitWidth: 36; implicitHeight: 40
            Accessible.name: "Kaldır"
            onClicked: row.removeRequested()
            ToolTip.visible: hovered
            ToolTip.text: "Kaldır"
        }
    }
}
