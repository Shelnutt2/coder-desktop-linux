import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

// Inline card for a sub-agent chat: title plus live status dot. Clicking
// navigates into the child chat (the chat page shows a breadcrumb back to
// the parent).
Rectangle {
    id: card
    property string chatId: ""
    property string title: ""
    property string statusString: ""
    signal openRequested(string chatId)

    implicitHeight: row.implicitHeight + 16
    radius: CoderTheme.radiusSm
    color: ma.containsMouse ? CoderTheme.hoverBg : CoderTheme.surface
    border.color: CoderTheme.border
    border.width: 1

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 8

        Rectangle {
            width: 8; height: 8; radius: 4
            color: {
                var s = card.statusString
                if (s === "running" || s === "pending") return CoderTheme.warning
                if (s === "completed") return CoderTheme.success
                if (s === "error") return CoderTheme.error
                if (s === "requires_action") return CoderTheme.warning
                return CoderTheme.textDisabled
            }
        }
        Label {
            text: "\u2937"
            color: CoderTheme.textDisabled
            font.pixelSize: 11
        }
        Label {
            text: card.title.length > 0 ? card.title : "Sub-agent"
            color: CoderTheme.textPrimary
            font.pixelSize: 12
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label {
            text: card.statusString
            color: CoderTheme.textSecondary
            font.pixelSize: 10
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: card.openRequested(card.chatId)
    }
}
