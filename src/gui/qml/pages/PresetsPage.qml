import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Cyan.Gui
import "../components" as C

Item {
    id: page
    property int selectedPreset: -1

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
        width: 390
        modal: true
        title: "Önayarı Yeniden Adlandır"
        standardButtons: Dialog.Save | Dialog.Cancel
        onAccepted: app.presets.renamePreset(page.selectedPreset, renameField.text)
        C.FormField { id: renameField; width: parent.width; placeholderText: "Yeni ad" }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 18
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            Text { text: "Önayarlar"; color: Theme.text; font.pixelSize: 25; font.weight: Font.DemiBold }
            Text {
                text: "Sık kullandığınız tweak ve Payload kombinasyonlarını yönetin."
                color: Theme.secondaryText; font.pixelSize: 13
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            radius: Theme.radius; color: Theme.surface
            border.width: 1; border.color: Theme.border

            ColumnLayout {
                anchors.centerIn: parent; spacing: 7
                visible: app.presets.count === 0
                Text { Layout.alignment: Qt.AlignHCenter; text: "◇"; color: Theme.tertiaryText; font.pixelSize: 34 }
                Text { text: "Henüz önayar yok"; color: Theme.text; font.pixelSize: 16; font.weight: Font.DemiBold }
                Text { text: "Yeni İş ekranındaki seçimlerden bir önayar oluşturabilirsiniz."; color: Theme.secondaryText; font.pixelSize: 13 }
            }

            ListView {
                anchors.fill: parent; anchors.margins: 12
                spacing: 10; clip: true; model: app.presets
                visible: app.presets.count > 0
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: Rectangle {
                    required property int index
                    required property string name
                    required property string subtitle
                    required property int missingCount
                    required property string missingSummary
                    required property bool includesSettings
                    width: ListView.view.width
                    implicitHeight: presetContent.implicitHeight + 28
                    radius: 12; color: Theme.surfaceAlt
                    border.width: 1
                    border.color: missingCount > 0 ? Theme.warning : Theme.border
                    ColumnLayout {
                        id: presetContent
                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                        anchors.margins: 14; spacing: 9
                        RowLayout {
                            Layout.fillWidth: true; spacing: 12
                            Rectangle {
                                Layout.preferredWidth: 40; Layout.preferredHeight: 40
                                radius: 10; color: Theme.accentSurface
                                Text { anchors.centerIn: parent; text: "P"; color: Theme.accent; font.bold: true }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 3
                                Text { Layout.fillWidth: true; text: name; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                Text { Layout.fillWidth: true; text: subtitle + (includesSettings ? "  ·  Ayarlar dahil" : ""); color: Theme.secondaryText; font.pixelSize: 12; elide: Text.ElideRight }
                            }
                            C.AppButton {
                                text: "Uygula"; compact: true
                                onClicked: app.editPreset(index)
                            }
                            ToolButton {
                                text: "•••"; implicitWidth: 42; implicitHeight: 40
                                onClicked: presetMenu.popup()
                                Menu {
                                    id: presetMenu
                                    MenuItem { text: "Düzenle"; onTriggered: app.editPreset(index) }
                                    MenuItem { text: "Çoğalt"; onTriggered: app.presets.duplicatePreset(index) }
                                    MenuItem {
                                        text: "Yeniden Adlandır"
                                        onTriggered: { page.selectedPreset = index; renameField.text = name; renameDialog.open() }
                                    }
                                    MenuSeparator { }
                                    MenuItem { text: "Sil"; onTriggered: app.presets.removePreset(index) }
                                }
                            }
                        }
                        C.InlineMessage {
                            Layout.fillWidth: true; kind: "warning"
                            text: missingSummary
                        }
                        C.AppButton {
                            text: "Yeniden Bağla…"; compact: true
                            visible: missingCount > 0
                            onClicked: { page.selectedPreset = index; relinkMenu.popup() }
                        }
                    }
                }
            }
        }
    }
}
