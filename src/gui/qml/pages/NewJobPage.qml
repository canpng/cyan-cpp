import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Cyan.Gui
import "../components" as C

Item {
    id: page
    property var composer: app.composer

    FileDialog {
        id: inputFileDialog
        title: "Uygulama seç"
        nameFilters: ["iOS uygulama paketleri (*.ipa *.tipa)"]
        onAccepted: composer.setInputUrl(selectedFile)
    }
    FolderDialog {
        id: inputAppDialog
        title: ".app klasörünü seç"
        onAccepted: composer.setInputUrl(selectedFolder)
    }
    FileDialog {
        id: injectionFileDialog
        title: "Enjeksiyon dosyaları ekle"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Desteklenen içerikler (*.deb *.dylib *.cyan)", "Tüm dosyalar (*)"]
        onAccepted: composer.addInjectionUrls(selectedFiles)
    }
    FolderDialog {
        id: injectionBundleDialog
        title: ".framework, .appex veya .bundle klasörü seç"
        onAccepted: composer.addInjectionUrl(selectedFolder)
    }
    FileDialog {
        id: payloadFileDialog
        title: "Payload köküne kopyalanacak dosyaları seç"
        fileMode: FileDialog.OpenFiles
        onAccepted: composer.addPayloadUrls(selectedFiles)
    }
    FolderDialog {
        id: payloadFolderDialog
        title: "Payload köküne kopyalanacak klasörü seç"
        onAccepted: composer.addPayloadUrl(selectedFolder)
    }
    FileDialog {
        id: iconDialog
        title: "Uygulama ikonu seç"
        nameFilters: ["Görseller (*.png *.jpg *.jpeg *.bmp *.ico)"]
        onAccepted: composer.iconPath = app.localPath(selectedFile)
    }
    FileDialog {
        id: plistDialog
        title: "Info.plist seç"
        nameFilters: ["Property list (*.plist)"]
        onAccepted: composer.plistPath = app.localPath(selectedFile)
    }
    FileDialog {
        id: entitlementsDialog
        title: "Entitlements dosyası seç"
        nameFilters: ["Property list (*.plist *.entitlements)"]
        onAccepted: composer.entitlementsPath = app.localPath(selectedFile)
    }
    FolderDialog {
        id: outputDirectoryDialog
        title: "Kayıt dizinini seç"
        onAccepted: composer.outputDirectory = app.localPath(selectedFolder)
    }
    FileDialog {
        id: ldidDialog
        title: "ldid.exe seç"
        nameFilters: ["Windows executable (ldid.exe)", "Executable (*.exe)"]
        onAccepted: composer.customLdidPath = app.localPath(selectedFile)
    }
    FolderDialog {
        id: dependencyDialog
        title: "Dependency Directory seç"
        onAccepted: composer.dependencyDirectory = app.localPath(selectedFolder)
    }
    FileDialog {
        id: ipapatchDialog
        title: "iPAPatch payload dylib seç"
        nameFilters: ["Dynamic library (*.dylib)"]
        onAccepted: composer.customIpapatchDylib = app.localPath(selectedFile)
    }

    Menu {
        id: inputBrowseMenu
        MenuItem { text: ".ipa / .tipa dosyası…"; onTriggered: inputFileDialog.open() }
        MenuItem { text: ".app klasörü…"; onTriggered: inputAppDialog.open() }
    }
    Menu {
        id: injectionAddMenu
        MenuItem { text: "Dosya Ekle…"; onTriggered: injectionFileDialog.open() }
        MenuItem { text: "Bundle Klasörü Ekle…"; onTriggered: injectionBundleDialog.open() }
    }
    Menu {
        id: payloadAddMenu
        MenuItem { text: "Dosya Ekle…"; onTriggered: payloadFileDialog.open() }
        MenuItem { text: "Klasör Ekle…"; onTriggered: payloadFolderDialog.open() }
    }

    Dialog {
        id: createPresetDialog
        anchors.centerIn: parent
        width: 420
        modal: true
        title: "Yeni Önayar"
        standardButtons: Dialog.Save | Dialog.Cancel
        onAccepted: {
            app.createPreset(presetName.text, includeSettings.checked)
            presetName.clear()
        }
        ColumnLayout {
            width: parent.width
            spacing: 12
            C.FormField {
                id: presetName
                Layout.fillWidth: true
                placeholderText: "Önayar adı"
            }
            CheckBox {
                id: includeSettings
                text: "İşlem ayarlarını da kaydet"
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            Layout.topMargin: 22
            Layout.bottomMargin: 16
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    text: composer.editing ? "İşi Düzenle" : "Yeni İş"
                    color: Theme.text
                    font.pixelSize: 25
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "Uygulamayı, tweakleri ve çıktı ayarlarını hazırlayın."
                    color: Theme.secondaryText
                    font.pixelSize: 13
                }
            }
            C.AppButton {
                text: "Temizle"
                compact: true
                onClicked: composer.reset()
            }
        }

        Flickable {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            contentHeight: content.implicitHeight + 36
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: content
                width: Math.max(560, scroll.width - 56)
                x: 28
                spacing: 14

                C.SectionCard {
                    Layout.fillWidth: true
                    title: "Kaynak Uygulama"

                    C.DropZone {
                        Layout.fillWidth: true
                        title: "Uygulama Seç"
                        subtitle: ".ipa, .tipa veya .app dosyasını buraya bırakın"
                        visible: composer.inputPath.length === 0
                        onBrowseRequested: inputBrowseMenu.popup()
                        onFilesDropped: function(urls) {
                            if (urls.length > 0) composer.setInputUrl(urls[0])
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 84
                        radius: 11
                        color: Theme.surfaceAlt
                        border.width: 1
                        border.color: Theme.border
                        visible: composer.inputPath.length > 0
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 12
                            Rectangle {
                                Layout.preferredWidth: 44; Layout.preferredHeight: 44
                                radius: 10; color: Theme.accentSurface
                                Text { anchors.centerIn: parent; text: "A"; color: Theme.accent; font.bold: true }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 3
                                Text {
                                    Layout.fillWidth: true
                                    text: composer.inputName
                                    color: Theme.text
                                    font.pixelSize: 15; font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text { text: composer.inputDetails; color: Theme.secondaryText; font.pixelSize: 12 }
                                Text {
                                    Layout.fillWidth: true
                                    text: composer.inputPath
                                    color: Theme.tertiaryText; font.pixelSize: 11; elide: Text.ElideMiddle
                                    ToolTip.visible: inputPathHover.hovered
                                    ToolTip.text: composer.inputPath
                                    HoverHandler { id: inputPathHover }
                                }
                            }
                            C.AppButton { text: "Değiştir"; compact: true; onClicked: inputBrowseMenu.popup() }
                            ToolButton {
                                text: "×"; Accessible.name: "Uygulamayı kaldır"
                                onClicked: composer.clearInput()
                                ToolTip.visible: hovered; ToolTip.text: "Kaldır"
                            }
                        }
                    }
                    C.InlineMessage { Layout.fillWidth: true; text: composer.inputError }
                }

                C.SectionCard {
                    Layout.fillWidth: true
                    title: "Önayar"
                    description: "Kaydedilmiş tweak kombinasyonlarını tek adımda uygulayın."
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        ComboBox {
                            id: presetCombo
                            Layout.fillWidth: true
                            implicitHeight: 42
                            model: app.presets
                            textRole: "name"
                            currentIndex: -1
                            displayText: currentIndex < 0 ? "Yok" : currentText
                        }
                        C.AppButton {
                            text: "Uygula"
                            compact: true
                            enabled: presetCombo.currentIndex >= 0
                            onClicked: presetApplyMenu.popup()
                            Menu {
                                id: presetApplyMenu
                                MenuItem {
                                    text: "Mevcut seçimlerle birleştir"
                                    onTriggered: app.applyPreset(presetCombo.currentIndex, false)
                                }
                                MenuItem {
                                    text: "Mevcut seçimlerin yerine uygula"
                                    onTriggered: app.applyPreset(presetCombo.currentIndex, true)
                                }
                            }
                        }
                        C.AppButton {
                            text: "Yeni Önayar"
                            compact: true
                            onClicked: createPresetDialog.open()
                        }
                    }
                }

                C.SectionCard {
                    Layout.fillWidth: true
                    title: "Enjeksiyonlar"
                    description: "Tweakleri uygulamaya enjekte edin veya IPA Payload köküne ayrı dosyalar ekleyin."

                    C.SegmentedControl {
                        id: injectionTabs
                        Layout.preferredWidth: 340
                        options: ["Injection", "Payload Root"]
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        visible: injectionTabs.currentIndex === 0
                        C.DropZone {
                            Layout.fillWidth: true
                            title: "Tweak veya bileşen ekleyin"
                            subtitle: ".deb, .dylib, .appex, .bundle, .framework ve .cyan"
                            buttonText: "Dosya Ekle…"
                            onBrowseRequested: injectionAddMenu.popup()
                            onFilesDropped: function(urls) { composer.addInjectionUrls(urls) }
                        }
                        C.AppButton {
                            Layout.alignment: Qt.AlignRight
                            text: "Bundle Ekle…"
                            compact: true
                            onClicked: injectionBundleDialog.open()
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: count * 74
                            interactive: false
                            spacing: 8
                            model: composer.injections
                            delegate: C.FileItemRow {
                                required property int index
                                required property string name
                                required property string path
                                required property string type
                                required property string target
                                required property bool missing
                                width: ListView.view.width
                                fileName: name; filePath: path; fileType: type
                                targetLabel: target; isMissing: missing
                                onRemoveRequested: composer.injections.remove(index)
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            visible: composer.cyanPackages.count > 0
                            Text {
                                text: "Cyan Packages — uygulama sırası"
                                color: Theme.secondaryText
                                font.pixelSize: 12; font.weight: Font.DemiBold
                            }
                            ListView {
                                Layout.fillWidth: true
                                Layout.preferredHeight: count * 74
                                interactive: false
                                spacing: 8
                                model: composer.cyanPackages
                                delegate: C.FileItemRow {
                                    required property int index
                                    required property string name
                                    required property string path
                                    required property string type
                                    required property string target
                                    required property bool missing
                                    property int packageIndex: index
                                    width: ListView.view.width
                                    fileName: (index + 1) + ".  " + name
                                    filePath: path; fileType: type; targetLabel: target
                                    isMissing: missing; reorderable: true
                                    Drag.active: packageDrag.active
                                    Drag.source: this
                                    Drag.hotSpot.x: width / 2
                                    Drag.hotSpot.y: height / 2
                                    z: Drag.active ? 5 : 0
                                    DragHandler { id: packageDrag; target: null }
                                    DropArea {
                                        anchors.fill: parent
                                        onEntered: function(drag) {
                                            if (drag.source && drag.source !== parent)
                                                composer.cyanPackages.move(drag.source.packageIndex,
                                                                           parent.packageIndex)
                                        }
                                    }
                                    onRemoveRequested: composer.cyanPackages.remove(index)
                                    onMoveUpRequested: composer.cyanPackages.move(index, index - 1)
                                    onMoveDownRequested: composer.cyanPackages.move(index, index + 1)
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        visible: injectionTabs.currentIndex === 1
                        C.InlineMessage {
                            Layout.fillWidth: true
                            kind: "warning"
                            text: "Bu dosyalar uygulamanın içine enjekte edilmez; IPA’nın Payload dizinine doğrudan kopyalanır."
                        }
                        C.DropZone {
                            Layout.fillWidth: true
                            title: "Payload Root öğeleri"
                            subtitle: "Hedef: IPA package → Payload/"
                            buttonText: "Dosya Ekle…"
                            onBrowseRequested: payloadAddMenu.popup()
                            onFilesDropped: function(urls) { composer.addPayloadUrls(urls) }
                        }
                        C.AppButton {
                            Layout.alignment: Qt.AlignRight
                            text: "Klasör Ekle…"
                            compact: true
                            onClicked: payloadFolderDialog.open()
                        }
                        ListView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: count * 74
                            interactive: false
                            spacing: 8
                            model: composer.payloadRootItems
                            delegate: C.FileItemRow {
                                required property int index
                                required property string name
                                required property string path
                                required property string type
                                required property string target
                                required property bool missing
                                width: ListView.view.width
                                fileName: name; filePath: path; fileType: type
                                targetLabel: target; isMissing: missing
                                onRemoveRequested: composer.payloadRootItems.remove(index)
                            }
                        }
                    }
                }

                C.CollapsibleCard {
                    Layout.fillWidth: true
                    title: "Uygulama Bilgileri"
                    description: "Ad, sürüm, bundle kimliği ve plist değişiklikleri"
                    GridLayout {
                        Layout.fillWidth: true
                        columns: width > 700 ? 2 : 1
                        columnSpacing: 12; rowSpacing: 10
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 5
                            Text { text: "Uygulama Adı"; color: Theme.secondaryText; font.pixelSize: 12 }
                            C.FormField { Layout.fillWidth: true; text: composer.appName; onTextEdited: composer.appName = text }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 5
                            Text { text: "Sürüm"; color: Theme.secondaryText; font.pixelSize: 12 }
                            C.FormField { Layout.fillWidth: true; text: composer.appVersion; onTextEdited: composer.appVersion = text }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 5
                            Text { text: "Bundle Identifier"; color: Theme.secondaryText; font.pixelSize: 12 }
                            C.FormField { Layout.fillWidth: true; text: composer.bundleId; onTextEdited: composer.bundleId = text }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 5
                            Text { text: "Minimum iOS"; color: Theme.secondaryText; font.pixelSize: 12 }
                            C.FormField { Layout.fillWidth: true; text: composer.minimumOs; placeholderText: "örn. 15.0"; onTextEdited: composer.minimumOs = text }
                        }
                    }
                    C.PathPicker {
                        Layout.fillWidth: true; label: "Uygulama İkonu"; value: composer.iconPath
                        onValueEdited: function(value) { composer.iconPath = value }
                        onChooseRequested: iconDialog.open()
                    }
                    C.PathPicker {
                        Layout.fillWidth: true; label: "Info.plist Merge"; value: composer.plistPath
                        onValueEdited: function(value) { composer.plistPath = value }
                        onChooseRequested: plistDialog.open()
                    }
                    C.PathPicker {
                        Layout.fillWidth: true; label: "Entitlements Merge"; value: composer.entitlementsPath
                        onValueEdited: function(value) { composer.entitlementsPath = value }
                        onChooseRequested: entitlementsDialog.open()
                    }
                }

                C.CollapsibleCard {
                    Layout.fillWidth: true
                    title: "Uygulama İşlemleri"
                    description: "Bundle temizleme, binary ve paylaşım seçenekleri"
                    C.ToggleRow { Layout.fillWidth: true; label: "Supported Devices listesini kaldır"; checked: composer.removeSupportedDevices; onToggled: function(v) { composer.removeSupportedDevices = v } }
                    C.ToggleRow { Layout.fillWidth: true; label: "Apple Watch içeriğini kaldır"; checked: composer.noWatch; onToggled: function(v) { composer.noWatch = v } }
                    C.ToggleRow { Layout.fillWidth: true; label: "Document Sharing’i etkinleştir"; checked: composer.enableDocuments; onToggled: function(v) { composer.enableDocuments = v } }
                    C.ToggleRow { Layout.fillWidth: true; label: "Ad-hoc / Fake Sign"; checked: composer.fakeSign; onToggled: function(v) { composer.fakeSign = v } }
                    C.ToggleRow { Layout.fillWidth: true; label: "Yalnız arm64 bırak"; checked: composer.thin; onToggled: function(v) { composer.thin = v } }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 6
                        Text { text: "Extension İşlemi"; color: Theme.secondaryText; font.pixelSize: 12; font.weight: Font.Medium }
                        ComboBox {
                            Layout.fillWidth: true; implicitHeight: 42
                            model: ["Değişiklik yapma", "Tüm App Extension’ları kaldır", "Yalnız şifreli Extension’ları kaldır"]
                            currentIndex: composer.extensionMode
                            onActivated: composer.extensionMode = currentIndex
                        }
                    }
                    C.ToggleRow { Layout.fillWidth: true; label: "Şifreli ana executable’ı görmezden gel"; checked: composer.ignoreEncrypted; onToggled: function(v) { composer.ignoreEncrypted = v } }
                    C.InlineMessage {
                        Layout.fillWidth: true; kind: "warning"
                        text: composer.ignoreEncrypted ? "Bu seçenek önemli bir şifreleme uyumluluk kontrolünü devre dışı bırakır." : ""
                    }
                }

                C.CollapsibleCard {
                    Layout.fillWidth: true
                    title: "Signing & Dependencies"
                    description: "Signer, dependency directory ve uyumluluk"
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 6
                        Text { text: "LDID"; color: Theme.secondaryText; font.pixelSize: 12; font.weight: Font.Medium }
                        ComboBox {
                            Layout.fillWidth: true; implicitHeight: 42
                            model: ["Bundled ldid.exe", "Özel ldid.exe"]
                            currentIndex: composer.ldidMode
                            onActivated: composer.ldidMode = currentIndex
                        }
                    }
                    C.PathPicker {
                        Layout.fillWidth: true; visible: composer.ldidMode === 1
                        label: "Özel ldid.exe"; value: composer.customLdidPath
                        onValueEdited: function(value) { composer.customLdidPath = value }
                        onChooseRequested: ldidDialog.open()
                    }
                    C.PathPicker {
                        Layout.fillWidth: true; label: "Dependency Directory"
                        value: composer.dependencyDirectory; buttonText: "Klasör Seç…"
                        onValueEdited: function(value) { composer.dependencyDirectory = value }
                        onChooseRequested: dependencyDialog.open()
                    }
                    C.ToggleRow {
                        Layout.fillWidth: true; label: "Compatibility Mode"
                        description: "Eski cyan injection semantiğiyle uyumluluğu etkinleştirir."
                        checked: composer.compatibilityMode
                        onToggled: function(v) { composer.compatibilityMode = v }
                    }
                }

                C.SectionCard {
                    Layout.fillWidth: true
                    title: "iPAPatch"
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: "Payload injection ve yeniden imzalama servisini kullanın."
                            color: Theme.secondaryText; font.pixelSize: 13; wrapMode: Text.WordWrap
                        }
                        Switch {
                            text: "Kullan"; checked: composer.ipapatchEnabled
                            onToggled: composer.ipapatchEnabled = checked
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 10; visible: composer.ipapatchEnabled
                        ComboBox {
                            Layout.fillWidth: true; implicitHeight: 42
                            model: ["Bundled zxPluginsInject.dylib", "Custom dylib…"]
                            currentIndex: composer.ipapatchPayloadMode
                            onActivated: composer.ipapatchPayloadMode = currentIndex
                        }
                        C.PathPicker {
                            Layout.fillWidth: true; visible: composer.ipapatchPayloadMode === 1
                            label: "Custom dylib"; value: composer.customIpapatchDylib
                            onValueEdited: function(value) { composer.customIpapatchDylib = value }
                            onChooseRequested: ipapatchDialog.open()
                        }
                        C.ToggleRow {
                            Layout.fillWidth: true; label: "Yalnız App Extensions"
                            checked: composer.ipapatchPluginsOnly
                            onToggled: function(v) { composer.ipapatchPluginsOnly = v }
                        }
                    }
                    C.InlineMessage { Layout.fillWidth: true; text: composer.ipapatchError }
                }

                C.SectionCard {
                    Layout.fillWidth: true
                    title: "Çıktı"
                    GridLayout {
                        Layout.fillWidth: true; columns: width > 700 ? 2 : 1
                        columnSpacing: 12; rowSpacing: 10
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 5
                            Text { text: "Dosya Adı"; color: Theme.secondaryText; font.pixelSize: 12 }
                            C.FormField { Layout.fillWidth: true; text: composer.outputFileName; onTextEdited: composer.outputFileName = text }
                        }
                        C.PathPicker {
                            Layout.fillWidth: true; label: "Kayıt Dizini"; value: composer.outputDirectory
                            buttonText: "Klasör Seç…"
                            onValueEdited: function(value) { composer.outputDirectory = value }
                            onChooseRequested: outputDirectoryDialog.open()
                        }
                    }
                    C.ToggleRow {
                        Layout.fillWidth: true; label: "Mevcut dosyanın üzerine yaz"
                        description: "Hedef dosya zaten varsa güvenli atomik yayınlama ile değiştirir."
                        checked: composer.overwriteExisting
                        onToggled: function(v) { composer.overwriteExisting = v }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "IPA Sıkıştırma"; color: Theme.text; font.pixelSize: 14; font.weight: Font.Medium }
                            Item { Layout.fillWidth: true }
                            Text { text: composer.compression; color: Theme.accent; font.pixelSize: 15; font.weight: Font.Bold }
                        }
                        Slider {
                            Layout.fillWidth: true; from: 0; to: 9; stepSize: 1
                            value: composer.compression
                            onMoved: composer.compression = Math.round(value)
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "0  Daha hızlı"; color: Theme.tertiaryText; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Text { text: "6  Dengeli"; color: Theme.tertiaryText; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Text { text: "9  Daha küçük"; color: Theme.tertiaryText; font.pixelSize: 11 }
                        }
                    }
                }

                Item { Layout.preferredHeight: 4 }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: actionContent.implicitHeight + 24
            color: Theme.surface
            border.width: 1
            border.color: Theme.border
            RowLayout {
                id: actionContent
                anchors.fill: parent
                anchors.leftMargin: 28; anchors.rightMargin: 28
                anchors.topMargin: 12; anchors.bottomMargin: 12
                spacing: 16
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 2
                    Text {
                        Layout.fillWidth: true
                        text: composer.summary
                        color: Theme.secondaryText; font.pixelSize: 12; elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: composer.validationMessage
                        color: Theme.error; font.pixelSize: 11; elide: Text.ElideRight
                        visible: text.length > 0
                    }
                }
                C.AppButton {
                    text: composer.editing ? "İşi Güncelle" : "Kuyruğa Ekle"
                    primary: true
                    enabled: composer.canQueue
                    onClicked: app.commitComposer()
                }
            }
        }
    }
}
