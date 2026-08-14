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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            Text { text: "Ayarlar"; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
            Text { Layout.fillWidth: true; text: "Uygulama varsayılanları"; color: Theme.tertiaryText; font.pixelSize: 10 }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 2
            rows: 2
            columnSpacing: 7
            rowSpacing: 7

            C.Panel {
                Layout.row: 0
                Layout.column: 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 260
                Layout.horizontalStretchFactor: 32
                title: "GÖRÜNÜM"
                subtitle: "Windows tercihini izleyin veya sabit tema kullanın."

                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: "Tema"; color: Theme.text; font.pixelSize: 11; font.weight: Font.Medium }
                    C.CompactComboBox {
                        Layout.preferredWidth: 150
                        model: ["System", "Light", "Dark"]
                        currentIndex: app.settings.theme === "light" ? 1 : app.settings.theme === "dark" ? 2 : 0
                        onActivated: app.settings.theme = currentIndex === 1 ? "light" : currentIndex === 2 ? "dark" : "system"
                    }
                }
            }

            C.Panel {
                Layout.row: 0
                Layout.column: 1
                Layout.rowSpan: 2
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 520
                Layout.horizontalStretchFactor: 68
                title: "İŞ VARSAYILANLARI"
                subtitle: "Yeni işler bu değerlerle başlatılır."

                C.PathPicker {
                    Layout.fillWidth: true
                    label: "Varsayılan çıktı dizini"
                    value: app.settings.defaultOutputDirectory
                    buttonText: "Seç…"
                    onValueEdited: function(value) { app.settings.defaultOutputDirectory = value }
                    onChooseRequested: outputDirectoryDialog.open()
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text { Layout.preferredWidth: 160; text: "Sıkıştırma"; color: Theme.text; font.pixelSize: 11 }
                    C.AppSlider {
                        Layout.fillWidth: true
                        from: 0
                        to: 9
                        stepSize: 1
                        value: app.settings.defaultCompression
                        onMoved: app.settings.defaultCompression = Math.round(value)
                    }
                    Text { Layout.preferredWidth: 18; text: app.settings.defaultCompression; color: Theme.accent; font.pixelSize: 11; font.weight: Font.Bold }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.preferredWidth: 160; text: "İmzalama aracı"; color: Theme.text; font.pixelSize: 11 }
                    C.CompactComboBox {
                        Layout.fillWidth: true
                        model: ["Bundled ldid.exe", "Özel ldid.exe"]
                        currentIndex: app.settings.defaultLdidMode
                        onActivated: app.settings.defaultLdidMode = currentIndex
                    }
                }
                C.PathPicker {
                    Layout.fillWidth: true
                    visible: app.settings.defaultLdidMode === 1
                    label: "Özel ldid.exe"
                    value: app.settings.defaultLdidPath
                    buttonText: "Seç…"
                    onValueEdited: function(value) { app.settings.defaultLdidPath = value }
                    onChooseRequested: ldidDialog.open()
                }
                C.PathPicker {
                    Layout.fillWidth: true
                    label: "Dependency directory"
                    value: app.settings.defaultDependencyDirectory
                    buttonText: "Seç…"
                    onValueEdited: function(value) { app.settings.defaultDependencyDirectory = value }
                    onChooseRequested: dependencyDirectoryDialog.open()
                }
                Item { Layout.fillHeight: true }
            }

            C.Panel {
                Layout.row: 1
                Layout.column: 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "CYAN"
                subtitle: "Yerel Qt 6 arayüzü ve C++20 pipeline"

                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: "Core"; color: Theme.secondaryText; font.pixelSize: 10 }
                    Text { text: app.cyanVersion; color: Theme.text; font.pixelSize: 10; font.family: "Cascadia Mono" }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { Layout.fillWidth: true; text: "GUI"; color: Theme.secondaryText; font.pixelSize: 10 }
                    Text { text: app.guiVersion; color: Theme.text; font.pixelSize: 10; font.family: "Cascadia Mono" }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
