import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Cyan.Gui
import "../components" as C

Item {
    id: page

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            spacing: 8

            Text { text: "İşlem Kuyruğu"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
            Rectangle {
                implicitWidth: countLabel.implicitWidth + 12
                implicitHeight: 22
                radius: 4
                color: Theme.surfaceAlt
                border.width: 1
                border.color: Theme.border
                Text { id: countLabel; anchors.centerIn: parent; text: app.queue.count + " iş"; color: Theme.secondaryText; font.pixelSize: 10 }
            }
            Text {
                Layout.fillWidth: true
                text: app.queue.running ? "İşlem sürüyor" : "İşler sırayla çalıştırılır"
                color: app.queue.running ? Theme.accent : Theme.tertiaryText
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            C.AppButton {
                text: app.queue.running ? "Çalışıyor" : "Kuyruğu Başlat"
                primary: true
                compact: true
                enabled: app.queue.count > 0 && !app.queue.running
                onClicked: app.queue.startQueue()
            }
        }

        C.Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            headerVisible: false
            padding: 0
            contentSpacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                color: Theme.surfaceAlt
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 8
                    spacing: 10
                    Text { Layout.preferredWidth: 82; text: "DURUM"; color: Theme.tertiaryText; font.pixelSize: 9; font.weight: Font.DemiBold }
                    Text { Layout.fillWidth: true; Layout.minimumWidth: 170; text: "UYGULAMA / ÇIKTI"; color: Theme.tertiaryText; font.pixelSize: 9; font.weight: Font.DemiBold }
                    Text { Layout.preferredWidth: 78; text: "ENJEKSİYON"; color: Theme.tertiaryText; font.pixelSize: 9; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignRight }
                    Text { Layout.preferredWidth: 150; visible: page.width >= 1050; text: "İLERLEME"; color: Theme.tertiaryText; font.pixelSize: 9; font.weight: Font.DemiBold }
                    Item { Layout.preferredWidth: 96 }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Text {
                    anchors.centerIn: parent
                    visible: app.queue.count === 0
                    text: "Kuyruk boş — Yeni İş ekranından iş ekleyin."
                    color: Theme.tertiaryText
                    font.pixelSize: 12
                }

                ListView {
                    anchors.fill: parent
                    clip: true
                    model: app.queue
                    visible: app.queue.count > 0
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: C.AppScrollBar { }

                    delegate: Rectangle {
                        id: jobRow
                        required property int index
                        required property string inputName
                        required property string outputName
                        required property string presetName
                        required property int injectionCount
                        required property bool ipapatch
                        required property int status
                        required property string statusText
                        required property real progress
                        required property string stage
                        required property string logs
                        required property string errorText
                        required property string outputPath
                        required property bool canEdit
                        required property bool detailsExpanded
                        property int jobIndex: index
                        objectName: jobIndex.toString()

                        width: ListView.view.width
                        height: detailsExpanded ? 184 : 46
                        color: hover.hovered ? Theme.surfaceHover : (status === 1 ? Theme.accentSurface : "transparent")

                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border }
                        Rectangle {
                            anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                            width: 2; visible: jobRow.status === 1 || jobRow.status === 3
                            color: jobRow.status === 3 ? Theme.error : Theme.accent
                        }
                        HoverHandler { id: hover }

                        Drag.active: dragHandler.active && canEdit
                        Drag.source: jobRow
                        Drag.hotSpot.x: width / 2
                        Drag.hotSpot.y: 22
                        z: Drag.active ? 10 : 0
                        DragHandler { id: dragHandler; enabled: jobRow.canEdit; target: null; acceptedButtons: Qt.LeftButton }
                        DropArea {
                            anchors.fill: parent
                            onEntered: function(drag) {
                                if (drag.source && drag.source !== jobRow && jobRow.canEdit)
                                    app.queue.moveJob(Number(drag.source.objectName), jobRow.jobIndex)
                            }
                        }

                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.leftMargin: 10
                            anchors.rightMargin: 8
                            height: 46
                            spacing: 10

                            Item {
                                Layout.preferredWidth: 82
                                Layout.fillHeight: true
                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 5
                                    Text { text: "⋮"; color: jobRow.canEdit ? Theme.tertiaryText : "transparent"; font.pixelSize: 15 }
                                    C.StatusBadge { text: jobRow.statusText; status: jobRow.status }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 170
                                spacing: 0
                                Text { Layout.fillWidth: true; text: jobRow.inputName; color: Theme.text; font.pixelSize: 11; font.weight: Font.Medium; elide: Text.ElideRight }
                                Text {
                                    Layout.fillWidth: true
                                    text: jobRow.outputName + (jobRow.presetName.length ? "  ·  " + jobRow.presetName : "")
                                    color: Theme.secondaryText
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                            }
                            Text {
                                Layout.preferredWidth: 78
                                text: jobRow.injectionCount + (jobRow.ipapatch ? " + patch" : "")
                                color: Theme.secondaryText
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignRight
                            }
                            Item {
                                Layout.preferredWidth: 150
                                Layout.fillHeight: true
                                visible: page.width >= 1050
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.topMargin: 7
                                    anchors.bottomMargin: 7
                                    spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: jobRow.errorText.length ? jobRow.errorText : (jobRow.stage.length ? jobRow.stage : "Bekliyor")
                                        color: jobRow.errorText.length ? Theme.error : Theme.secondaryText
                                        font.pixelSize: 9
                                        elide: Text.ElideRight
                                    }
                                    ProgressBar { Layout.fillWidth: true; Layout.preferredHeight: 3; from: 0; to: 1; value: jobRow.progress }
                                }
                            }
                            RowLayout {
                                Layout.preferredWidth: 96
                                spacing: 2
                                C.AppButton {
                                    text: jobRow.detailsExpanded ? "Kapat" : "Log"
                                    compact: true
                                    enabled: jobRow.logs.length > 0
                                    onClicked: app.queue.toggleDetails(jobRow.jobIndex)
                                }
                                C.IconButton {
                                    glyph: "···"
                                    implicitWidth: 30
                                    implicitHeight: 28
                                    ToolTip.visible: hovered
                                    ToolTip.text: "İş işlemleri"
                                    onClicked: jobMenu.popup()
                                    Menu {
                                        id: jobMenu
                                        MenuItem { text: "Düzenle"; enabled: jobRow.canEdit; onTriggered: app.editJob(jobRow.jobIndex) }
                                        MenuItem { text: "Çoğalt"; enabled: jobRow.status !== 1; onTriggered: app.queue.duplicateJob(jobRow.jobIndex) }
                                        MenuItem { text: "Klasörde Göster"; enabled: jobRow.status === 2; onTriggered: app.queue.showInFolder(jobRow.jobIndex) }
                                        MenuSeparator { }
                                        MenuItem { text: "Sil"; enabled: jobRow.canEdit; onTriggered: app.queue.removeJob(jobRow.jobIndex) }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.topMargin: 46
                            anchors.bottom: parent.bottom
                            anchors.margins: 8
                            visible: jobRow.detailsExpanded
                            color: Theme.surfaceAlt
                            radius: Theme.controlRadius
                            border.width: 1
                            border.color: Theme.border

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        Layout.fillWidth: true
                                        text: jobRow.errorText.length ? jobRow.errorText : jobRow.outputPath
                                        color: jobRow.errorText.length ? Theme.error : Theme.secondaryText
                                        font.pixelSize: 9
                                        elide: Text.ElideMiddle
                                    }
                                    C.AppButton { text: "Kopyala"; compact: true; onClicked: app.queue.copyLog(jobRow.jobIndex) }
                                }
                                TextArea {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    text: jobRow.logs
                                    readOnly: true
                                    wrapMode: TextEdit.WrapAnywhere
                                    color: Theme.text
                                    font.family: "Cascadia Mono"
                                    font.pixelSize: 10
                                    background: Item { }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
