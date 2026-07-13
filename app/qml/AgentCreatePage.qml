import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import CoderDesktop

// New-agent creation page: large prompt editor, workspace picker, model
// picker, reasoning-effort selector, MCP multi-select (force_on servers are
// always included and locked), plan-mode switch, and attachments. On success
// the parent AgentsPage navigates into the created chat.
Rectangle {
    id: page
    signal backClicked()

    color: CoderTheme.background

    // Input fence: this page is loaded on top of the still-live chat list;
    // consume all pointer events so nothing falls through to the hidden
    // rows underneath (see AgentChatPage for the full explanation).
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onClicked: {}
        onWheel: function(wheel) { wheel.accepted = true }
    }

    // Attachment chips: {localPath, name, fileId, uploading, failed, mediaType}
    property var attachments: []
    property var mcpSelectedIds: []
    property bool submitting: false

    readonly property bool uploadsPending: {
        for (var i = 0; i < attachments.length; ++i)
            if (attachments[i].uploading) return true
        return false
    }

    function addAttachment(fileUrl) {
        var path = fileUrl.toString().replace(/^file:\/\//, "")
        var list = attachments.slice()
        list.push({ localPath: path, name: path.split("/").pop(), fileId: "",
                    uploading: true, failed: false, mediaType: "" })
        attachments = list
        agentsController.uploadAttachment(fileUrl)
    }

    Connections {
        target: agentsController
        function onAttachmentUploaded(localPath, fileId, name, mediaType) {
            var list = page.attachments.slice()
            for (var i = 0; i < list.length; ++i) {
                if (list[i].localPath === localPath && list[i].uploading) {
                    list[i].uploading = false
                    list[i].fileId = fileId
                    list[i].mediaType = mediaType
                    break
                }
            }
            page.attachments = list
        }
        function onAttachmentUploadFailed(localPath, error) {
            var list = page.attachments.slice()
            for (var i = 0; i < list.length; ++i) {
                if (list[i].localPath === localPath && list[i].uploading) {
                    list[i].uploading = false
                    list[i].failed = true
                    break
                }
            }
            page.attachments = list
        }
        function onChatCreated(chatId) { page.submitting = false }
    }

    FileDialog {
        id: fileDialog
        title: "Attach file"
        onAccepted: page.addAttachment(selectedFile)
    }

    // Mark workspaces that already have an agent chat ("in use") in the
    // picker, refreshed via GET /chats/by-workspace whenever the workspace
    // list is available.
    function refreshInUse() {
        var ids = []
        var count = workspaceModel.count
        for (var i = 0; i < count; ++i) {
            var idx = workspaceModel.index(i, 0)
            ids.push(workspaceModel.data(idx, 257))  // IdRole
        }
        if (ids.length > 0) agentsController.refreshWorkspacesInUse(ids)
    }
    Component.onCompleted: refreshInUse()
    Connections {
        target: workspaceModel
        function onCountChanged() { page.refreshInUse() }
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentWidth: width
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: content
            // Centered readable form width when the window is maximized.
            width: Math.min(parent.width, 700)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ToolButton {
                    text: "\u2190"
                    font.pixelSize: 16
                    onClicked: page.backClicked()
                }
                Label {
                    text: "New agent"
                    font.pixelSize: 16
                    font.bold: true
                    color: CoderTheme.textPrimary
                    Layout.fillWidth: true
                }
            }

            // ---- Prompt editor (autosizing, clipped container) ----
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: Math.max(120, Math.min(promptInput.implicitHeight + 16, 280))
                radius: CoderTheme.radius
                color: CoderTheme.surface
                border.color: promptInput.activeFocus ? CoderTheme.primary : CoderTheme.border
                border.width: 1
                clip: true

                Flickable {
                    id: promptFlick
                    anchors.fill: parent
                    anchors.margins: 4
                    contentWidth: width
                    contentHeight: promptInput.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    TextArea.flickable: TextArea {
                        id: promptInput
                        placeholderText: "Describe what the agent should do\u2026"
                        wrapMode: TextArea.Wrap
                        color: CoderTheme.textPrimary
                        placeholderTextColor: CoderTheme.textDisabled
                        font.pixelSize: 13
                        background: null
                    }
                }
            }

            // ---- Attachments ----
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                CoderButton {
                    text: "Attach file"
                    variant: "outline"
                    onClicked: fileDialog.open()
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 4
                    Repeater {
                        model: page.attachments
                        FileChip {
                            required property var modelData
                            required property int index
                            name: modelData.name
                            uploading: modelData.uploading
                            failed: modelData.failed
                            removable: true
                            onRemoveClicked: {
                                var list = page.attachments.slice()
                                list.splice(index, 1)
                                page.attachments = list
                            }
                        }
                    }
                }
            }

            // ---- Workspace picker ----
            Label { text: "Workspace"; color: CoderTheme.textSecondary; font.pixelSize: 11 }
            ComboBox {
                id: workspaceBox
                Layout.fillWidth: true
                font.pixelSize: 12
                textRole: "name"
                valueRole: "id"
                model: {
                    // First entry is "No workspace"; the rest mirror
                    // workspaceModel rows, tagged "(in use)" when the
                    // workspace already has an agent chat.
                    var inUse = agentsController.workspacesInUse
                    var items = [{ id: "", name: "No workspace" }]
                    var count = workspaceModel.count
                    for (var i = 0; i < count; ++i) {
                        var idx = workspaceModel.index(i, 0)
                        var wsId = workspaceModel.data(idx, 257)   // IdRole
                        var wsName = workspaceModel.data(idx, 258) // NameRole
                        items.push({
                            id: wsId,
                            name: inUse[wsId] !== undefined
                                ? wsName + "  (in use)" : wsName
                        })
                    }
                    return items
                }
                Component.onCompleted: {
                    // Restore the persisted last selection.
                    var last = agentsController.lastWorkspaceId
                    if (last.length > 0) {
                        var at = indexOfValue(last)
                        if (at >= 0) currentIndex = at
                    }
                }
                onActivated: agentsController.lastWorkspaceId = currentValue
            }

            // ---- Model picker ----
            Label { text: "Model"; color: CoderTheme.textSecondary; font.pixelSize: 11 }
            ComboBox {
                id: modelBox
                Layout.fillWidth: true
                font.pixelSize: 12
                textRole: "displayName"
                valueRole: "id"
                model: agentsController.modelConfigs
                function selectPreferred() {
                    var preferred = agentsController.lastModelConfigId.length > 0
                        ? agentsController.lastModelConfigId
                        : agentsController.defaultModelConfigId
                    var at = indexOfValue(preferred)
                    if (at >= 0) currentIndex = at
                }
                Component.onCompleted: selectPreferred()
                onModelChanged: selectPreferred()
                onActivated: agentsController.lastModelConfigId = currentValue
            }

            // ---- Reasoning effort ----
            Label { text: "Reasoning effort"; color: CoderTheme.textSecondary; font.pixelSize: 11 }
            ComboBox {
                id: effortBox
                Layout.fillWidth: true
                font.pixelSize: 12
                textRole: "label"
                valueRole: "value"
                model: [
                    { label: "Default", value: "" },
                    { label: "Off", value: "none" },
                    { label: "Low", value: "low" },
                    { label: "Medium", value: "medium" },
                    { label: "High", value: "high" }
                ]
            }

            // ---- MCP servers ----
            Label {
                visible: agentsController.mcpServers.length > 0
                text: "MCP servers"
                color: CoderTheme.textSecondary
                font.pixelSize: 11
            }
            ColumnLayout {
                visible: agentsController.mcpServers.length > 0
                Layout.fillWidth: true
                spacing: 0
                Repeater {
                    model: agentsController.mcpServers
                    CheckBox {
                        required property var modelData
                        text: modelData.displayName
                        font.pixelSize: 12
                        // force_on servers are always included and locked.
                        checked: modelData.forceOn
                                 || modelData.defaultOn
                                 || page.mcpSelectedIds.indexOf(modelData.id) >= 0
                        enabled: !modelData.forceOn
                        onToggled: {
                            var ids = page.mcpSelectedIds.slice()
                            var at = ids.indexOf(modelData.id)
                            if (checked && at < 0) ids.push(modelData.id)
                            if (!checked && at >= 0) ids.splice(at, 1)
                            page.mcpSelectedIds = ids
                        }
                        Component.onCompleted: {
                            if (modelData.defaultOn
                                    && page.mcpSelectedIds.indexOf(modelData.id) < 0) {
                                var ids = page.mcpSelectedIds.slice()
                                ids.push(modelData.id)
                                page.mcpSelectedIds = ids
                            }
                        }
                    }
                }
            }

            // ---- Plan mode ----
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "Plan mode"
                    color: CoderTheme.textPrimary
                    font.pixelSize: 13
                    Layout.fillWidth: true
                }
                Switch { id: planSwitch }
            }

            // ---- Submit ----
            CoderButton {
                Layout.fillWidth: true
                text: page.submitting ? "Creating\u2026" : "Start agent"
                enabled: !page.submitting && !page.uploadsPending
                         && promptInput.text.trim().length > 0
                onClicked: {
                    page.submitting = true
                    var files = []
                    var ids = page.mcpSelectedIds.slice()
                    // force_on servers are always sent.
                    var servers = agentsController.mcpServers
                    for (var s = 0; s < servers.length; ++s) {
                        if (servers[s].forceOn && ids.indexOf(servers[s].id) < 0)
                            ids.push(servers[s].id)
                    }
                    for (var i = 0; i < page.attachments.length; ++i) {
                        var a = page.attachments[i]
                        if (!a.failed && a.fileId)
                            files.push({ fileId: a.fileId, name: a.name,
                                         mediaType: a.mediaType })
                    }
                    agentsController.createChat(
                        promptInput.text, workspaceBox.currentValue,
                        modelBox.currentValue !== undefined ? modelBox.currentValue : "",
                        effortBox.currentValue, ids, planSwitch.checked, files)
                }
            }

            Item { Layout.preferredHeight: 12 }
        }
    }
}
