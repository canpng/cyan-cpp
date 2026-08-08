import QtQuick
import QtQuick.Layouts
import Cyan.Gui

ColumnLayout {
    id: picker
    property string label: ""
    property string value: ""
    property string placeholder: "Seçilmedi"
    property string buttonText: "Seç…"
    signal valueEdited(string value)
    signal chooseRequested()
    spacing: 6

    Text {
        text: picker.label
        color: Theme.secondaryText
        font.pixelSize: 12
        font.weight: Font.Medium
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        FormField {
            Layout.fillWidth: true
            text: picker.value
            placeholderText: picker.placeholder
            onTextEdited: picker.valueEdited(text)
        }
        AppButton {
            text: picker.buttonText
            compact: true
            onClicked: picker.chooseRequested()
        }
    }
}
