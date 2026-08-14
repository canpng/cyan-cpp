pragma Singleton
import QtQuick

QtObject {
    property bool dark: false
    readonly property color background: dark ? "#141518" : "#f2f3f7"
    readonly property color sidebar: dark ? "#191a1e" : "#ebecef"
    readonly property color topBar: dark ? "#1b1c20" : "#fbfbfd"
    readonly property color surface: dark ? "#202126" : "#ffffff"
    readonly property color surfaceElevated: dark ? "#2b2d33" : "#ffffff"
    readonly property color surfaceAlt: dark ? "#18191d" : "#f4f5f8"
    readonly property color surfaceHover: dark ? "#2a2c32" : "#e9ebf0"
    readonly property color border: dark ? "#33353c" : "#d8dae0"
    readonly property color strongBorder: dark ? "#484b54" : "#bec2ca"
    readonly property color text: dark ? "#f2f2f7" : "#1c1c1e"
    readonly property color secondaryText: dark ? "#b1b2ba" : "#636366"
    readonly property color tertiaryText: dark ? "#7f818b" : "#8e8e93"
    readonly property color accent: dark ? "#0a84ff" : "#007aff"
    readonly property color accentHover: dark ? "#409cff" : "#1687ff"
    readonly property color accentSurface: dark ? "#153452" : "#e5f1ff"
    readonly property color error: dark ? "#ff7474" : "#c93636"
    readonly property color errorSurface: dark ? "#4a2427" : "#fff0f0"
    readonly property color warning: dark ? "#f1b64c" : "#9a6500"
    readonly property color warningSurface: dark ? "#473819" : "#fff7df"
    readonly property color success: dark ? "#70c692" : "#24864d"
    readonly property int radius: 6
    readonly property int panelRadius: 10
    readonly property int controlRadius: 7
    readonly property int controlHeight: 32
    readonly property int panelPadding: 10
    readonly property int panelSpacing: 7
    readonly property int motionFast: 120
    readonly property int motionNormal: 180
}
