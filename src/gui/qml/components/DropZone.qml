import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Cyan.Gui

Rectangle {
    id: zone
    property string title: ""
    property string subtitle: ""
    property string errorText: ""
    property bool compact: false
    signal browseRequested()
    signal filesDropped(var urls)

    implicitHeight: compact ? 58 : 88
    radius: Theme.panelRadius
    color: dropArea.containsDrag || hover.hovered ? Theme.accentSurface : Theme.surfaceAlt
    border.width: 0

    Behavior on color { ColorAnimation { duration: Theme.motionFast } }

    DropArea {
        id: dropArea
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: function(drop) {
            if (drop.hasUrls) {
                zone.filesDropped(drop.urls)
                drop.acceptProposedAction()
            }
        }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: zone.browseRequested() }

    Canvas {
        id: outline
        anchors.fill: parent
        property color lineColor: dropArea.containsDrag ? Theme.accent
                                                        : zone.errorText.length > 0 ? Theme.error
                                                                                  : hover.hovered ? Theme.accent
                                                                                                  : Theme.strongBorder
        onLineColorChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            const inset = 1.5
            const corner = Math.min(10, Math.max(4, zone.radius))
            ctx.reset()
            ctx.strokeStyle = lineColor
            ctx.lineWidth = dropArea.containsDrag ? 2 : 1.4
            ctx.setLineDash([7, 5])
            ctx.beginPath()
            ctx.moveTo(corner, inset)
            ctx.lineTo(width - corner, inset)
            ctx.quadraticCurveTo(width - inset, inset, width - inset, corner)
            ctx.lineTo(width - inset, height - corner)
            ctx.quadraticCurveTo(width - inset, height - inset, width - corner, height - inset)
            ctx.lineTo(corner, height - inset)
            ctx.quadraticCurveTo(inset, height - inset, inset, height - corner)
            ctx.lineTo(inset, corner)
            ctx.quadraticCurveTo(inset, inset, corner, inset)
            ctx.stroke()
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 24, 360)
        spacing: zone.compact ? 1 : 3

        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: zone.compact ? 22 : 28
            Layout.preferredHeight: zone.compact ? 16 : 22
            visible: zone.errorText.length === 0
            transform: Translate {
                y: dropArea.containsDrag ? -4 : hover.hovered ? -2 : 0
                Behavior on y {
                    NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic }
                }
            }

            Canvas {
                id: glyph
                anchors.fill: parent
                property color glyphColor: dropArea.containsDrag || hover.hovered
                                           ? Theme.accent : Theme.secondaryText
                onGlyphColorChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = glyphColor
                    ctx.lineWidth = 1.7
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(width * 0.22, height * 0.62)
                    ctx.lineTo(width * 0.22, height * 0.82)
                    ctx.lineTo(width * 0.78, height * 0.82)
                    ctx.lineTo(width * 0.78, height * 0.62)
                    ctx.moveTo(width * 0.5, height * 0.12)
                    ctx.lineTo(width * 0.5, height * 0.62)
                    ctx.moveTo(width * 0.34, height * 0.46)
                    ctx.lineTo(width * 0.5, height * 0.62)
                    ctx.lineTo(width * 0.66, height * 0.46)
                    ctx.stroke()
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: zone.title
            color: zone.errorText.length > 0 ? Theme.error : Theme.text
            font.pixelSize: zone.compact ? 11 : 12
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            text: zone.errorText.length > 0 ? zone.errorText : zone.subtitle
            color: zone.errorText.length > 0 ? Theme.error : Theme.secondaryText
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            visible: text.length > 0 && !zone.compact
            ToolTip.visible: errorHover.hovered && zone.errorText.length > 0
            ToolTip.text: zone.errorText
            HoverHandler { id: errorHover }
        }
    }
}
