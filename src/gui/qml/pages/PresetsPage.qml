import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Cyan.Gui
import "../components" as C

Item {
    id: page
    property int selectedPreset: -1
    signal createRequested()

    FileDialog {
        id: relinkDialog
        title: "Eksik dosyayı yeniden bağla"
        onAccepted: app.relinkPreset(page.selectedPreset, selectedFile)
    }
    FolderDialog {
        id: relinkFolderDialog
        title: "Eksik bundle veya klasörü yeniden bağla"
        onAccepted: app.relinkPreset(page.selectedPreset, selectedFolder)
    }
    Menu {
        id: relinkMenu
        MenuItem { text: "Dosya Seç…"; onTriggered: relinkDialog.open() }
        MenuItem { text: "Klasör Seç…"; onTriggered: relinkFolderDialog.open() }
    }
    Dialog {
        id: renameDialog
        anchors.centerIn: parent
        width: 360
        modal: true
        title: "Önayarı Yeniden Adlandır"
        standardButtons: Dialog.Save | Dialog.Cancel
        onAccepted: app.presets.renamePreset(page.selectedPreset, renameField.text)
        C.FormField { id: renameField; width: parent.width; placeholderText: "Yeni ad" }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            spacing: 8
            Text { text: "Önayarlar"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
            Text { text: app.presets.count + " kayıt"; color: Theme.tertiaryText; font.pixelSize: 10 }
            Item { Layout.fillWidth: true }
            C.FormField {
                id: searchField
                Layout.preferredWidth: Math.min(250, page.width * 0.28)
                placeholderText: "Önayar ara"
            }
            C.AppButton { text: "Yeni İşe Git"; compact: true; primary: true; onClicked: page.createRequested() }
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
                    Text { Layout.fillWidth: true; Layout.minimumWidth: 180; text: "ÖNAYAR"; color: Theme.tertiaryText; font.pixelSize: 9; font.weight: Font.DemiBold }
                    Text { Layout.preferredWidth: 250; visible: page.width >= 1020; text: "İÇERİK"; color: Theme.tertiaryText; font.pixelSize: 9; font.weight: Font.DemiBold }
                    Text { Layout.preferredWidth: 90; text: "AYARLAR"; color: Theme.tertiaryText; font.pixelSize: 9; font.weight: Font.DemiBold }
                    Text { Layout.preferredWidth: 140; text: "DURUM"; color: Theme.tertiaryText; font.pixelSize: 9; font.weight: Font.DemiBold }
                    Item { Layout.preferredWidth: 110 }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Text {
                    anchors.centerIn: parent
                    visible: app.presets.count === 0
                    text: "Henüz önayar yok — Yeni İş ekranındaki ayarları kaydedebilirsiniz."
                    color: Theme.tertiaryText
                    font.pixelSize: 12
                }

                ListView {
                    anchors.fill: parent
                    clip: true
                    model: app.presets
                    visible: app.presets.count > 0
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: C.AppScrollBar { }

                    delegate: Rectangle {
                        id: presetRow
                        required property int index
                        required property string name
                        required property string subtitle
                        required property int missingCount
                        required property string missingSummary
                        required property bool includesSettings
                        property bool matchesSearch: searchField.text.length === 0 || name.toLocaleLowerCase().includes(searchField.text.toLocaleLowerCase())

                        width: ListView.view.width
                        height: matchesSearch ? (missingCount > 0 ? 62 : 44) : 0
                        visible: matchesSearch
                        color: hover.hovered ? Theme.surfaceHover : "transparent"
                        clip: true
                        HoverHandler { id: hover }

                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.border }
                        Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 2; visible: presetRow.missingCount > 0; color: Theme.warning }

                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.leftMargin: 10
                            anchors.rightMargin: 8
                            height: 44
                            spacing: 10

                            Text {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 180
                                text: presetRow.name
                                color: Theme.text
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.preferredWidth: 250
                                visible: page.width >= 1020
                                text: presetRow.subtitle
                                color: Theme.secondaryText
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.preferredWidth: 90
                                text: presetRow.includesSettings ? "Dahil" : "—"
                                color: presetRow.includesSettings ? Theme.text : Theme.tertiaryText
                                font.pixelSize: 10
                            }
                            Text {
                                Layout.preferredWidth: 140
                                text: presetRow.missingCount > 0 ? presetRow.missingCount + " eksik dosya" : "Hazır"
                                color: presetRow.missingCount > 0 ? Theme.warning : Theme.success
                                font.pixelSize: 10
                            }
                            RowLayout {
                                Layout.preferredWidth: 110
                                spacing: 3
                                C.AppButton { text: "Uygula"; compact: true; primary: true; onClicked: app.editPreset(presetRow.index) }
                                C.IconButton {
                                    glyph: "···"
                                    implicitWidth: 30
                                    implicitHeight: 28
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Önayar işlemleri"
                                    onClicked: presetMenu.popup()
                                    Menu {
                                        id: presetMenu
                                        MenuItem { text: "Düzenle"; onTriggered: app.editPreset(presetRow.index) }
                                        MenuItem { text: "Çoğalt"; onTriggered: app.presets.duplicatePreset(presetRow.index) }
                                        MenuItem {
                                            text: "Yeniden Adlandır"
                                            onTriggered: {
                                                page.selectedPreset = presetRow.index
                                                renameField.text = presetRow.name
                                                renameDialog.open()
                                            }
                                        }
                                        MenuSeparator { }
                                        MenuItem { text: "Sil"; onTriggered: app.presets.removePreset(presetRow.index) }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 10
                            anchors.rightMargin: 8
                            height: 18
                            visible: presetRow.missingCount > 0
                            Text { Layout.fillWidth: true; text: presetRow.missingSummary; color: Theme.warning; font.pixelSize: 9; elide: Text.ElideRight }
                            Text {
                                text: "Yeniden bağla…"
                                color: Theme.accent
                                font.pixelSize: 9
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        page.selectedPreset = presetRow.index
                                        relinkMenu.popup()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
