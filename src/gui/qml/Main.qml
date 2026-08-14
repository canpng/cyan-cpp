import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Cyan.Gui
import "components" as C
import "pages" as Pages

ApplicationWindow {
    id: window
    width: 1240
    height: 800
    minimumWidth: 900
    minimumHeight: 700
    visible: true
    title: "cyan"
    color: Theme.background
    property int currentPage: 0
    readonly property bool compactNavigation: width < 1050
    readonly property Item currentNavButton: currentPage === 0 ? newJobNav
                                                  : currentPage === 1 ? queueNav
                                                  : currentPage === 2 ? presetsNav
                                                                      : settingsNav

    SystemPalette { id: systemPalette; colorGroup: SystemPalette.Active }
    Binding {
        target: Theme
        property: "dark"
        value: app.settings.theme === "dark" ||
               (app.settings.theme === "system" && systemPalette.window.hslLightness < 0.5)
    }

    palette.window: Theme.background
    palette.windowText: Theme.text
    palette.base: Theme.surface
    palette.text: Theme.text
    palette.button: Theme.surface
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: "white"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: topBar
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: Theme.topBar

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 6
                spacing: 4

                RowLayout {
                    Layout.preferredWidth: 64
                    spacing: 0
                    Text {
                        text: "cyan"
                        color: Theme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                }

                C.NavButton {
                    id: newJobNav
                    text: "Yeni İş"
                    selected: window.currentPage === 0
                    onClicked: window.currentPage = 0
                }
                C.NavButton {
                    id: queueNav
                    text: window.compactNavigation ? "Kuyruk" : "İşlem Kuyruğu"
                    selected: window.currentPage === 1
                    onClicked: window.currentPage = 1
                }
                C.NavButton {
                    id: presetsNav
                    text: "Önayarlar"
                    selected: window.currentPage === 2
                    onClicked: window.currentPage = 2
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "cyan " + app.cyanVersion
                    color: Theme.tertiaryText
                    font.pixelSize: 11
                    visible: !window.compactNavigation
                }
                C.NavButton {
                    id: settingsNav
                    text: "Ayarlar"
                    selected: window.currentPage === 3
                    onClicked: window.currentPage = 3
                }
            }

            Rectangle {
                z: 3
                y: parent.height - height
                x: window.currentNavButton
                   ? window.currentNavButton.mapToItem(topBar, 0, 0).x + 10 +
                     window.currentNavButton.x * 0
                   : 0
                width: window.currentNavButton ? Math.max(24, window.currentNavButton.width - 20) : 0
                height: 2
                radius: 1
                color: Theme.accent
                Behavior on x {
                    NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic }
                }
                Behavior on width {
                    NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic }
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

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: window.currentPage
            Pages.NewJobPage { onOpenQueueRequested: window.currentPage = 1 }
            Pages.QueuePage { }
            Pages.PresetsPage { onCreateRequested: window.currentPage = 0 }
            Pages.SettingsPage { }
        }
    }

    Rectangle {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        width: Math.min(parent.width - 40, toastText.implicitWidth + 32)
        height: toastText.implicitHeight + 18
        radius: Theme.panelRadius
        color: Theme.dark ? "#35383d" : "#262a30"
        opacity: 0
        visible: opacity > 0
        z: 100

        Text {
            id: toastText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, window.width - 76)
            color: "white"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
        Behavior on opacity { NumberAnimation { duration: 150 } }
        Timer {
            id: toastTimer
            interval: 3200
            onTriggered: toast.opacity = 0
        }
        function show(message) {
            toastText.text = message
            toast.opacity = 1
            toastTimer.restart()
        }
    }

    Connections {
        target: app
        function onNavigateToComposer() { window.currentPage = 0 }
        function onNavigateToQueue() {
            if (window.currentPage !== 0)
                window.currentPage = 1
        }
        function onNotification(message) { toast.show(message) }
    }
}
