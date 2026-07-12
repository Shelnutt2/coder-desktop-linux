import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

// Queued-message chips shown between the timeline and the composer, each
// with a promote (send now) and a remove action.
ColumnLayout {
    id: list
    // The page's ChatController context object.
    property var chat: null
    spacing: 4
    visible: chat && chat.queuedCount > 0

    Label {
        text: "Queued"
        color: CoderTheme.textDisabled
        font.pixelSize: 10
        font.capitalization: Font.AllUppercase
    }

    Repeater {
        model: list.chat ? list.chat.queuedMessages : []

        Rectangle {
            required property var modelData
            Layout.fillWidth: true
            implicitHeight: qRow.implicitHeight + 12
            radius: CoderTheme.radiusSm
            color: CoderTheme.surface
            border.color: CoderTheme.border
            border.width: 1

            RowLayout {
                id: qRow
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8

                Label {
                    text: modelData.text
                    color: CoderTheme.textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    Layout.fillWidth: true
                }
                Label {
                    text: "Send now"
                    color: CoderTheme.primary
                    font.pixelSize: 11
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -4
                        cursorShape: Qt.PointingHandCursor
                        onClicked: list.chat.promoteQueued(modelData.queuedId)
                    }
                }
                Label {
                    text: "\u00d7"
                    color: CoderTheme.textSecondary
                    font.pixelSize: 13
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -4
                        cursorShape: Qt.PointingHandCursor
                        onClicked: list.chat.deleteQueued(modelData.queuedId)
                    }
                }
            }
        }
    }
}
