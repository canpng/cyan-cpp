import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Cyan.Gui
import "../components" as C

Item {
    id: page
    property var composer: app.composer
    signal openQueueRequested()
    readonly property bool tightHeight: height < 740
    readonly property int outerMargin: 8
    readonly property int injectionCount: composer.injections.count + composer.cyanPackages.count +
                                          composer.payloadRootItems.count
    readonly property int metadataChangeCount:
        (composer.appName.trim().length > 0 ? 1 : 0) +
        (composer.appVersion.trim().length > 0 ? 1 : 0) +
        (composer.bundleId.trim().length > 0 ? 1 : 0) +
        (composer.minimumOs.trim().length > 0 ? 1 : 0) +
        (composer.iconPath.trim().length > 0 ? 1 : 0) +
        (composer.plistPath.trim().length > 0 ? 1 : 0) +
        (composer.entitlementsPath.trim().length > 0 ? 1 : 0)
    readonly property int operationCount:
        (composer.removeSupportedDevices ? 1 : 0) +
        (composer.noWatch ? 1 : 0) +
        (composer.enableDocuments ? 1 : 0) +
        (composer.fakeSign ? 1 : 0) +
        (composer.thin ? 1 : 0) +
        (composer.ignoreEncrypted ? 1 : 0) +
        (composer.extensionMode !== 0 ? 1 : 0)
    readonly property string advancedSummary:
        (metadataChangeCount === 0 ? "Metadata değişikliği yok" : metadataChangeCount + " metadata alanı") +
        "  ·  " + (operationCount === 0 ? "İşlem seçilmedi" : operationCount + " işlem etkin") +
        "  ·  " + (composer.ldidMode === 0 ? "Bundled ldid" : "Özel ldid")

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
        title: "İçerik dosyaları ekle"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Desteklenen içerikler (*.deb *.dylib *.cyan)", "Tüm dosyalar (*)"]
        onAccepted: composer.addContentUrls(selectedFiles)
    }
    FolderDialog {
        id: injectionBundleDialog
        title: "İçerik klasörü seç"
        onAccepted: composer.addContentUrl(selectedFolder)
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
        id: contentBrowseMenu
        MenuItem { text: "Dosya seç…"; onTriggered: injectionFileDialog.open() }
        MenuItem { text: "Klasör seç…"; onTriggered: injectionBundleDialog.open() }
    }
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

    Dialog {
        id: createPresetDialog
        anchors.centerIn: parent
        width: 400
        modal: true
        title: "Yeni Önayar"
        standardButtons: Dialog.Save | Dialog.Cancel
        onAccepted: {
            app.createPreset(presetName.text, includeSettings.checked)
            presetName.clear()
        }
        ColumnLayout {
            width: parent.width
            spacing: 10
            C.FormField {
                id: presetName
                Layout.fillWidth: true
                placeholderText: "Önayar adı"
            }
            C.CompactCheckBox {
                id: includeSettings
                text: "İşlem ayarlarını da kaydet"
            }
        }
    }

    ColumnLayout {
        id: pageLayout
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: presetToolbar
            Layout.fillWidth: true
            Layout.preferredHeight: page.tightHeight ? 34 : 38
            color: Theme.topBar

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: page.outerMargin
                anchors.rightMargin: page.outerMargin
                spacing: 8
                Text {
                    text: composer.editing ? "İŞİ DÜZENLE" : "YENİ İŞ"
                    color: Theme.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.5
                }
                Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 18; color: Theme.border }
                Text {
                    Layout.fillWidth: true
                    text: page.injectionCount + " içerik  ·  iPAPatch " +
                          (composer.ipapatchEnabled ? "açık" : "kapalı") +
                          "  ·  sıkıştırma " + composer.compression
                    color: Theme.tertiaryText
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
                C.AppButton {
                    text: "Temizle"
                    compact: true
                    onClicked: composer.reset()
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.border
            }
        }

        Item {
            id: workbench
            Layout.fillWidth: true
            Layout.fillHeight: true

            SplitView {
                anchors.fill: parent
                anchors.leftMargin: page.outerMargin
                anchors.rightMargin: page.outerMargin
                anchors.topMargin: 8
                anchors.bottomMargin: 6
                orientation: Qt.Horizontal

                handle: Item {
                    implicitWidth: 8
                    implicitHeight: 8

                    Rectangle {
                        anchors.centerIn: parent
                        width: splitHover.hovered || SplitHandle.pressed ? 2 : 1
                        height: parent.height - 12
                        radius: 1
                        color: SplitHandle.pressed ? Theme.accent
                                                   : splitHover.hovered ? Theme.strongBorder
                                                                        : Theme.border
                        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                        Behavior on width { NumberAnimation { duration: Theme.motionFast } }
                    }
                    HoverHandler {
                        id: splitHover
                        cursorShape: Qt.SplitHCursor
                    }
                }

                ColumnLayout {
                    id: leftColumn
                    SplitView.minimumWidth: 270
                    SplitView.preferredWidth: workbench.width * 0.34
                    spacing: 8

                C.Panel {
                    id: sourcePanel
                    title: "Kaynak Uygulama"
                    badgeText: composer.inputPath.length > 0 ? "Hazır" : "Gerekli"
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    Layout.minimumHeight: page.tightHeight ? 196 : 214
                    Layout.preferredHeight: page.tightHeight ? 196 : 214
                    Layout.maximumHeight: Layout.preferredHeight
                    contentSpacing: 6

                    C.DropZone {
                        Layout.fillWidth: true
                        Layout.preferredHeight: page.tightHeight ? 74 : 82
                        title: "IPA / TIPA / APP seçin"
                        subtitle: ""
                        errorText: composer.inputError
                        visible: composer.inputPath.length === 0
                        onBrowseRequested: inputBrowseMenu.popup()
                        onFilesDropped: function(urls) {
                            if (urls.length > 0)
                                composer.setInputUrl(urls[0])
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: page.tightHeight ? 74 : 82
                        radius: Theme.controlRadius
                        color: Theme.surfaceAlt
                        border.width: 1
                        border.color: Theme.border
                        visible: composer.inputPath.length > 0

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 9
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 42
                                radius: 8
                                color: Theme.accentSurface
                                clip: true
                                Image {
                                    id: selectedApplicationIcon
                                    anchors.fill: parent
                                    source: composer.inputIconUrl
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    smooth: true
                                    mipmap: true
                                    visible: status === Image.Ready
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: "APP"
                                    color: Theme.accent
                                    font.pixelSize: 9
                                    font.weight: Font.Bold
                                    visible: !selectedApplicationIcon.visible
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text {
                                    Layout.fillWidth: true
                                    text: composer.appName.length > 0 ? composer.appName : composer.inputName
                                    color: Theme.text
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: composer.bundleId
                                    color: Theme.secondaryText
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    visible: text.length > 0
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: (composer.appVersion.length > 0 ? "v" + composer.appVersion : "") +
                                          (composer.minimumOs.length > 0 ? "  ·  iOS " + composer.minimumOs : "") +
                                          ((composer.appVersion.length > 0 || composer.minimumOs.length > 0) &&
                                           composer.inputDetails.length > 0 ? "  ·  " : "") +
                                          composer.inputDetails
                                    color: Theme.tertiaryText
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: composer.inputPath
                                    color: Theme.tertiaryText
                                    font.pixelSize: 10
                                    elide: Text.ElideMiddle
                                    ToolTip.visible: sourcePathHover.hovered
                                    ToolTip.text: composer.inputPath
                                    HoverHandler { id: sourcePathHover }
                                }
                            }
                            C.AppButton {
                                text: "Değiştir"
                                compact: true
                                onClicked: inputBrowseMenu.popup()
                                ToolTip.visible: hovered
                                ToolTip.text: "Başka bir uygulama seç"
                            }
                            C.IconButton {
                                glyph: "×"
                                danger: true
                                Accessible.name: "Uygulamayı kaldır"
                                onClicked: composer.clearInput()
                                ToolTip.visible: hovered
                                ToolTip.text: "Kaldır"
                            }
                        }
                    }

                    C.InlineMessage {
                        Layout.fillWidth: true
                        text: composer.inputError
                        visible: composer.inputPath.length > 0 && composer.inputError.length > 0
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "ÖNAYAR"
                                color: Theme.tertiaryText
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignRight
                                text: composer.selectedPresetName
                                color: Theme.tertiaryText
                                font.pixelSize: 9
                                elide: Text.ElideRight
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            C.CompactComboBox {
                                id: presetCombo
                                Layout.fillWidth: true
                                implicitHeight: 30
                                model: app.presets
                                textRole: "name"
                                currentIndex: -1
                                displayText: currentIndex < 0 ? "Önayar seçin" : currentText
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            C.AppButton {
                                Layout.fillWidth: true
                                text: "Uygula"
                                compact: true
                                enabled: presetCombo.currentIndex >= 0
                                onClicked: presetApplyMenu.popup()
                            }
                            C.AppButton {
                                Layout.fillWidth: true
                                text: "Kaydet"
                                compact: true
                                onClicked: createPresetDialog.open()
                            }
                        }
                    }
                }

                C.Panel {
                    id: advancedSettingsPanel
                    title: "Gelişmiş Ayarlar"
                    badgeText: page.metadataChangeCount + " alan · " + page.operationCount + " işlem"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 240
                    contentSpacing: 6

                    Loader {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        active: true
                        sourceComponent: advancedPanelComponent
                    }
                }
            }

                C.Panel {
                    id: injectionPanel
                    title: "İçerik / Enjeksiyonlar"
                    badgeText: page.injectionCount + " öğe"
                    SplitView.minimumWidth: 310
                    SplitView.fillWidth: true
                    contentSpacing: 6

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 7

                        Rectangle {
                            id: injectionSurface
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 120
                            radius: Theme.controlRadius
                            color: injectionDropArea.containsDrag ? Theme.accentSurface
                                                                 : page.injectionCount === 0 ? "transparent"
                                                                                             : Theme.surfaceAlt
                            border.width: page.injectionCount === 0 ? 0
                                                                   : injectionDropArea.containsDrag ? 2 : 1
                            border.color: injectionDropArea.containsDrag ? Theme.accent : Theme.border
                            clip: true

                            DropArea {
                                id: injectionDropArea
                                anchors.fill: parent
                                enabled: page.injectionCount > 0
                                keys: ["text/uri-list"]
                                onDropped: function(drop) {
                                    if (drop.hasUrls) {
                                        composer.addContentUrls(drop.urls)
                                        drop.acceptProposedAction()
                                    }
                                }
                            }

                            C.DropZone {
                                anchors.fill: parent
                                visible: page.injectionCount === 0
                                title: "İçerik ekleyin"
                                subtitle: ".deb · .dylib · .cyan · framework · bundle · appex"
                                onBrowseRequested: contentBrowseMenu.popup()
                                onFilesDropped: function(urls) { composer.addContentUrls(urls) }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 5
                                spacing: 4
                                visible: page.injectionCount > 0

                                ListView {
                                    id: injectionList
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.minimumHeight: visible ? 54 : 0
                                    visible: composer.injections.count > 0
                                    clip: true
                                    spacing: 3
                                    model: composer.injections
                                    boundsBehavior: Flickable.StopAtBounds
                                    ScrollBar.vertical: C.AppScrollBar { }
                                    delegate: C.FileItemRow {
                                        required property int index
                                        required property string name
                                        required property string path
                                        required property string type
                                        required property string target
                                        required property bool missing
                                        width: ListView.view.width
                                        fileName: name
                                        filePath: path
                                        fileType: type
                                        targetLabel: target
                                        isMissing: missing
                                        onRemoveRequested: composer.injections.remove(index)
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 22
                                    visible: composer.cyanPackages.count > 0
                                    Text {
                                        Layout.fillWidth: true
                                        text: "CYAN PACKAGES · UYGULAMA SIRASI"
                                        color: Theme.tertiaryText
                                        font.pixelSize: 9
                                        font.weight: Font.DemiBold
                                    }
                                    Text {
                                        text: composer.cyanPackages.count
                                        color: Theme.tertiaryText
                                        font.pixelSize: 10
                                    }
                                }
                                ListView {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: composer.injections.count === 0
                                    Layout.preferredHeight: Math.min(132, contentHeight)
                                    Layout.minimumHeight: visible ? 52 : 0
                                    visible: composer.cyanPackages.count > 0
                                    clip: true
                                    spacing: 3
                                    model: composer.cyanPackages
                                    boundsBehavior: Flickable.StopAtBounds
                                    ScrollBar.vertical: C.AppScrollBar { }
                                    delegate: C.FileItemRow {
                                        id: cyanRow
                                        required property int index
                                        required property string name
                                        required property string path
                                        required property string type
                                        required property string target
                                        required property bool missing
                                        property int packageIndex: index
                                        objectName: packageIndex.toString()
                                        width: ListView.view.width
                                        fileName: (index + 1) + ".  " + name
                                        filePath: path
                                        fileType: type
                                        targetLabel: target
                                        isMissing: missing
                                        reorderable: true
                                        Drag.active: packageDrag.active
                                        Drag.source: cyanRow
                                        Drag.hotSpot.x: width / 2
                                        Drag.hotSpot.y: height / 2
                                        z: Drag.active ? 5 : 0
                                        DragHandler { id: packageDrag; target: null }
                                        DropArea {
                                            anchors.fill: parent
                                            onEntered: function(drag) {
                                                if (drag.source && drag.source !== cyanRow)
                                                    composer.cyanPackages.move(Number(drag.source.objectName),
                                                                               cyanRow.packageIndex)
                                            }
                                        }
                                        onRemoveRequested: composer.cyanPackages.remove(index)
                                        onMoveUpRequested: composer.cyanPackages.move(index, index - 1)
                                        onMoveDownRequested: composer.cyanPackages.move(index, index + 1)
                                    }
                                }

                                ListView {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: composer.injections.count === 0 &&
                                                       composer.cyanPackages.count === 0
                                    Layout.preferredHeight: Math.min(132, contentHeight)
                                    Layout.minimumHeight: visible ? 42 : 0
                                    visible: composer.payloadRootItems.count > 0
                                    clip: true
                                    spacing: 3
                                    model: composer.payloadRootItems
                                    boundsBehavior: Flickable.StopAtBounds
                                    ScrollBar.vertical: C.AppScrollBar { }
                                    delegate: C.FileItemRow {
                                        required property int index
                                        required property string name
                                        required property string path
                                        required property string type
                                        required property string target
                                        required property bool missing
                                        width: ListView.view.width
                                        fileName: name
                                        filePath: path
                                        fileType: type
                                        targetLabel: target
                                        isMissing: missing
                                        onRemoveRequested: composer.payloadRootItems.remove(index)
                                    }
                                }
                            }
                        }

                        C.DropZone {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            visible: page.injectionCount > 0
                            compact: true
                            title: "İçerik ekle"
                            onBrowseRequested: contentBrowseMenu.popup()
                            onFilesDropped: function(urls) { composer.addContentUrls(urls) }
                        }
                    }

                }

                ColumnLayout {
                    id: rightColumn
                    SplitView.minimumWidth: 260
                    SplitView.preferredWidth: workbench.width * 0.26
                    spacing: 8

                C.Panel {
                    id: outputPanel
                    title: "Çıktı"
                    badgeText: "Sıkıştırma " + composer.compression
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    Layout.minimumHeight: page.tightHeight ? 300 : 318
                    Layout.preferredHeight: page.tightHeight ? 300 : 318
                    Layout.maximumHeight: Layout.preferredHeight
                    contentSpacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        Text {
                            Layout.preferredWidth: 58
                            text: "Dosya adı"
                            color: Theme.secondaryText
                            font.pixelSize: 10
                        }
                        C.FormField {
                            Layout.fillWidth: true
                            text: composer.outputFileName
                            onTextEdited: composer.outputFileName = text
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        Text {
                            Layout.preferredWidth: 58
                            text: "Kayıt yeri"
                            color: Theme.secondaryText
                            font.pixelSize: 10
                        }
                        C.FormField {
                            Layout.fillWidth: true
                            text: composer.outputDirectory
                            placeholderText: "Klasör seçilmedi"
                            onTextEdited: composer.outputDirectory = text
                        }
                        C.IconButton {
                            glyph: "…"
                            implicitHeight: Theme.controlHeight
                            Accessible.name: "Kayıt dizinini seç"
                            onClicked: outputDirectoryDialog.open()
                            ToolTip.visible: hovered
                            ToolTip.text: "Kayıt dizinini seç"
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        Text {
                            Layout.preferredWidth: 58
                            text: "Sıkıştırma"
                            color: Theme.secondaryText
                            font.pixelSize: 10
                        }
                        C.AppSlider {
                            Layout.fillWidth: true
                            implicitHeight: 26
                            from: 0; to: 9; stepSize: 1
                            value: composer.compression
                            onMoved: composer.compression = Math.round(value)
                        }
                        Text {
                            Layout.preferredWidth: 14
                            text: composer.compression
                            color: Theme.accent
                            font.pixelSize: 11
                            font.weight: Font.Bold
                        }
                    }
                    C.CompactCheckBox {
                        Layout.fillWidth: true
                        text: "Mevcut dosyanın üzerine yaz"
                        checked: composer.overwriteExisting
                        onToggled: composer.overwriteExisting = checked
                    }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        Text {
                            Layout.fillWidth: true
                            text: "iPAPatch"
                            color: Theme.text
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: composer.ipapatchEnabled ? "AÇIK" : "KAPALI"
                            color: composer.ipapatchEnabled ? Theme.success : Theme.tertiaryText
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }
                        C.AppSwitch {
                            Accessible.name: "iPAPatch"
                            checked: composer.ipapatchEnabled
                            onToggled: composer.ipapatchEnabled = checked
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                        Layout.minimumHeight: 78
                        Layout.preferredHeight: 78
                        clip: true

                        ColumnLayout {
                            id: ipapatchContent
                            anchors.fill: parent
                            spacing: 4
                            enabled: composer.ipapatchEnabled
                            opacity: enabled ? 1.0 : 0.42
                            Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    C.CompactComboBox {
                                        Layout.fillWidth: true
                                        model: ["Bundled zxPluginsInject.dylib", "Custom dylib…"]
                                        currentIndex: composer.ipapatchPayloadMode
                                        onActivated: composer.ipapatchPayloadMode = currentIndex
                                    }
                                    C.CompactCheckBox {
                                        text: "Plugins Only"
                                        checked: composer.ipapatchPluginsOnly
                                        onToggled: composer.ipapatchPluginsOnly = checked
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    C.FormField {
                                        Layout.fillWidth: true
                                        text: composer.customIpapatchDylib
                                        placeholderText: composer.ipapatchPayloadMode === 1
                                                         ? "Custom dylib yolu"
                                                         : "Özel dylib seçildiğinde kullanılır"
                                        enabled: composer.ipapatchEnabled && composer.ipapatchPayloadMode === 1
                                        onTextEdited: composer.customIpapatchDylib = text
                                    }
                                    C.IconButton {
                                        glyph: "…"
                                        implicitHeight: Theme.controlHeight
                                        enabled: composer.ipapatchEnabled && composer.ipapatchPayloadMode === 1
                                        onClicked: ipapatchDialog.open()
                                        ToolTip.visible: hovered
                                        ToolTip.text: "iPAPatch dylib seç"
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 10
                                    text: composer.ipapatchError
                                    color: Theme.error
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                    opacity: text.length > 0 ? 1.0 : 0.0
                                }
                        }
                    }
                }

                C.Panel {
                    id: quickQueuePanel
                    title: "Hızlı Kuyruk"
                    badgeText: app.queue.count + " iş"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 160
                    contentSpacing: 5

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: Theme.surfaceAlt
                        border.width: 1
                        border.color: Theme.border
                        radius: Theme.controlRadius
                        clip: true

                        Text {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.margins: 9
                            text: "Kuyruk boş · işler burada görünecek"
                            color: Theme.tertiaryText
                            font.pixelSize: 10
                            visible: app.queue.count === 0
                        }
                        ListView {
                            anchors.fill: parent
                            anchors.margins: 3
                            model: app.queue
                            visible: app.queue.count > 0
                            clip: true
                            spacing: 2
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: C.AppScrollBar { }
                            delegate: Rectangle {
                                required property int index
                                required property string inputName
                                required property string outputName
                                required property int status
                                required property string statusText
                                required property real progress
                                required property string stage
                                required property bool canEdit
                                width: ListView.view.width
                                height: 42
                                color: status === 1 ? Theme.accentSurface
                                                    : rowHover.hovered ? Theme.surfaceHover : Theme.surface
                                border.width: 1
                                border.color: status === 3 ? Theme.error : Theme.border
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    anchors.rightMargin: 3
                                    spacing: 5
                                    Rectangle {
                                        Layout.preferredWidth: 5
                                        Layout.preferredHeight: 24
                                        radius: 2
                                        color: status === 2 ? Theme.success
                                             : status === 3 ? Theme.error
                                             : status === 1 ? Theme.accent : Theme.strongBorder
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0
                                        Text {
                                            Layout.fillWidth: true
                                            text: inputName
                                            color: Theme.text
                                            font.pixelSize: 10
                                            font.weight: Font.Medium
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: status === 1 ? stage : outputName + " · " + statusText
                                            color: Theme.tertiaryText
                                            font.pixelSize: 9
                                            elide: Text.ElideRight
                                        }
                                    }
                                    C.IconButton {
                                        glyph: "×"
                                        danger: true
                                        implicitWidth: 24
                                        implicitHeight: 28
                                        enabled: canEdit
                                        onClicked: app.queue.removeJob(index)
                                        ToolTip.visible: hovered
                                        ToolTip.text: "İşi kaldır"
                                    }
                                }
                                HoverHandler { id: rowHover }
                                ProgressBar {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 2
                                    from: 0; to: 1; value: progress
                                    visible: status === 1
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        C.AppButton {
                            Layout.fillWidth: true
                            text: app.queue.running ? "Çalışıyor" : "Başlat"
                            compact: true
                            primary: true
                            enabled: app.queue.count > 0 && !app.queue.running
                            onClicked: app.queue.startQueue()
                        }
                        C.AppButton {
                            Layout.fillWidth: true
                            text: "Tam Kuyruk"
                            compact: true
                            onClicked: page.openQueueRequested()
                        }
                    }
                }
            }
        }
        }

        Rectangle {
            id: actionBar
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: Theme.topBar

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: Theme.border
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: page.outerMargin
                anchors.rightMargin: page.outerMargin
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 7
                    Layout.preferredHeight: 7
                    radius: 4
                    color: composer.validationMessage.length > 0 ? Theme.error : Theme.success
                }
                Text {
                    Layout.fillWidth: true
                    text: composer.validationMessage.length > 0
                          ? composer.validationMessage
                          : composer.summary
                    color: composer.validationMessage.length > 0 ? Theme.error : Theme.secondaryText
                    font.pixelSize: 11
                    elide: Text.ElideRight
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

    Component {
        id: advancedPanelComponent

        Rectangle {
            id: advancedPanel
            radius: 0
            color: "transparent"
            border.width: 0

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 0
            spacing: 5

            C.SegmentedControl {
                id: advancedTabs
                Layout.fillWidth: true
                options: advancedPanel.width < 560
                         ? ["Metadata", "İşlemler", "İmzalama"]
                         : ["Uygulama Bilgileri", "Uygulama İşlemleri", "İmzalama ve Bağımlılıklar"]
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: advancedTabs.currentIndex

                Flickable {
                    id: metadataScroll
                    contentWidth: width
                    contentHeight: metadataContent.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: C.AppScrollBar {
                        policy: metadataScroll.contentHeight > metadataScroll.height + 8
                                ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                    }

                    ColumnLayout {
                        id: metadataContent
                        width: metadataScroll.width - (metadataScroll.contentHeight > metadataScroll.height ? 7 : 0)
                        spacing: 8

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 7

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Text { text: "Uygulama Adı"; color: Theme.secondaryText; font.pixelSize: 10 }
                                C.FormField {
                                    Layout.fillWidth: true
                                    text: composer.appName
                                    onTextEdited: composer.appName = text
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Text { text: "Sürüm"; color: Theme.secondaryText; font.pixelSize: 10 }
                                C.FormField {
                                    Layout.fillWidth: true
                                    text: composer.appVersion
                                    onTextEdited: composer.appVersion = text
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Text { text: "Bundle Identifier"; color: Theme.secondaryText; font.pixelSize: 10 }
                                C.FormField {
                                    Layout.fillWidth: true
                                    text: composer.bundleId
                                    onTextEdited: composer.bundleId = text
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Text { text: "Minimum iOS"; color: Theme.secondaryText; font.pixelSize: 10 }
                                C.FormField {
                                    Layout.fillWidth: true
                                    text: composer.minimumOs
                                    placeholderText: "örn. 15.0"
                                    onTextEdited: composer.minimumOs = text
                                }
                            }
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: advancedPanel.width >= 620 ? 3 : 1
                            columnSpacing: 10
                            rowSpacing: 7
                            C.PathPicker {
                                Layout.fillWidth: true
                                label: "Uygulama İkonu"
                                value: composer.iconPath
                                onValueEdited: function(value) { composer.iconPath = value }
                                onChooseRequested: iconDialog.open()
                            }
                            C.PathPicker {
                                Layout.fillWidth: true
                                label: "Info.plist Merge"
                                value: composer.plistPath
                                onValueEdited: function(value) { composer.plistPath = value }
                                onChooseRequested: plistDialog.open()
                            }
                            C.PathPicker {
                                Layout.fillWidth: true
                                label: "Entitlements Merge"
                                value: composer.entitlementsPath
                                onValueEdited: function(value) { composer.entitlementsPath = value }
                                onChooseRequested: entitlementsDialog.open()
                            }
                        }
                    }
                }

                Flickable {
                    id: operationsScroll
                    contentWidth: width
                    contentHeight: operationsContent.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: C.AppScrollBar {
                        policy: operationsScroll.contentHeight > operationsScroll.height + 8
                                ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                    }

                    GridLayout {
                        id: operationsContent
                        width: operationsScroll.width - (operationsScroll.contentHeight > operationsScroll.height ? 7 : 0)
                        columns: advancedPanel.width >= 620 ? 2 : 1
                        columnSpacing: 18
                        rowSpacing: 4

                        C.ToggleRow {
                            Layout.fillWidth: true
                            label: "Supported Devices listesini kaldır"
                            checked: composer.removeSupportedDevices
                            onToggled: function(v) { composer.removeSupportedDevices = v }
                        }
                        C.ToggleRow {
                            Layout.fillWidth: true
                            label: "Apple Watch içeriğini kaldır"
                            checked: composer.noWatch
                            onToggled: function(v) { composer.noWatch = v }
                        }
                        C.ToggleRow {
                            Layout.fillWidth: true
                            label: "Document Sharing'i etkinleştir"
                            checked: composer.enableDocuments
                            onToggled: function(v) { composer.enableDocuments = v }
                        }
                        C.ToggleRow {
                            Layout.fillWidth: true
                            label: "Ad-hoc / Fake Sign"
                            checked: composer.fakeSign
                            onToggled: function(v) { composer.fakeSign = v }
                        }
                        C.ToggleRow {
                            Layout.fillWidth: true
                            label: "Yalnız arm64 bırak"
                            checked: composer.thin
                            onToggled: function(v) { composer.thin = v }
                        }
                        C.ToggleRow {
                            Layout.fillWidth: true
                            label: "Şifreli ana executable'ı görmezden gel"
                            checked: composer.ignoreEncrypted
                            onToggled: function(v) { composer.ignoreEncrypted = v }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                Layout.preferredWidth: 96
                                text: "Extension işlemi"
                                color: Theme.secondaryText
                                font.pixelSize: 11
                            }
                            C.CompactComboBox {
                                Layout.fillWidth: true
                                implicitHeight: Theme.controlHeight
                                font.pixelSize: 11
                                model: ["Değişiklik yapma", "Tüm App Extension'ları kaldır", "Yalnız şifreli Extension'ları kaldır"]
                                currentIndex: composer.extensionMode
                                onActivated: composer.extensionMode = currentIndex
                            }
                        }
                        C.InlineMessage {
                            Layout.fillWidth: true
                            kind: "warning"
                            text: composer.ignoreEncrypted
                                  ? "Önemli bir şifreleme uyumluluk kontrolü devre dışı."
                                  : ""
                        }
                    }
                }

                Flickable {
                    id: signingScroll
                    contentWidth: width
                    contentHeight: signingContent.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: C.AppScrollBar {
                        policy: signingScroll.contentHeight > signingScroll.height + 8
                                ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                    }

                    GridLayout {
                        id: signingContent
                        width: signingScroll.width - (signingScroll.contentHeight > signingScroll.height ? 7 : 0)
                        columns: advancedPanel.width >= 620 ? 2 : 1
                        columnSpacing: 14
                        rowSpacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text { text: "LDID"; color: Theme.secondaryText; font.pixelSize: 10 }
                            C.CompactComboBox {
                                Layout.fillWidth: true
                                implicitHeight: Theme.controlHeight
                                model: ["Bundled ldid.exe", "Özel ldid.exe"]
                                currentIndex: composer.ldidMode
                                onActivated: composer.ldidMode = currentIndex
                            }
                        }
                        C.PathPicker {
                            Layout.fillWidth: true
                            visible: composer.ldidMode === 1
                            label: "Özel ldid.exe"
                            value: composer.customLdidPath
                            onValueEdited: function(value) { composer.customLdidPath = value }
                            onChooseRequested: ldidDialog.open()
                        }
                        C.PathPicker {
                            Layout.fillWidth: true
                            label: "Dependency Directory"
                            value: composer.dependencyDirectory
                            buttonText: "Klasör Seç…"
                            onValueEdited: function(value) { composer.dependencyDirectory = value }
                            onChooseRequested: dependencyDialog.open()
                        }
                        C.ToggleRow {
                            Layout.fillWidth: true
                            label: "Compatibility Mode"
                            description: "Eski cyan injection semantiğiyle uyumluluk."
                            checked: composer.compatibilityMode
                            onToggled: function(v) { composer.compatibilityMode = v }
                        }
                    }
                }
            }
        }
    }
    }
}
