import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

// Coder Agents list page: search, filter row, time-grouped chat list with
// pinned section first, per-row context menu, availability notice, and the
// create / detail sub-pages hosted in Loaders (same pattern as the previous
// Tasks page).
Item {
    id: agentsPage

    // Navigation state. selectedChatId opens the chat detail; showCreate
    // opens the create form. chatTrail records parent chats so sub-agent
    // navigation can breadcrumb back.
    property string selectedChatId: ""
    property var chatTrail: []
    property bool showCreate: false

    // Rename dialog state.
    property string renameChatId: ""

    function openChat(chatId) {
        chatTrail = []
        selectedChatId = chatId
    }

    function openChildChat(chatId) {
        var trail = chatTrail.slice()
        trail.push(selectedChatId)
        chatTrail = trail
        selectedChatId = chatId
    }

    function goBack() {
        if (chatTrail.length > 0) {
            var trail = chatTrail.slice()
            selectedChatId = trail.pop()
            chatTrail = trail
        } else {
            selectedChatId = ""
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        // ---- Header: title + refresh + new agent ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Agents"
                font.pixelSize: 18
                font.bold: true
                color: CoderTheme.textPrimary
                Layout.fillWidth: true
            }
            CoderButton {
                text: "Refresh"
                variant: "outline"
                onClicked: agentsController.refreshNow()
            }
            CoderButton {
                text: "New agent"
                enabled: agentsController.available
                onClicked: agentsPage.showCreate = true
            }
        }

        // ---- Search ----
        TextField {
            id: searchField
            placeholderText: "Search agents\u2026"
            Layout.fillWidth: true
            selectByMouse: true
            onTextChanged: chatListModel.searchText = text
            background: Rectangle {
                implicitHeight: 36
                radius: CoderTheme.radius
                color: CoderTheme.surface
                border.color: searchField.activeFocus ? CoderTheme.primary : CoderTheme.border
                border.width: 1
            }
        }

        // ---- Filter row ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                // Values match ChatListModel::Filter: 0 All, 1 Unread,
                // 2 RequiresAction, 3 Archived.
                model: [
                    { label: "All", value: 0 },
                    { label: "Unread", value: 1 },
                    { label: "Needs action", value: 2 },
                    { label: "Archived", value: 3 }
                ]
                Rectangle {
                    required property var modelData
                    readonly property bool active: chatListModel.filter === modelData.value
                    implicitWidth: filterLabel.implicitWidth + 20
                    implicitHeight: 26
                    radius: 13
                    color: active ? CoderTheme.activeSurface : CoderTheme.surface
                    border.color: active ? CoderTheme.primary : CoderTheme.border
                    border.width: 1

                    Label {
                        id: filterLabel
                        anchors.centerIn: parent
                        text: modelData.label
                        font.pixelSize: 11
                        color: parent.active ? CoderTheme.primary : CoderTheme.textSecondary
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: chatListModel.filter = modelData.value
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }

        // ---- Availability notice ----
        Rectangle {
            visible: !agentsController.available
                     && agentsController.availabilityReason.length > 0
            Layout.fillWidth: true
            implicitHeight: availCol.implicitHeight + 16
            radius: CoderTheme.radiusSm
            color: CoderTheme.warningSurface
            border.color: CoderTheme.warning
            border.width: 1

            ColumnLayout {
                id: availCol
                anchors.fill: parent
                anchors.margins: 8
                spacing: 2
                Label {
                    text: "Coder Agents is not available on this deployment"
                    color: CoderTheme.warning
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Label {
                    text: {
                        var r = agentsController.availabilityReason
                        if (r === "chat daemon not configured")
                            return "The deployment has no chat daemon (chatd) configured. Ask your administrator to enable Coder Agents."
                        if (r === "missing chat create permission")
                            return "Your account does not have permission to create agent chats."
                        return "The chats endpoint could not be reached. Retry after checking your connection."
                    }
                    color: CoderTheme.textSecondary
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                CoderButton {
                    text: "Retry"
                    variant: "outline"
                    onClicked: agentsController.start()
                }
            }
        }

        // Models-not-configured notice (endpoint is fine, no usable model).
        Rectangle {
            visible: agentsController.available && !agentsController.modelsAvailable
            Layout.fillWidth: true
            implicitHeight: modelsNotice.implicitHeight + 16
            radius: CoderTheme.radiusSm
            color: CoderTheme.warningSurface
            border.color: CoderTheme.warning
            border.width: 1
            Label {
                id: modelsNotice
                anchors.fill: parent
                anchors.margins: 8
                text: "No AI model providers are configured or available. Agents may fail until an administrator adds a provider API key."
                color: CoderTheme.warning
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }

        // ---- Chat list ----
        ListView {
            id: chatList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            // Conditional model binding per the repo's tab-switch workaround.
            model: agentsPage.visible ? chatListModel : null

            section.property: "timeGroup"
            section.criteria: ViewSection.FullString
            section.delegate: Label {
                required property string section
                width: chatList.width
                text: section
                color: CoderTheme.textDisabled
                font.pixelSize: 10
                font.capitalization: Font.AllUppercase
                font.weight: Font.DemiBold
                topPadding: 10
                bottomPadding: 2
            }

            delegate: Rectangle {
                id: row
                required property var model
                required property int index
                width: chatList.width
                height: rowLayout.implicitHeight + 16
                radius: CoderTheme.radius
                color: rowMa.containsMouse ? CoderTheme.hoverBg : CoderTheme.surface
                border.color: CoderTheme.border
                border.width: 1

                RowLayout {
                    id: rowLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: row.model.isSubagent ? 28 : 10
                    anchors.rightMargin: 8
                    spacing: 8

                    // Sub-agent connector.
                    Label {
                        visible: row.model.isSubagent
                        text: "\u2937"
                        color: CoderTheme.textDisabled
                        font.pixelSize: 11
                    }

                    // Status indicator: colored dot, spinning arc while
                    // running, "!" overlay for requires_action.
                    Item {
                        implicitWidth: 14
                        implicitHeight: 14
                        Rectangle {
                            id: statusDot
                            anchors.centerIn: parent
                            width: 10; height: 10; radius: 5
                            color: {
                                var s = row.model.statusString
                                if (s === "running" || s === "pending") return CoderTheme.warning
                                if (s === "completed") return CoderTheme.success
                                if (s === "error") return CoderTheme.error
                                if (s === "requires_action") return CoderTheme.warning
                                return CoderTheme.textDisabled
                            }
                        }
                        // Spinner ring while running.
                        Rectangle {
                            visible: row.model.statusString === "running"
                                     || row.model.statusString === "pending"
                            anchors.centerIn: parent
                            width: 14; height: 14; radius: 7
                            color: "transparent"
                            border.color: CoderTheme.warning
                            border.width: 1
                            opacity: 0.7
                            // A notch that rotates to read as activity.
                            Rectangle {
                                width: 4; height: 4; radius: 2
                                color: CoderTheme.background
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                            RotationAnimation on rotation {
                                running: visible && agentsPage.visible
                                loops: Animation.Infinite
                                from: 0; to: 360
                                duration: 1200
                            }
                        }
                        Label {
                            visible: row.model.requiresAction
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.rightMargin: -4
                            anchors.topMargin: -6
                            text: "!"
                            color: CoderTheme.error
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }

                    ColumnLayout {
                        spacing: 1
                        Layout.fillWidth: true

                        RowLayout {
                            spacing: 6
                            Layout.fillWidth: true
                            Label {
                                text: row.model.pinned ? "\u2605" : ""
                                visible: row.model.pinned
                                color: CoderTheme.warning
                                font.pixelSize: 10
                            }
                            Label {
                                text: row.model.title.length > 0 ? row.model.title : "Untitled"
                                color: CoderTheme.textPrimary
                                font.pixelSize: 13
                                font.bold: row.model.hasUnread
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            // Unread dot.
                            Rectangle {
                                visible: row.model.hasUnread
                                width: 8; height: 8; radius: 4
                                color: CoderTheme.primary
                            }
                        }
                        RowLayout {
                            spacing: 6
                            Layout.fillWidth: true
                            // Workspace chip.
                            Rectangle {
                                visible: row.model.workspaceName.length > 0
                                implicitWidth: wsLabel.implicitWidth + 12
                                implicitHeight: 16
                                radius: 8
                                color: CoderTheme.surfaceSecondary
                                Label {
                                    id: wsLabel
                                    anchors.centerIn: parent
                                    text: row.model.workspaceName
                                    color: CoderTheme.textSecondary
                                    font.pixelSize: 9
                                }
                            }
                            Label {
                                visible: row.model.diffStatus.length > 0
                                text: row.model.diffStatus
                                color: CoderTheme.textSecondary
                                font.family: "monospace"
                                font.pixelSize: 9
                            }
                            Item { Layout.fillWidth: true }
                            Label {
                                text: row.model.relativeTime
                                color: CoderTheme.textDisabled
                                font.pixelSize: 10
                            }
                        }
                    }

                    // Overflow button (same menu as right-click).
                    ToolButton {
                        text: "\u22ee"
                        font.pixelSize: 12
                        visible: rowMa.containsMouse
                        implicitWidth: 22
                        implicitHeight: 22
                        onClicked: row.openContextMenu()
                    }
                }

                function openContextMenu() {
                    contextMenu.chatId = row.model.id
                    contextMenu.pinned = row.model.pinned
                    contextMenu.archived = row.model.archived
                    contextMenu.chatTitle = row.model.title
                    contextMenu.popup()
                }

                MouseArea {
                    id: rowMa
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    // Leave the overflow button clickable.
                    z: -1
                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) {
                            row.openContextMenu()
                        } else {
                            agentsPage.openChat(row.model.id)
                        }
                    }
                }
            }

            // ---- Empty state ----
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 8
                visible: chatList.count === 0 && agentsController.available
                opacity: 0.6

                Label {
                    text: "No agents yet"
                    font.pixelSize: 16
                    color: CoderTheme.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                }
                Label {
                    text: "Start a new agent to delegate work"
                    font.pixelSize: 12
                    color: CoderTheme.textDisabled
                    Layout.alignment: Qt.AlignHCenter
                }
                CoderButton {
                    text: "New agent"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: agentsPage.showCreate = true
                }
            }
        }
    }

    // Shared context menu for chat rows.
    Menu {
        id: contextMenu
        property string chatId: ""
        property bool pinned: false
        property bool archived: false
        property string chatTitle: ""

        MenuItem {
            text: contextMenu.pinned ? "Unpin" : "Pin"
            onTriggered: agentsController.pinChat(contextMenu.chatId, !contextMenu.pinned)
        }
        MenuItem {
            text: contextMenu.archived ? "Unarchive" : "Archive"
            onTriggered: agentsController.archiveChat(contextMenu.chatId, !contextMenu.archived)
        }
        MenuItem {
            text: "Rename"
            onTriggered: {
                agentsPage.renameChatId = contextMenu.chatId
                renameField.text = contextMenu.chatTitle
                renameDialog.open()
                renameField.forceActiveFocus()
            }
        }
        MenuItem {
            text: "Regenerate title"
            onTriggered: agentsController.regenerateChatTitle(contextMenu.chatId)
        }
    }

    // Rename dialog (PATCH title).
    Dialog {
        id: renameDialog
        title: "Rename agent"
        modal: true
        anchors.centerIn: parent
        width: Math.min(320, parent.width - 48)
        standardButtons: Dialog.Ok | Dialog.Cancel

        TextField {
            id: renameField
            width: parent.width
            selectByMouse: true
            onAccepted: renameDialog.accept()
        }
        onAccepted: {
            if (renameField.text.trim().length > 0)
                agentsController.renameChat(agentsPage.renameChatId, renameField.text.trim())
        }
    }

    // ---- Create page ----
    Loader {
        anchors.fill: parent
        active: agentsPage.showCreate
        sourceComponent: AgentCreatePage {
            onBackClicked: agentsPage.showCreate = false
        }
    }
    // Navigate into a freshly created chat.
    Connections {
        target: agentsController
        function onChatCreated(chatId) {
            if (agentsPage.showCreate) {
                agentsPage.showCreate = false
                agentsPage.openChat(chatId)
            }
        }
    }

    // ---- Chat detail ----
    Loader {
        anchors.fill: parent
        active: agentsPage.selectedChatId.length > 0
        sourceComponent: AgentChatPage {
            chatId: agentsPage.selectedChatId
            onBackClicked: agentsPage.goBack()
            onOpenChildRequested: function(childId) { agentsPage.openChildChat(childId) }
        }
    }
}
