import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

// Compact row for one sub-agent chat inside the chat page's collapsible
// sub-agent strip: status dot, title, status text, and a chevron. Clicking
// navigates into the child chat. Navigation uses a TapHandler with the
// DragThreshold gesture policy so only a real click activates it; drag or
// flick scroll gestures over the row are cancelled and never navigate.
Rectangle {
    id: card
    property string chatId: ""
    property string title: ""
    property string statusString: ""
    signal openRequested(string chatId)

    implicitHeight: 28
    radius: CoderTheme.radiusSm
    color: hover.hovered ? CoderTheme.hoverBg : "transparent"

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 8
        anchors.rightMargin: 8
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
        Image {
            source: "qrc:/CoderDesktop/assets/icons/subagent.svg"
            sourceSize.width: 12
            sourceSize.height: 12
            Layout.preferredWidth: 12
            Layout.preferredHeight: 12
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
        Label {
            text: "\u203a"
            color: CoderTheme.textDisabled
            font.pixelSize: 12
        }
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        gesturePolicy: TapHandler.DragThreshold
        onTapped: card.openRequested(card.chatId)
    }
}
