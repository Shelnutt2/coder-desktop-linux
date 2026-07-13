import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

// Collapsible "Thinking" block for reasoning parts: chevron header with a
// pulse animation while streaming and a duration label once complete.
// Expansion animation is gated by animateExpansion so the chat timeline can
// disable it while the user is scrolled away from the newest end.
Rectangle {
    id: block
    property string text: ""
    property bool streaming: false
    property real durationMs: -1
    property bool expanded: false
    property bool animateExpansion: true

    implicitHeight: headerRow.implicitHeight + (expanded ? bodyText.implicitHeight + 16 : 0) + 16
    radius: CoderTheme.radiusSm
    color: CoderTheme.surface
    border.color: CoderTheme.border
    border.width: 1
    clip: true

    Behavior on implicitHeight {
        enabled: block.animateExpansion
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }

    RowLayout {
        id: headerRow
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        spacing: 6

        Label {
            text: block.expanded ? "\u25be" : "\u25b8"
            color: CoderTheme.textSecondary
            font.pixelSize: 12
        }
        Label {
            id: thinkingLabel
            text: block.streaming ? "Thinking\u2026" : "Thinking"
            color: CoderTheme.textSecondary
            font.pixelSize: 12
            font.italic: true

            // Gentle pulse while the reasoning is still streaming in.
            SequentialAnimation on opacity {
                running: block.streaming
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.4; duration: 700 }
                NumberAnimation { from: 0.4; to: 1.0; duration: 700 }
            }
            onVisibleChanged: opacity = 1
        }
        Label {
            visible: !block.streaming && block.durationMs >= 0
            text: (block.durationMs / 1000).toFixed(1) + "s"
            color: CoderTheme.textDisabled
            font.pixelSize: 11
        }
        Item { Layout.fillWidth: true }
    }

    Label {
        id: bodyText
        visible: block.expanded
        anchors.top: headerRow.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        text: block.text
        wrapMode: Text.Wrap
        color: CoderTheme.textSecondary
        font.pixelSize: 12
    }

    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: headerRow.implicitHeight + 16
        onClicked: block.expanded = !block.expanded
    }
}
