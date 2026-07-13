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

    // Content is centered at a readable width when the window is maximized
    // (mirrors the web UI's ~900px chat column).
    readonly property real contentMaxWidth: 900

    // Input fence: this page is loaded on top of the still-live chat list
    // (AgentsPage keeps its ListView underneath). Plain Rectangles do not
    // consume pointer events, so without this any wheel or click not
    // handled by a child of this page would fall through to the hidden
    // chat-list rows and navigate to whatever chat happens to sit under
    // the cursor. Declared first so all page content stacks above it.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onClicked: {}
        onWheel: function(wheel) { wheel.accepted = true }
    }

    // ChatController lives for the lifetime of this page and is recreated
    // when the page navigates to a different chat (sub-agent breadcrumbs).
    property var chat: null
    function reopenChat() {
        if (chat) {
            chat.destroy()
            chat = null
        }
        if (chatId.length > 0) chat = agentsController.openChat(chatId)
        refreshWorkspaceInfo()
    }
    Component.onCompleted: {
        reopenChat()
        syncFocus()
    }
    onChatIdChanged: {
        if (!chat || chat.chatId !== chatId) reopenChat()
        syncFocus()
    }
    Component.onDestruction: {
        if (chat) chat.destroy()
        if (agentsController.focusedChatId === chatId)
            agentsController.focusedChatId = ""
    }

    // Notification focus suppression: report the open chat to the
    // controller only while this page is visible in an active window.
    readonly property bool chatInFocus: visible && Window.active
    onChatInFocusChanged: syncFocus()
    function syncFocus() {
        if (chatInFocus)
            agentsController.focusedChatId = chatId
        else if (agentsController.focusedChatId === chatId)
            agentsController.focusedChatId = ""
    }

    property bool showDiff: false

    // Associated workspace (name + status) resolved from WorkspaceModel.
    // Re-resolved when the chat metadata or the workspace list changes, so
    // the chip fills in even when chats load before workspaces.
    property var workspaceInfo: ({ found: false, name: "", statusString: "Unknown" })
    function refreshWorkspaceInfo() {
        workspaceInfo = workspaceModel.infoForId(chat && chat.workspaceId ? chat.workspaceId : "")
    }
    Connections {
        target: workspaceModel
        function onCountChanged() { page.refreshWorkspaceInfo() }
        function onDataChanged() { page.refreshWorkspaceInfo() }
    }

    // Sub-agent children. Recomputed on chat-list changes, but only
    // reassigned when the result actually differs so unrelated chat
    // upserts do not rebuild the drawer's delegates.
    property var subagents: agentsController.subagentsOf(chatId)
    readonly property int runningSubagentCount: {
        var n = 0
        for (var i = 0; i < subagents.length; ++i) {
            var s = subagents[i].statusString
            if (s === "running" || s === "pending" || s === "waiting") ++n
        }
        return n
    }
    function refreshSubagents() {
        var next = agentsController.subagentsOf(chatId)
        if (JSON.stringify(next) !== JSON.stringify(subagents)) subagents = next
    }
    Connections {
        target: agentsController
        function onChatUpserted(c) { page.refreshSubagents() }
        function onChatsReset(c) { page.refreshSubagents() }
    }

    // Jump to the newest end after the user sends a message.
    Connections {
        target: page.chat
        function onMessageSent() { timeline.positionViewAtBeginning() }
        function onChatInfoChanged() { page.refreshWorkspaceInfo() }
    }

    function statusChipStatus(s) {
        if (s === "running" || s === "pending" || s === "waiting") return "Running"
        if (s === "interrupting") return "Stopping"
        if (s === "error") return "Failed"
        if (s === "requires_action") return "Starting"
        return "Stopped"
    }

    // Vertical contract (see ChatLayoutTest): a fixed compact header, then
    // the timeline as the ONLY Layout.fillHeight item, then a bottom dock
    // whose height is exactly the sum of its visible children. Sub-agents
    // cost zero vertical space here; they live in a header chip that opens
    // a right-edge drawer.
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        spacing: 4
        visible: !page.showDiff

        // ---- Compact header (fixed height, never more than 44px) ----
        Item {
            objectName: "chatHeader"
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Layout.maximumHeight: 44

            RowLayout {
                anchors.fill: parent
                spacing: 6

                ToolButton {
                    implicitWidth: 32
                    implicitHeight: 32
                    padding: 0
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
                    implicitHeight: 28
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

                // Sub-agents chip: the only main-column footprint sub-agents
                // have. Click opens the right-edge drawer. Hidden when the
                // chat has no sub-agents.
                Rectangle {
                    objectName: "subagentChip"
                    visible: page.subagents.length > 0
                    implicitWidth: subChipRow.implicitWidth + 16
                    implicitHeight: 22
                    radius: 11
                    color: CoderTheme.surfaceSecondary
                    border.color: CoderTheme.border
                    border.width: 1

                    RowLayout {
                        id: subChipRow
                        anchors.centerIn: parent
                        spacing: 5
                        Rectangle {
                            width: 7; height: 7; radius: 3.5
                            visible: page.runningSubagentCount > 0
                            color: CoderTheme.warning
                        }
                        Label {
                            text: page.subagents.length + " sub-agent"
                                  + (page.subagents.length === 1 ? "" : "s")
                            color: CoderTheme.textSecondary
                            font.pixelSize: 10
                        }
                    }
                    ToolTip.visible: subChipHover.hovered
                    ToolTip.text: page.runningSubagentCount > 0
                        ? page.runningSubagentCount + " running" : "Show sub-agents"
                    HoverHandler { id: subChipHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        gesturePolicy: TapHandler.DragThreshold
                        onTapped: subagentDrawer.open()
                    }
                }

                StatusChip {
                    visible: page.chat !== null
                    status: page.chat ? page.statusChipStatus(page.chat.statusString) : "Stopped"
                }

                // Associated workspace chip (name + status dot).
                Rectangle {
                    visible: page.chat !== null && page.chat.workspaceId.length > 0
                    implicitWidth: wsChipRow.implicitWidth + 16
                    implicitHeight: 22
                    radius: 11
                    color: CoderTheme.surfaceSecondary
                    border.color: CoderTheme.border
                    border.width: 1

                    RowLayout {
                        id: wsChipRow
                        anchors.centerIn: parent
                        spacing: 5
                        Rectangle {
                            width: 7; height: 7; radius: 3.5
                            color: CoderTheme.statusColor(page.workspaceInfo.statusString)
                        }
                        Label {
                            text: page.workspaceInfo.found ? page.workspaceInfo.name : "workspace"
                            color: CoderTheme.textSecondary
                            font.pixelSize: 10
                            elide: Text.ElideRight
                            Layout.maximumWidth: 140
                        }
                    }
                    ToolTip.visible: wsChipHover.hovered
                    ToolTip.text: "Workspace: " + (page.workspaceInfo.found
                        ? page.workspaceInfo.name + " (" + page.workspaceInfo.statusString + ")"
                        : page.chat ? page.chat.workspaceId : "")
                    HoverHandler { id: wsChipHover }
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
                    implicitWidth: 32
                    implicitHeight: 32
                    padding: 0
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
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: CoderTheme.divider }

        // ---- Timeline (the only Layout.fillHeight item on the page) ----
        Item {
            objectName: "timelineArea"
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: timeline
                objectName: "timeline"
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
                // contentY is tracked continuously while visible (sampling
                // it in onVisibleChanged races the model unbind) and
                // restored after the model rebinds. Saves are frozen from
                // the moment the view hides until the restore completes so
                // unbind/rebind transients cannot clobber the saved value.
                property real savedContentY: 0
                property bool restorePending: false
                model: page.visible && page.chat ? page.chat.messagesModel : null
                onContentYChanged: {
                    if (visible && model && !restorePending) savedContentY = contentY
                }
                onVisibleChanged: {
                    if (!visible) {
                        restorePending = true
                    } else {
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
                // maybeLoadOlder also re-fires after each completed page (and
                // the initial load): when a short page leaves the view still
                // at the oldest end with more history available, the next
                // page is requested automatically; otherwise loadOlder would
                // be unreachable because atYBeginning never toggles.
                function maybeLoadOlder() {
                    if (atYBeginning && page.chat && model
                            && model.hasMore && !model.loadingOlder)
                        page.chat.loadOlder()
                }
                onAtYBeginningChanged: maybeLoadOlder()
                Connections {
                    target: timeline.model
                    function onLoadingOlderChanged() { Qt.callLater(timeline.maybeLoadOlder) }
                    function onHasMoreChanged() { Qt.callLater(timeline.maybeLoadOlder) }
                    function onCountChanged() { Qt.callLater(timeline.maybeLoadOlder) }
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
                        // Centered readable column when the window is wider
                        // than the max content width.
                        width: Math.min(parent.width, page.contentMaxWidth)
                        anchors.horizontalCenter: parent.horizontalCenter
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
                                // ask_user_question tool calls render as an
                                // interactive question card once their args
                                // parse as a valid questions object; until
                                // then (streaming/incomplete) the generic
                                // tool card is used.
                                readonly property var askQuestions:
                                    (ptype === "tool-call" || ptype === "tool-result")
                                    && modelData.toolName === "ask_user_question"
                                    && page.chat
                                        ? page.chat.parseAskUserQuestions(modelData.argsJson)
                                        : []
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
                                            return partItem.askQuestions.length > 0
                                                ? askUserComp : toolComp
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
                                        id: askUserComp
                                        AskUserQuestionCard {
                                            chat: page.chat
                                            questions: partItem.askQuestions
                                            isError: partItem.modelData.isError === true
                                            // A newer user message means the
                                            // questions were already answered
                                            // (here or elsewhere). count in
                                            // the binding re-evaluates it as
                                            // messages arrive.
                                            answered: timeline.model
                                                ? timeline.model.count >= 0
                                                  && timeline.model.hasNewerUserMessage(
                                                         messageRow.index)
                                                : false
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
                        // chat is in plan mode. planMarkdown stays empty while
                        // the message is still streaming so PlanCard's parsed
                        // steps are not recomputed on every delta flush.
                        PlanCard {
                            visible: steps.length > 0
                            Layout.fillWidth: true
                            chat: page.chat
                            planMarkdown: {
                                if (!page.chat || !page.chat.planMode) return ""
                                if (messageRow.model.role !== "assistant"
                                        || messageRow.index !== 0) return ""
                                if (messageRow.model.isStreaming === true) return ""
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

        // ---- Bottom dock ----
        // Height is exactly the sum of its visible children: the status
        // callout (which embeds action-required forms and shows only when
        // an error/retry/action/usage event is active), the queued chips
        // (only when the queue is non-empty), and the composer.
        // Layout.fillHeight is forced off: layouts nested inside layouts
        // otherwise default to filling, which is exactly the bug that let
        // an empty callout swallow half the window.
        ColumnLayout {
            objectName: "bottomDock"
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.maximumWidth: page.contentMaxWidth
            Layout.alignment: Qt.AlignHCenter
            spacing: 4

            ChatStatusCallout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                chat: page.chat
            }
            QueuedMessagesList {
                Layout.fillWidth: true
                Layout.fillHeight: false
                chat: page.chat
            }
            ChatComposer {
                id: composer
                Layout.fillWidth: true
                Layout.fillHeight: false
                chat: page.chat
                disabled: (page.chat && page.chat.archived) || !agentsController.available
            }
        }
    }

    // ---- Sub-agent drawer ----
    // Slide-in panel from the right edge listing this chat's sub-agents.
    // Opened from the header chip; costs zero vertical space in the page
    // column.
    Drawer {
        id: subagentDrawer
        objectName: "subagentDrawer"
        edge: Qt.RightEdge
        width: Math.min(320, page.width * 0.85)
        height: page.height

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Label {
                    text: "Sub-agents (" + page.subagents.length + ")"
                          + (page.runningSubagentCount > 0
                             ? " - " + page.runningSubagentCount + " running" : "")
                    color: CoderTheme.textPrimary
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                ToolButton {
                    implicitWidth: 28
                    implicitHeight: 28
                    padding: 0
                    text: "\u00d7"
                    font.pixelSize: 14
                    onClicked: subagentDrawer.close()
                }
            }

            ListView {
                id: subagentList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds
                model: page.subagents

                delegate: SubagentInlineCard {
                    required property var modelData
                    width: subagentList.width
                    chatId: modelData.id
                    title: modelData.title
                    statusString: modelData.statusString
                    onOpenRequested: function(id) {
                        subagentDrawer.close()
                        page.openChildRequested(id)
                    }
                }

                // Contain wheel events: scroll this list in place and never
                // let the wheel reach anything behind the drawer.
                WheelHandler {
                    onWheel: function(event) {
                        subagentList.contentY = Math.max(0, Math.min(
                            subagentList.contentHeight - subagentList.height,
                            subagentList.contentY - event.angleDelta.y))
                        event.accepted = true
                    }
                }
            }
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
