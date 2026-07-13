import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import CoderDesktop

// Chat composer: one compact input row (min ~48px) that grows with the
// text up to ~40% of the window height and scrolls internally beyond
// that. An options button in front of the editor holds the attachment
// picker and the per-message overrides (model, MCP, plan mode, busy
// behavior); the send button morphs into Stop while the agent is running.
// Attachment chips and active-override chips render only when present, so
// the idle composer stays a single row.
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

    // ---- Input row: options, editor, send ----
    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        // Options: attachments plus per-message overrides, folded into one
        // menu so the composer stays a single ~48px row when idle.
        ToolButton {
            id: optionsButton
            implicitWidth: 30
            implicitHeight: 30
            padding: 0
            text: "+"
            font.pixelSize: 16
            enabled: !composer.disabled
            Layout.alignment: Qt.AlignBottom
            onClicked: optionsMenu.open()
            ToolTip.visible: hovered
            ToolTip.text: "Attachments and message options"

            Menu {
                id: optionsMenu
                MenuItem {
                    text: "Attach file\u2026"
                    onTriggered: fileDialog.open()
                }
                Menu {
                    title: "Model"
                    MenuItem {
                        text: "Default"
                        checkable: true
                        checked: composer.modelOverrideId.length === 0
                        onTriggered: composer.modelOverrideId = ""
                    }
                    Repeater {
                        model: agentsController.modelConfigs
                        MenuItem {
                            required property var modelData
                            text: modelData.displayName
                            checkable: true
                            checked: composer.modelOverrideId === modelData.id
                            onTriggered: composer.modelOverrideId = modelData.id
                        }
                    }
                }
                Menu {
                    title: "MCP servers"
                    // MCP server override menu (per-message mcp_server_ids).
                    enabled: agentsController.mcpServers.length > 0
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
                MenuItem {
                    // Persistent plan-mode toggle.
                    text: "Plan mode"
                    checkable: true
                    checked: composer.chat ? composer.chat.planMode : false
                    onTriggered: composer.chat.setPlanModeEnabled(checked)
                }
                Menu {
                    // Busy behavior for sends while the agent works.
                    title: "When busy"
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
        }

        // Active-override chips: zero footprint unless an override is on.
        Rectangle {
            visible: composer.chat ? composer.chat.planMode : false
            implicitWidth: planChipLabel.implicitWidth + 12
            implicitHeight: 20
            radius: 10
            color: CoderTheme.activeSurface
            Layout.alignment: Qt.AlignBottom
            Layout.bottomMargin: 5
            Label {
                id: planChipLabel
                anchors.centerIn: parent
                text: "Plan"
                color: CoderTheme.primary
                font.pixelSize: 10
            }
            ToolTip.visible: planChipHover.hovered
            ToolTip.text: "Plan mode is on"
            HoverHandler { id: planChipHover }
        }
        Rectangle {
            visible: composer.modelOverrideId.length > 0
            implicitWidth: modelChipLabel.implicitWidth + 12
            implicitHeight: 20
            radius: 10
            color: CoderTheme.surfaceSecondary
            border.color: CoderTheme.border
            border.width: 1
            Layout.alignment: Qt.AlignBottom
            Layout.bottomMargin: 5
            Label {
                id: modelChipLabel
                anchors.centerIn: parent
                text: {
                    var configs = agentsController.modelConfigs
                    for (var i = 0; i < configs.length; ++i)
                        if (configs[i].id === composer.modelOverrideId)
                            return configs[i].displayName
                    return "Model"
                }
                color: CoderTheme.textSecondary
                font.pixelSize: 10
            }
        }

        // ---- Editor ----
        Rectangle {
            Layout.fillWidth: true
            // Autosize with the text up to ~40% of the window; beyond that
            // the inner Flickable scrolls.
            readonly property real maxEditorHeight:
                (Window.window ? Window.window.height : 700) * 0.4
            implicitHeight: Math.max(36, Math.min(input.implicitHeight + 8, maxEditorHeight))
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
                // Consume wheel events so scrolling the composer never
                // scrolls the chat timeline behind it.
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
                    topPadding: 5
                    bottomPadding: 5
                    leftPadding: 8
                    rightPadding: 8
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
                            var sendOnPlainEnter =
                                agentsController.sendShortcut !== "modifier_enter"
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

        // Send button; morphs into a Stop square while the agent runs and
        // no sendable input exists. Bottom-anchored so it stays by the
        // newest line while the editor grows.
        Button {
            id: sendButton
            readonly property bool stopMode: composer.agentBusy && !composer.canSend
            enabled: !composer.disabled && (stopMode || composer.canSend)
            implicitWidth: 32
            implicitHeight: 32
            Layout.alignment: Qt.AlignBottom
            onClicked: stopMode ? composer.chat.interrupt() : composer.sendNow()
            ToolTip.visible: hovered
            ToolTip.text: stopMode ? "Stop the agent (Esc)" : "Send"

            display: AbstractButton.IconOnly
            icon.source: stopMode ? "qrc:/CoderDesktop/assets/icons/stop.svg"
                                  : "qrc:/CoderDesktop/assets/icons/send.svg"
            icon.color: CoderTheme.textInvert
            icon.width: 15
            icon.height: 15

            background: Rectangle {
                radius: 16
                color: sendButton.enabled
                    ? (sendButton.stopMode ? CoderTheme.error : CoderTheme.primary)
                    : CoderTheme.surfaceSecondary
            }
        }
    }
}
