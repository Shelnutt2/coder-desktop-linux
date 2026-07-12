import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import CoderDesktop

// Chat composer: autosizing multiline editor (capped at ~40% of the window
// height, internally scrollable beyond that), attachment chips, per-message
// override chips (model, MCP, plan mode, busy behavior), and a send button
// that morphs into Stop while the agent is running.
//
// Key handling honors agentsController.sendShortcut:
//  - "enter": Enter sends, Shift+Enter inserts a newline.
//  - "modifier_enter": Ctrl+Enter sends, Enter inserts a newline.
// Esc interrupts a running agent. Up/Down in an empty composer cycles the
// prompt history. The composer is never disabled while the agent works
// (messages queue); only archived or unavailable chats disable it.
ColumnLayout {
    id: composer
    // The page's ChatController context object.
    property var chat: null
    property bool disabled: false

    // Attachment chips: {localPath, name, fileId, uploading, failed, mediaType}
    property var attachments: []
    property string busyBehavior: "queue"
    property string modelOverrideId: ""
    property var mcpOverrideIds: []
    property int historyIndex: -1

    readonly property bool agentBusy: chat
        && (chat.statusString === "running" || chat.statusString === "pending"
            || chat.statusString === "waiting" || chat.statusString === "interrupting")
    readonly property bool uploadsPending: {
        for (var i = 0; i < attachments.length; ++i)
            if (attachments[i].uploading) return true
        return false
    }
    readonly property bool canSend: !disabled && !uploadsPending
        && (input.text.trim().length > 0 || attachments.length > 0)

    spacing: 6

    // Drafts: the editor text is deliberately NOT bound to chat.draft. A
    // binding would be severed by the first keystroke, and the stale editor
    // text would then leak the previous chat's draft into the next chat
    // when the page swaps its ChatController. Instead the text is assigned
    // explicitly whenever the chat changes. The outgoing chat's draft needs
    // no save here: every keystroke already persisted it live through
    // onTextChanged while that chat was current.
    onChatChanged: {
        historyIndex = -1
        input.text = chat ? chat.draft : ""
    }
    Component.onCompleted: input.text = chat ? chat.draft : ""

    function sendNow() {
        if (!chat || !canSend) return
        var files = []
        for (var i = 0; i < attachments.length; ++i) {
            var a = attachments[i]
            if (!a.failed && a.fileId)
                files.push({ fileId: a.fileId, name: a.name, mediaType: a.mediaType })
        }
        var options = {}
        if (modelOverrideId.length > 0) options.modelConfigId = modelOverrideId
        if (mcpOverrideIds.length > 0) options.mcpServerIds = mcpOverrideIds
        chat.sendMessageWithOptions(input.text, files, busyBehavior, options)
        input.text = ""
        chat.draft = ""
        attachments = []
        historyIndex = -1
    }

    function addAttachment(fileUrl) {
        var path = fileUrl.toString().replace(/^file:\/\//, "")
        var name = path.split("/").pop()
        var list = attachments.slice()
        list.push({ localPath: path, name: name, fileId: "", uploading: true,
                    failed: false, mediaType: "" })
        attachments = list
        agentsController.uploadAttachment(fileUrl)
    }

    Connections {
        target: agentsController
        function onAttachmentUploaded(localPath, fileId, name, mediaType) {
            var list = composer.attachments.slice()
            for (var i = 0; i < list.length; ++i) {
                if (list[i].localPath === localPath && list[i].uploading) {
                    list[i].uploading = false
                    list[i].fileId = fileId
                    list[i].mediaType = mediaType
                    break
                }
            }
            composer.attachments = list
        }
        function onAttachmentUploadFailed(localPath, error) {
            var list = composer.attachments.slice()
            for (var i = 0; i < list.length; ++i) {
                if (list[i].localPath === localPath && list[i].uploading) {
                    list[i].uploading = false
                    list[i].failed = true
                    break
                }
            }
            composer.attachments = list
        }
    }

    FileDialog {
        id: fileDialog
        title: "Attach file"
        onAccepted: composer.addAttachment(selectedFile)
    }

    // ---- Attachment chips ----
    Flow {
        Layout.fillWidth: true
        spacing: 4
        visible: composer.attachments.length > 0

        Repeater {
            model: composer.attachments
            FileChip {
                required property var modelData
                required property int index
                name: modelData.name
                uploading: modelData.uploading
                failed: modelData.failed
                removable: true
                onRemoveClicked: {
                    var list = composer.attachments.slice()
                    list.splice(index, 1)
                    composer.attachments = list
                }
            }
        }
    }

    // ---- Editor ----
    Rectangle {
        Layout.fillWidth: true
        // Autosize with the text up to ~40% of the window; beyond that the
        // inner Flickable scrolls.
        readonly property real maxEditorHeight: (Window.window ? Window.window.height : 700) * 0.4
        implicitHeight: Math.min(input.implicitHeight + 16, maxEditorHeight)
        radius: CoderTheme.radius
        color: CoderTheme.surface
        border.color: input.activeFocus ? CoderTheme.primary : CoderTheme.border
        border.width: 1

        Flickable {
            id: editorFlick
            anchors.fill: parent
            anchors.margins: 4
            contentWidth: width
            contentHeight: input.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            // Consume wheel events so scrolling the composer never scrolls
            // the chat timeline behind it.
            WheelHandler {
                onWheel: function(event) {
                    editorFlick.contentY = Math.max(0, Math.min(
                        editorFlick.contentHeight - editorFlick.height,
                        editorFlick.contentY - event.angleDelta.y))
                    event.accepted = true
                }
            }

            TextArea.flickable: TextArea {
                id: input
                placeholderText: composer.disabled
                    ? "This chat is read-only"
                    : "Message the agent\u2026"
                wrapMode: TextArea.Wrap
                enabled: !composer.disabled
                color: CoderTheme.textPrimary
                placeholderTextColor: CoderTheme.textDisabled
                font.pixelSize: 13
                background: null
                onTextChanged: if (composer.chat && composer.chat.draft !== text)
                                   composer.chat.draft = text

                Keys.onPressed: function(event) {
                    // Esc interrupts a running agent.
                    if (event.key === Qt.Key_Escape && composer.agentBusy) {
                        composer.chat.interrupt()
                        event.accepted = true
                        return
                    }
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        var modifier = (event.modifiers & Qt.ControlModifier)
                        var shift = (event.modifiers & Qt.ShiftModifier)
                        var sendOnPlainEnter = agentsController.sendShortcut !== "modifier_enter"
                        var shouldSend = sendOnPlainEnter ? !shift : modifier
                        if (shouldSend) {
                            composer.sendNow()
                            event.accepted = true
                            return
                        }
                        // Fall through: newline.
                    }
                    // Up/Down in an empty composer cycles prompt history
                    // (newest first, as returned by GET /prompts).
                    if ((event.key === Qt.Key_Up || event.key === Qt.Key_Down)
                            && composer.chat && composer.chat.prompts.length > 0
                            && (input.text.length === 0 || composer.historyIndex >= 0)) {
                        var prompts = composer.chat.prompts
                        var idx = composer.historyIndex
                        idx += (event.key === Qt.Key_Up) ? 1 : -1
                        if (idx < -1) idx = -1
                        if (idx >= prompts.length) idx = prompts.length - 1
                        composer.historyIndex = idx
                        input.text = idx >= 0 ? prompts[idx] : ""
                        input.cursorPosition = input.text.length
                        event.accepted = true
                    }
                }
            }
        }
    }

    // ---- Chip row + send ----
    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        // Attachment picker.
        ToolButton {
            text: "\u{1F4CE}"
            font.pixelSize: 13
            enabled: !composer.disabled
            onClicked: fileDialog.open()
            ToolTip.visible: hovered
            ToolTip.text: "Attach file"
        }

        // Compact model override.
        ComboBox {
            id: modelBox
            Layout.preferredWidth: 110
            font.pixelSize: 10
            flat: true
            enabled: !composer.disabled
            model: {
                var items = [{ id: "", displayName: "Model" }]
                var configs = agentsController.modelConfigs
                for (var i = 0; i < configs.length; ++i) items.push(configs[i])
                return items
            }
            textRole: "displayName"
            valueRole: "id"
            onActivated: composer.modelOverrideId = currentValue
        }

        // MCP server override menu (per-message mcp_server_ids).
        ToolButton {
            text: "MCP"
            font.pixelSize: 10
            enabled: !composer.disabled
            visible: agentsController.mcpServers.length > 0
            onClicked: mcpMenu.open()
            Menu {
                id: mcpMenu
                Repeater {
                    model: agentsController.mcpServers
                    MenuItem {
                        required property var modelData
                        text: modelData.displayName
                        checkable: true
                        // force_on servers are always included and locked.
                        enabled: !modelData.forceOn
                        checked: modelData.forceOn
                                 || composer.mcpOverrideIds.indexOf(modelData.id) >= 0
                        onToggled: {
                            var ids = composer.mcpOverrideIds.slice()
                            var at = ids.indexOf(modelData.id)
                            if (checked && at < 0) ids.push(modelData.id)
                            if (!checked && at >= 0) ids.splice(at, 1)
                            composer.mcpOverrideIds = ids
                        }
                    }
                }
            }
        }

        // Persistent plan-mode toggle.
        ToolButton {
            text: "Plan"
            font.pixelSize: 10
            checkable: true
            enabled: !composer.disabled
            checked: composer.chat ? composer.chat.planMode : false
            onClicked: composer.chat.setPlanModeEnabled(checked)
            ToolTip.visible: hovered
            ToolTip.text: "Plan mode"
        }

        // Busy behavior for sends while the agent works.
        ToolButton {
            text: composer.busyBehavior === "queue" ? "Queue" : "Interrupt"
            font.pixelSize: 10
            enabled: !composer.disabled
            onClicked: busyMenu.open()
            ToolTip.visible: hovered
            ToolTip.text: "What happens when you send while the agent is busy"
            Menu {
                id: busyMenu
                MenuItem {
                    text: "Queue (default)"
                    checkable: true
                    checked: composer.busyBehavior === "queue"
                    onTriggered: composer.busyBehavior = "queue"
                }
                MenuItem {
                    text: "Interrupt"
                    checkable: true
                    checked: composer.busyBehavior === "interrupt"
                    onTriggered: composer.busyBehavior = "interrupt"
                }
            }
        }

        Item { Layout.fillWidth: true }

        // Send button; morphs into a Stop square while the agent runs and no
        // sendable input exists.
        Button {
            id: sendButton
            readonly property bool stopMode: composer.agentBusy && !composer.canSend
            enabled: !composer.disabled && (stopMode || composer.canSend)
            implicitWidth: 36
            implicitHeight: 36
            onClicked: stopMode ? composer.chat.interrupt() : composer.sendNow()
            ToolTip.visible: hovered
            ToolTip.text: stopMode ? "Stop the agent (Esc)" : "Send"

            background: Rectangle {
                radius: 18
                color: sendButton.enabled
                    ? (sendButton.stopMode ? CoderTheme.error : CoderTheme.primary)
                    : CoderTheme.surfaceSecondary
            }
            contentItem: Item {
                // Paper-plane triangle for send; square for stop.
                Label {
                    anchors.centerIn: parent
                    visible: !sendButton.stopMode
                    text: "\u27a4"
                    color: CoderTheme.textInvert
                    font.pixelSize: 15
                }
                Rectangle {
                    anchors.centerIn: parent
                    visible: sendButton.stopMode
                    width: 12; height: 12; radius: 2
                    color: CoderTheme.textInvert
                }
            }
        }
    }
}
