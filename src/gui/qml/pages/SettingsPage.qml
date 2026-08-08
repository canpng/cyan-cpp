import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Cyan.Gui
import "../components" as C

Item {
    id: page

    FolderDialog {
        id: outputDirectoryDialog
        title: "Varsayılan çıktı dizinini seç"
        onAccepted: app.settings.defaultOutputDirectory = app.localPath(selectedFolder)
    }
    FolderDialog {
        id: dependencyDirectoryDialog
        title: "Varsayılan dependency directory seç"
        onAccepted: app.settings.defaultDependencyDirectory = app.localPath(selectedFolder)
    }
    FileDialog {
        id: ldidDialog
        title: "Varsayılan ldid.exe seç"
        nameFilters: ["Windows executable (ldid.exe)", "Executable (*.exe)"]
        onAccepted: app.settings.defaultLdidPath = app.localPath(selectedFile)
    }

    Flickable {
        anchors.fill: parent
        contentWidth: availableWidth
        contentHeight: settingsContent.implicitHeight + 56
        clip: true
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: settingsContent
            width: Math.max(560, parent.width - 56)
            x: 28; y: 24
            spacing: 16
            ColumnLayout {
                Layout.fillWidth: true; spacing: 3
                Text { text: "Ayarlar"; color: Theme.text; font.pixelSize: 25; font.weight: Font.DemiBold }
                Text { text: "Uygulama genelindeki kısa ve güvenli varsayılanlar."; color: Theme.secondaryText; font.pixelSize: 13 }
            }
            C.SectionCard {
                Layout.fillWidth: true; title: "Görünüm"
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 2
                        Text { text: "Tema"; color: Theme.text; font.pixelSize: 14; font.weight: Font.Medium }
                        Text { text: "Windows tercihini izleyin veya sabit bir tema seçin."; color: Theme.secondaryText; font.pixelSize: 12 }
                    }
                    ComboBox {
                        implicitWidth: 180; implicitHeight: 42
                        model: ["System", "Light", "Dark"]
                        currentIndex: app.settings.theme === "light" ? 1 : app.settings.theme === "dark" ? 2 : 0
                        onActivated: app.settings.theme = currentIndex === 1 ? "light" : currentIndex === 2 ? "dark" : "system"
                    }
                }
            }
            C.SectionCard {
                Layout.fillWidth: true; title: "İş Varsayılanları"
                C.PathPicker {
                    Layout.fillWidth: true; label: "Varsayılan çıktı dizini"
                    value: app.settings.defaultOutputDirectory; buttonText: "Klasör Seç…"
                    onValueEdited: function(value) { app.settings.defaultOutputDirectory = value }
                    onChooseRequested: outputDirectoryDialog.open()
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "Varsayılan sıkıştırma"; color: Theme.text; font.pixelSize: 14; font.weight: Font.Medium }
                        Item { Layout.fillWidth: true }
                        Text { text: app.settings.defaultCompression; color: Theme.accent; font.pixelSize: 15; font.weight: Font.Bold }
                    }
                    Slider {
                        Layout.fillWidth: true; from: 0; to: 9; stepSize: 1
                        value: app.settings.defaultCompression
                        onMoved: app.settings.defaultCompression = Math.round(value)
                    }
                }
                ComboBox {
                    Layout.fillWidth: true; implicitHeight: 42
                    model: ["Bundled ldid.exe", "Özel ldid.exe"]
                    currentIndex: app.settings.defaultLdidMode
                    onActivated: app.settings.defaultLdidMode = currentIndex
                }
                C.PathPicker {
                    Layout.fillWidth: true; visible: app.settings.defaultLdidMode === 1
                    label: "Özel ldid.exe"; value: app.settings.defaultLdidPath
                    onValueEdited: function(value) { app.settings.defaultLdidPath = value }
                    onChooseRequested: ldidDialog.open()
                }
                C.PathPicker {
                    Layout.fillWidth: true; label: "Varsayılan dependency directory"
                    value: app.settings.defaultDependencyDirectory; buttonText: "Klasör Seç…"
                    onValueEdited: function(value) { app.settings.defaultDependencyDirectory = value }
                    onChooseRequested: dependencyDirectoryDialog.open()
                }
            }
            C.SectionCard {
                Layout.fillWidth: true; title: "About cyan"
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 3
                        Text { text: "cyan"; color: Theme.text; font.pixelSize: 16; font.weight: Font.DemiBold }
                        Text { text: "Yerel Qt 6 arayüzü ve C++20 pipeline"; color: Theme.secondaryText; font.pixelSize: 12 }
                    }
                    ColumnLayout {
                        spacing: 3
                        Text { text: "Core  " + app.cyanVersion; color: Theme.secondaryText; font.pixelSize: 12 }
                        Text { text: "GUI   " + app.guiVersion; color: Theme.secondaryText; font.pixelSize: 12 }
                    }
                }
            }
        }
    }
}
