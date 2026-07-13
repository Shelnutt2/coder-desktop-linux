import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

// Collapsible tool-call card. Shows the tool name plus the model_intent
// subtitle, an animated dot with a live args preview while streaming, and a
// merged monospace result section with its own expand toggle (implemented by
// switching maximumLineCount, never a nested Flickable). Error results tint
// the card. Expansion animation is gated by animateExpansion so the timeline
// can disable it while scrolled away from the newest end.
Rectangle {
    id: card
    property string toolName: ""
    property string modelIntent: ""
    property string argsJson: ""
    property string resultText: ""
    property bool isError: false
    property bool streaming: false
    property real durationMs: -1
    property bool expanded: false
    property bool resultExpanded: false
    property bool animateExpansion: true

    implicitHeight: header.implicitHeight + (expanded ? bodyCol.implicitHeight + 8 : 0) + 16
    radius: CoderTheme.radiusSm
    color: isError ? CoderTheme.errorSurface : CoderTheme.surface
    border.color: isError ? CoderTheme.error : CoderTheme.border
    border.width: 1
    clip: true

    Behavior on implicitHeight {
        enabled: card.animateExpansion
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }

    RowLayout {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        spacing: 6

        Label {
            text: card.expanded ? "\u25be" : "\u25b8"
            color: CoderTheme.textSecondary
            font.pixelSize: 12
        }
        Label {
            text: "\u2699"
            color: card.isError ? CoderTheme.error : CoderTheme.textSecondary
            font.pixelSize: 12
        }
        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true
            Label {
                text: card.toolName || "tool"
                color: card.isError ? CoderTheme.error : CoderTheme.textPrimary
                font.pixelSize: 12
                font.weight: Font.Medium
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                visible: card.modelIntent.length > 0
                text: card.modelIntent
                color: CoderTheme.textSecondary
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
        // Animated activity dot while the call streams.
        Rectangle {
            visible: card.streaming
            width: 6; height: 6; radius: 3
            color: CoderTheme.warning
            SequentialAnimation on opacity {
                running: card.streaming
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.2; duration: 500 }
                NumberAnimation { from: 0.2; to: 1.0; duration: 500 }
            }
        }
        Label {
            visible: !card.streaming && card.durationMs >= 0
            text: (card.durationMs / 1000).toFixed(1) + "s"
            color: CoderTheme.textDisabled
            font.pixelSize: 11
        }
    }

    // Live one-line args preview while streaming and collapsed.
    Label {
        visible: card.streaming && !card.expanded && card.argsJson.length > 0
        anchors.left: header.left
        anchors.right: header.right
        anchors.top: header.bottom
        text: card.argsJson
        color: CoderTheme.textDisabled
        font.family: "monospace"
        font.pixelSize: 10
        elide: Text.ElideRight
        maximumLineCount: 1
    }

    ColumnLayout {
        id: bodyCol
        visible: card.expanded
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        anchors.topMargin: 4
        spacing: 6

        Label {
            visible: card.argsJson.length > 0
            text: card.argsJson
            wrapMode: Text.WrapAnywhere
            color: CoderTheme.textSecondary
            font.family: "monospace"
            font.pixelSize: 11
            Layout.fillWidth: true
        }
        Rectangle {
            visible: card.resultText.length > 0
            Layout.fillWidth: true
            implicitHeight: resultCol.implicitHeight + 12
            radius: CoderTheme.radiusSm
            color: CoderTheme.background

            ColumnLayout {
                id: resultCol
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4

                Label {
                    id: resultLabel
                    text: card.resultText
                    wrapMode: Text.WrapAnywhere
                    color: card.isError ? CoderTheme.error : CoderTheme.textSecondary
                    font.family: "monospace"
                    font.pixelSize: 11
                    // The result body caps its height via maximumLineCount;
                    // "Show more" lifts the cap in place instead of nesting
                    // a Flickable inside the timeline.
                    maximumLineCount: card.resultExpanded ? 100000 : 8
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    visible: resultLabel.truncated || card.resultExpanded
                    text: card.resultExpanded ? "Show less" : "Show more"
                    color: CoderTheme.primary
                    font.pixelSize: 11
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: card.resultExpanded = !card.resultExpanded
                    }
                }
            }
        }
    }

    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: header.implicitHeight + 16
        onClicked: card.expanded = !card.expanded
    }
}
