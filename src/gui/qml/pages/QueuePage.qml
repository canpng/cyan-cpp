import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Cyan.Gui
import "../components" as C

Item {
    id: page

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    text: "İşlem Kuyruğu"
                    color: Theme.text
                    font.pixelSize: 25
                    font.weight: Font.DemiBold
                }
                Text {
                    text: app.queue.count + " iş  •  İşler varsayılan olarak tek tek çalışır"
                    color: Theme.secondaryText
                    font.pixelSize: 13
                }
            }
            C.AppButton {
                text: app.queue.running ? "Kuyruk Çalışıyor" : "Kuyruğu Başlat"
                primary: true
                enabled: app.queue.count > 0 && !app.queue.running
                onClicked: app.queue.startQueue()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 8
                visible: app.queue.count === 0
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "☷"
                    color: Theme.tertiaryText
                    font.pixelSize: 34
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Kuyruk henüz boş"
                    color: Theme.text
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "Yeni İş ekranından ilk IPA işleminizi ekleyin."
                    color: Theme.secondaryText
                    font.pixelSize: 13
                }
            }

            ListView {
                id: queueList
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10
                clip: true
                model: app.queue
                visible: app.queue.count > 0
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

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
                    width: ListView.view.width
                    implicitHeight: rowContent.implicitHeight + 28
                    radius: 12
                    color: status === 1 ? Theme.accentSurface : Theme.surfaceAlt
                    border.width: status === 1 ? 2 : 1
                    border.color: status === 3 ? Theme.error
                                : status === 1 ? Theme.accent : Theme.border

                    Drag.active: dragHandler.active && canEdit
                    Drag.source: jobRow
                    Drag.hotSpot.x: width / 2
                    Drag.hotSpot.y: 24
                    z: Drag.active ? 10 : 0

                    DragHandler {
                        id: dragHandler
                        enabled: jobRow.canEdit
                        target: null
                        acceptedButtons: Qt.LeftButton
                    }
                    DropArea {
                        anchors.fill: parent
                        onEntered: function(drag) {
                            if (drag.source && drag.source !== jobRow && jobRow.canEdit)
                                app.queue.moveJob(drag.source.jobIndex, jobRow.jobIndex)
                        }
                    }

                    ColumnLayout {
                        id: rowContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 14
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Text {
                                text: canEdit ? "≡" : status === 1 ? "◉" : "○"
                                color: status === 1 ? Theme.accent : Theme.tertiaryText
                                font.pixelSize: 18
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Text {
                                    Layout.fillWidth: true
                                    text: inputName
                                    color: Theme.text
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "Çıktı: " + outputName +
                                          (presetName.length ? "  ·  " + presetName : "") +
                                          "  ·  " + injectionCount + " enjeksiyon" +
                                          "  ·  iPAPatch " + (ipapatch ? "açık" : "kapalı")
                                    color: Theme.secondaryText
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }
                            C.StatusBadge { text: statusText; status: jobRow.status }
                            ToolButton {
                                text: "•••"
                                implicitWidth: 42; implicitHeight: 40
                                onClicked: jobMenu.popup()
                                Menu {
                                    id: jobMenu
                                    MenuItem { text: "Düzenle"; enabled: canEdit; onTriggered: app.editJob(index) }
                                    MenuItem { text: "Çoğalt"; enabled: status !== 1; onTriggered: app.queue.duplicateJob(index) }
                                    MenuSeparator { }
                                    MenuItem { text: "Sil"; enabled: canEdit; onTriggered: app.queue.removeJob(index) }
                                }
                            }
                        }

                        ProgressBar {
                            Layout.fillWidth: true
                            from: 0; to: 1; value: progress
                            visible: status === 1 || status === 2
                        }
                        Text {
                            Layout.fillWidth: true
                            text: errorText.length ? errorText : stage
                            color: errorText.length ? Theme.error : Theme.secondaryText
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                            visible: status !== 0 || errorText.length > 0
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            C.AppButton {
                                text: detailsExpanded ? "Ayrıntıları Gizle" : "Ayrıntıları Göster"
                                compact: true
                                enabled: logs.length > 0
                                onClicked: app.queue.toggleDetails(index)
                            }
                            C.AppButton {
                                text: "Klasörde Göster"
                                compact: true
                                visible: status === 2
                                onClicked: app.queue.showInFolder(index)
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: detailsExpanded ? 190 : 0
                            visible: detailsExpanded
                            radius: 9
                            color: Theme.dark ? "#151619" : "#f2f3f5"
                            border.width: 1
                            border.color: Theme.border
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 6
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text { text: "İşlem Günlüğü"; color: Theme.secondaryText; font.pixelSize: 12; font.weight: Font.DemiBold }
                                    Item { Layout.fillWidth: true }
                                    C.AppButton { text: "Logu kopyala"; compact: true; onClicked: app.queue.copyLog(index) }
                                }
                                TextArea {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    text: logs
                                    readOnly: true
                                    wrapMode: TextEdit.WrapAnywhere
                                    color: Theme.text
                                    font.family: "Cascadia Mono"
                                    font.pixelSize: 11
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
