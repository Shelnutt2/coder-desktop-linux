import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

// Chat detail page for one Coder Agents chat.
//
// Owns a ChatController (created from agentsController.openChat) for the
// lifetime of the page. The timeline is a BottomToTop ListView bound to the
// controller's messages model (index 0 = newest).
//
// BottomToTop atY* semantics (verified empirically with an offscreen
// harness, Qt 6.10): with verticalLayoutDirection: ListView.BottomToTop,
// atYEnd is true when the view shows index 0 (the NEWEST row, visual
// bottom) and atYBeginning is true at the OLDEST end (highest index,
// visual top). positionViewAtBeginning() positions at the newest row.
Rectangle {
    id: page
    property string chatId: ""
    signal backClicked()
    signal openChildRequested(string chatId)

    color: CoderTheme.background

    // ChatController lives for the lifetime of this page and is recreated
    // when the page navigates to a different chat (sub-agent breadcrumbs).
    property var chat: null
    function reopenChat() {
        if (chat) {
            chat.destroy()
            chat = null
        }
        if (chatId.length > 0) chat = agentsController.openChat(chatId)
    }
    Component.onCompleted: reopenChat()
    onChatIdChanged: if (!chat || chat.chatId !== chatId) reopenChat()
    Component.onDestruction: if (chat) chat.destroy()

    property bool showDiff: false

    // Sub-agent children, recomputed on chat-list changes.
    property var subagents: agentsController.subagentsOf(chatId)
    Connections {
        target: agentsController
        function onChatUpserted(c) { page.subagents = agentsController.subagentsOf(page.chatId) }
        function onChatsReset(c) { page.subagents = agentsController.subagentsOf(page.chatId) }
    }

    // Jump to the newest end after the user sends a message.
    Connections {
        target: page.chat
        function onMessageSent() { timeline.positionViewAtBeginning() }
    }

    function statusChipStatus(s) {
        if (s === "running" || s === "pending" || s === "waiting") return "Running"
        if (s === "interrupting") return "Stopping"
        if (s === "error") return "Failed"
        if (s === "requires_action") return "Starting"
        return "Stopped"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        visible: !page.showDiff

        // ---- Top bar ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            ToolButton {
                text: "\u2190"
                font.pixelSize: 16
                // Breadcrumb: sub-agent chats navigate back to their parent.
                ToolTip.visible: hovered && page.chat && page.chat.parentChatId.length > 0
                ToolTip.text: "Back to parent chat"
                onClicked: page.backClicked()
            }

            // Inline-editable title.
            Item {
                Layout.fillWidth: true
                implicitHeight: 32
                Label {
                    id: titleLabel
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    visible: !titleEdit.visible
                    text: page.chat && page.chat.title.length > 0 ? page.chat.title : "Agent chat"
                    color: CoderTheme.textPrimary
                    font.pixelSize: 14
                    font.bold: true
                    elide: Text.ElideRight
                    MouseArea {
                        anchors.fill: parent
                        onDoubleClicked: {
                            titleEdit.text = page.chat.title
                            titleEdit.visible = true
                            titleEdit.forceActiveFocus()
                        }
                    }
                }
                TextField {
                    id: titleEdit
                    anchors.fill: parent
                    visible: false
                    font.pixelSize: 13
                    onAccepted: {
                        if (text.trim().length > 0) page.chat.rename(text.trim())
                        visible = false
                    }
                    Keys.onEscapePressed: visible = false
                    onActiveFocusChanged: if (!activeFocus) visible = false
                }
            }

            StatusChip {
                visible: page.chat !== null
                status: page.chat ? page.statusChipStatus(page.chat.statusString) : "Stopped"
            }

            // Connection pill; click reconnects when degraded.
            Rectangle {
                // ChatStreamWebSocket::ConnectionState: 0 Disconnected,
                // 1 Connecting, 2 Open, 3 Reconnecting, 4 Failed.
                readonly property int conn: page.chat ? page.chat.connectionState : 0
                visible: conn !== 2 && page.chat !== null
                implicitWidth: connRow.implicitWidth + 12
                implicitHeight: 22
                radius: 11
                color: conn === 4 ? CoderTheme.errorSurface : CoderTheme.warningSurface
                border.color: conn === 4 ? CoderTheme.error : CoderTheme.warning
                border.width: 1

                RowLayout {
                    id: connRow
                    anchors.centerIn: parent
                    spacing: 4
                    Label {
                        text: {
                            var c = parent.parent.conn
                            if (c === 1) return "Connecting\u2026"
                            if (c === 3) return "Reconnecting\u2026"
                            if (c === 4) return "Offline \u21bb"
                            return "Offline"
                        }
                        color: parent.parent.conn === 4 ? CoderTheme.error : CoderTheme.warning
                        font.pixelSize: 10
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: page.chat.reconnect()
                }
            }

            ToolButton {
                text: "\u22ee"
                font.pixelSize: 14
                onClicked: overflowMenu.open()
                Menu {
                    id: overflowMenu
                    MenuItem {
                        text: "View changes"
                        onTriggered: page.showDiff = true
                    }
                    MenuItem {
                        text: page.chat && page.chat.archived ? "Unarchive" : "Archive"
                        onTriggered: page.chat.setArchived(!page.chat.archived)
                    }
                    MenuItem {
                        text: "Plan history"
                        visible: page.chat && page.chat.planMode
                        height: visible ? implicitHeight : 0
                        onTriggered: planHistoryPopup.open()
                    }
                    MenuItem {
                        text: "Regenerate title"
                        onTriggered: page.chat.regenerateTitle()
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: CoderTheme.divider }

        // ---- Timeline ----
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: timeline
                anchors.fill: parent
                clip: true
                spacing: 10
                // Index 0 = newest; BottomToTop pins the viewport to the
                // newest end while the user is at the bottom and keeps the
                // viewport stable when older pages are prepended at the top.
                verticalLayoutDirection: ListView.BottomToTop
                // Reuse breaks variable-height mixed delegates; keep off.
                reuseItems: false
                cacheBuffer: 800
                boundsBehavior: Flickable.StopAtBounds

                // Rebind on visibility per the repo's tab-switch workaround;
                // save/restore contentY so the position survives.
                property real savedContentY: 0
                property bool restorePending: false
                model: page.visible && page.chat ? page.chat.messagesModel : null
                onVisibleChanged: {
                    if (!visible) {
                        savedContentY = contentY
                    } else {
                        restorePending = true
                        Qt.callLater(function() {
                            if (timeline.restorePending) {
                                timeline.contentY = timeline.savedContentY
                                timeline.restorePending = false
                            }
                        })
                    }
                }

                // Trigger older-page loads near the OLDEST end
                // (atYBeginning with BottomToTop, verified above). The model
                // guards against re-entry with hasMore + an in-flight flag.
                onAtYBeginningChanged: {
                    if (atYBeginning && page.chat && model) page.chat.loadOlder()
                }

                header: Item {
                    // Header sits at the newest (bottom) edge; keep a small gap.
                    width: timeline.width
                    height: 4
                }
                footer: Item {
                    width: timeline.width
                    height: loadingOlder.visible ? 32 : 4
                    BusyIndicator {
                        id: loadingOlder
                        anchors.centerIn: parent
                        implicitWidth: 20
                        implicitHeight: 20
                        visible: page.chat && page.chat.messagesModel
                                 && page.chat.messagesModel.loadingOlder === true
                        running: visible
                    }
                }

                delegate: Item {
                    id: messageRow
                    required property int index
                    required property var model
                    width: timeline.width
                    height: rowContent.implicitHeight

                    readonly property bool isUser: model.role === "user"

                    ColumnLayout {
                        id: rowContent
                        width: parent.width
                        spacing: 4

                        // Per-part rendering; user text collapses into a
                        // right-aligned bubble, assistant parts render
                        // full-width in order.
                        Repeater {
                            model: messageRow.model.parts
                            delegate: Item {
                                id: partItem
                                required property var modelData
                                readonly property string ptype: modelData.type
                                Layout.fillWidth: true
                                implicitHeight: partLoader.item ? partLoader.item.implicitHeight : 0

                                Loader {
                                    id: partLoader
                                    width: parent.width
                                    sourceComponent: {
                                        if (partItem.ptype === "text")
                                            return messageRow.isUser ? userBubble : assistantText
                                        if (partItem.ptype === "reasoning") return reasoningComp
                                        if (partItem.ptype === "tool-call"
                                                || partItem.ptype === "tool-result")
                                            return toolComp
                                        if (partItem.ptype === "source") return sourceComp
                                        if (partItem.ptype === "file"
                                                || partItem.ptype === "image"
                                                || partItem.ptype === "file-reference"
                                                || partItem.ptype === "context-file")
                                            return fileComp
                                        if (partItem.ptype === "skill") return skillComp
                                        return unknownComp
                                    }

                                    Component {
                                        id: userBubble
                                        Item {
                                            implicitHeight: bubble.implicitHeight
                                            Rectangle {
                                                id: bubble
                                                anchors.right: parent.right
                                                width: Math.min(
                                                    bubbleText.implicitWidth + 20,
                                                    partItem.width * 0.85)
                                                implicitHeight: bubbleText.implicitHeight + 16
                                                radius: CoderTheme.radius
                                                color: CoderTheme.activeSurface
                                                MarkdownText {
                                                    id: bubbleText
                                                    anchors.fill: parent
                                                    anchors.margins: 8
                                                    markdown: partItem.modelData.text
                                                }
                                            }
                                        }
                                    }
                                    Component {
                                        id: assistantText
                                        MarkdownText { markdown: partItem.modelData.text }
                                    }
                                    Component {
                                        id: reasoningComp
                                        ReasoningBlock {
                                            text: partItem.modelData.text
                                            streaming: messageRow.model.isStreaming === true
                                            durationMs: partItem.modelData.durationMs
                                            // Height animations off while the
                                            // user is scrolled away from the
                                            // newest end (atYEnd = newest).
                                            animateExpansion: timeline.atYEnd
                                        }
                                    }
                                    Component {
                                        id: toolComp
                                        ToolCallCard {
                                            toolName: partItem.modelData.toolName
                                            modelIntent: partItem.modelData.modelIntent
                                            argsJson: partItem.modelData.argsJson
                                            resultText: partItem.modelData.resultText
                                            isError: partItem.modelData.isError === true
                                            streaming: messageRow.model.isStreaming === true
                                                       && partItem.modelData.resultText.length === 0
                                            durationMs: partItem.modelData.durationMs
                                            animateExpansion: timeline.atYEnd
                                        }
                                    }
                                    Component {
                                        id: sourceComp
                                        SourceCitationRow {
                                            title: partItem.modelData.title
                                            url: partItem.modelData.url
                                        }
                                    }
                                    Component {
                                        id: fileComp
                                        Item {
                                            implicitHeight: fchip.implicitHeight
                                            FileChip {
                                                id: fchip
                                                anchors.right: messageRow.isUser
                                                    ? parent.right : undefined
                                                name: partItem.modelData.title.length > 0
                                                    ? partItem.modelData.title
                                                    : (partItem.modelData.fileId || "file")
                                            }
                                        }
                                    }
                                    Component {
                                        id: skillComp
                                        Item {
                                            implicitHeight: schip.implicitHeight
                                            SkillChip {
                                                id: schip
                                                name: partItem.modelData.skillName
                                            }
                                        }
                                    }
                                    Component {
                                        id: unknownComp
                                        Item { implicitHeight: 0 }
                                    }
                                }
                            }
                        }

                        // Plan card on the newest assistant message while the
                        // chat is in plan mode.
                        PlanCard {
                            visible: page.chat && page.chat.planMode
                                     && messageRow.model.role === "assistant"
                                     && messageRow.index === 0
                                     && messageRow.model.isStreaming !== true
                            Layout.fillWidth: true
                            chat: page.chat
                            planMarkdown: {
                                var parts = messageRow.model.parts
                                for (var i = parts.length - 1; i >= 0; --i)
                                    if (parts[i].type === "text") return parts[i].text
                                return ""
                            }
                            onKeepPlanningRequested: composer.forceActiveFocus()
                        }

                        // Typing indicator on the streaming tail row.
                        StreamingIndicator {
                            visible: messageRow.model.isStreaming === true
                            Layout.topMargin: 2
                        }
                    }
                }
            }

            // Scroll-to-newest floating button. With BottomToTop, atYEnd is
            // the newest end (verified), so show while NOT at the newest end.
            RoundButton {
                visible: !timeline.atYEnd && timeline.count > 0
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                width: 36; height: 36
                text: "\u2193"
                font.pixelSize: 15
                Material.background: CoderTheme.surfaceSecondary
                onClicked: timeline.positionViewAtBeginning()
            }

            // Empty state.
            Label {
                anchors.centerIn: parent
                visible: timeline.count === 0
                text: "No messages yet"
                color: CoderTheme.textDisabled
                font.pixelSize: 13
            }
        }

        // ---- Sub-agent cards ----
        ColumnLayout {
            visible: page.subagents.length > 0
            Layout.fillWidth: true
            spacing: 4
            Repeater {
                model: page.subagents
                SubagentInlineCard {
                    required property var modelData
                    Layout.fillWidth: true
                    chatId: modelData.id
                    title: modelData.title
                    statusString: modelData.statusString
                    onOpenRequested: function(id) { page.openChildRequested(id) }
                }
            }
        }

        // ---- Action-required forms ----
        Repeater {
            model: page.chat && page.chat.hasActionRequired ? page.chat.actionToolCalls : []
            ActionRequiredForm {
                required property var modelData
                Layout.fillWidth: true
                chat: page.chat
                toolCallId: modelData.toolCallId
                toolName: modelData.toolName
                argsJson: modelData.argsJson
            }
        }

        // ---- Status callouts / queue / composer ----
        ChatStatusCallout {
            Layout.fillWidth: true
            chat: page.chat
        }
        QueuedMessagesList {
            Layout.fillWidth: true
            chat: page.chat
        }
        ChatComposer {
            id: composer
            Layout.fillWidth: true
            chat: page.chat
            disabled: (page.chat && page.chat.archived) || !agentsController.available
        }
    }

    // ---- Diff viewer ----
    Loader {
        anchors.fill: parent
        active: page.showDiff
        sourceComponent: ChatDiffPage {
            chat: page.chat
            onBackClicked: page.showDiff = false
        }
    }

    // ---- Plan history (latest plan snapshot) ----
    Popup {
        id: planHistoryPopup
        anchors.centerIn: parent
        width: parent.width - 48
        height: Math.min(parent.height - 96, 420)
        modal: true

        contentItem: ColumnLayout {
            spacing: 8
            Label {
                text: "Plan history"
                font.bold: true
                color: CoderTheme.textPrimary
            }
            Label {
                text: "The current plan is shown inline on the newest assistant message."
                wrapMode: Text.Wrap
                color: CoderTheme.textSecondary
                font.pixelSize: 12
                Layout.fillWidth: true
            }
            Item { Layout.fillHeight: true }
            CoderButton {
                text: "Close"
                variant: "outline"
                Layout.alignment: Qt.AlignRight
                onClicked: planHistoryPopup.close()
            }
        }
    }
}
