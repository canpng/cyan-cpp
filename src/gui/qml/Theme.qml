pragma Singleton
import QtQuick

QtObject {
    property bool dark: false
    readonly property color background: dark ? "#18191b" : "#f4f5f7"
    readonly property color sidebar: dark ? "#1e2023" : "#eceef1"
    readonly property color surface: dark ? "#24262a" : "#ffffff"
    readonly property color surfaceAlt: dark ? "#2b2e33" : "#f7f8fa"
    readonly property color border: dark ? "#383b41" : "#dfe2e7"
    readonly property color text: dark ? "#f2f3f5" : "#20242a"
    readonly property color secondaryText: dark ? "#aeb3bc" : "#68707c"
    readonly property color tertiaryText: dark ? "#858b95" : "#8b929d"
    readonly property color accent: dark ? "#62a8ff" : "#1677e8"
    readonly property color accentHover: dark ? "#78b5ff" : "#0d68d2"
    readonly property color accentSurface: dark ? "#173a62" : "#e8f2ff"
    readonly property color error: dark ? "#ff7474" : "#c93636"
    readonly property color errorSurface: dark ? "#4a2427" : "#fff0f0"
    readonly property color warning: dark ? "#f1b64c" : "#9a6500"
    readonly property color warningSurface: dark ? "#473819" : "#fff7df"
    readonly property color success: dark ? "#70c692" : "#24864d"
    readonly property int radius: 14
}
