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
    minimumHeight: 640
    visible: true
    title: "cyan"
    color: Theme.background
    property int currentPage: 0

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

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 198
            Layout.fillHeight: true
            color: Theme.sidebar
            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 5

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 7
                    Layout.rightMargin: 7
                    Layout.topMargin: 8
                    Layout.bottomMargin: 18
                    spacing: 10
                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: 9
                        color: Theme.accent
                        Text {
                            anchors.centerIn: parent
                            text: "C"
                            color: "white"
                            font.pixelSize: 17
                            font.weight: Font.Bold
                        }
                    }
                    Text {
                        text: "cyan"
                        color: Theme.text
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                    }
                    Item { Layout.fillWidth: true }
                }

                C.SidebarItem {
                    Layout.fillWidth: true
                    iconText: "＋"
                    text: "Yeni İş"
                    selected: window.currentPage === 0
                    onClicked: window.currentPage = 0
                }
                C.SidebarItem {
                    Layout.fillWidth: true
                    iconText: "☷"
                    text: "İşlem Kuyruğu"
                    selected: window.currentPage === 1
                    onClicked: window.currentPage = 1
                }
                C.SidebarItem {
                    Layout.fillWidth: true
                    iconText: "◇"
                    text: "Önayarlar"
                    selected: window.currentPage === 2
                    onClicked: window.currentPage = 2
                }
                C.SidebarItem {
                    Layout.fillWidth: true
                    iconText: "⚙"
                    text: "Ayarlar"
                    selected: window.currentPage === 3
                    onClicked: window.currentPage = 3
                }
                Item { Layout.fillHeight: true }
                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.bottomMargin: 6
                    text: "cyan " + app.cyanVersion
                    color: Theme.tertiaryText
                    font.pixelSize: 11
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: window.currentPage
            Pages.NewJobPage { }
            Pages.QueuePage { }
            Pages.PresetsPage { }
            Pages.SettingsPage { }
        }
    }

    Rectangle {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        width: Math.min(parent.width - 48, toastText.implicitWidth + 36)
        height: toastText.implicitHeight + 22
        radius: 10
        color: Theme.dark ? "#35383d" : "#262a30"
        opacity: 0
        visible: opacity > 0
        z: 100
        Text {
            id: toastText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, window.width - 84)
            color: "white"
            font.pixelSize: 13
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
        function onNavigateToQueue() { window.currentPage = 1 }
        function onNotification(message) { toast.show(message) }
    }
}
