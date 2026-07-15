import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

// Workspace detail page — shows workspace header, status, agents, apps,
// and action buttons. Shown via a Loader from WorkspacesPage when a
// workspace is selected.
Item {
    id: workspaceDetailPage

    property string workspaceId: ""
    property string workspaceName: ""
    property string workspaceOwner: ""
    property string workspaceTemplate: ""
    property string workspaceStatus: ""
    property bool   workspaceOutdated: false
    property string workspaceHealth: ""

    signal backClicked()

    // -- Local models populated from fetchWorkspaceDetail response ----------
    ListModel { id: agentsModel }

    property bool detailLoading: false
    property string errorMessage: ""

    // -- In-app browser state ------------------------------------------------
    property string selectedAppSlug: ""
    property string selectedAppUrl: ""
    property string selectedAppName: ""
    property string selectedAgentId: ""
    property string selectedAgentName: ""
    property bool selectedAppExternal: false

    // -- Terminal state --
    property string selectedTerminalAgentId: ""
    property string selectedTerminalAgentName: ""
    // -- File browser state --
    property string selectedFileBrowserAgentHost: ""
    property string selectedFileBrowserWorkspaceName: ""

    // -- Helper: build apps list from a raw agent JSON object ----------------
    function buildAppsList(agent) {
        var list = []
        var agentApps = agent["apps"] || []
        for (var p = 0; p < agentApps.length; p++) {
            var app = agentApps[p]
            list.push({
                appName: app["display_name"] || app["slug"] || "",
                appUrl: app["url"] || "",
                appIcon: app["icon"] || "",
                appCommand: app["command"] || "",
                appSlug: app["slug"] || "",
                appSubdomain: app["subdomain"] || false,
                appExternal: app["external"] || false,
                isDisplayApp: false,
                displayAppType: ""
            })
        }
        var displayApps = agent["display_apps"] || []
        for (var d = 0; d < displayApps.length; d++) {
            var da = displayApps[d]
            if (da === "vscode" || da === "vscode_insiders") {
                list.push({
                    appName: da === "vscode" ? "VS Code Desktop" : "VS Code Insiders",
                    appUrl: "", appIcon: "/icon/code.svg", appCommand: "",
                    appSlug: da, appSubdomain: false, appExternal: false,
                    isDisplayApp: true, displayAppType: da
                })
            } else if (da === "ssh_helper") {
                list.push({
                    appName: "SSH", appUrl: "", appIcon: "", appCommand: "",
                    appSlug: "ssh_helper", appSubdomain: false, appExternal: false,
                    isDisplayApp: true, displayAppType: "ssh_helper"
                })
            } else if (da === "port_forwarding_helper") {
                list.push({
                    appName: "Ports", appUrl: "", appIcon: "", appCommand: "",
                    appSlug: "port_forwarding_helper", appSubdomain: false, appExternal: false,
                    isDisplayApp: true, displayAppType: "port_forwarding_helper"
                })
            }
        }
        return list
    }

    // -- Helper: handle app tile click ----------------------------------------
    function handleAppClick(appData, agentId, agentName, agentDirectory) {
        if (appData.isDisplayApp === true) {
            var daType = appData.displayAppType
            if (daType === "vscode" || daType === "vscode_insiders") {
                vscodeLaunchDialog.workspaceName = workspaceDetailPage.workspaceName
                vscodeLaunchDialog.workspaceOwner = workspaceDetailPage.workspaceOwner
                vscodeLaunchDialog.agentName = agentName
                vscodeLaunchDialog.agentId = agentId
                vscodeLaunchDialog.agentDirectory = agentDirectory || ""
                vscodeLaunchDialog.displayAppType = daType
                vscodeLaunchDialog.open()
            } else if (daType === "ssh_helper") {
                sshHelperDialog.agentName = agentName
                sshHelperDialog.workspaceName = workspaceDetailPage.workspaceName
                sshHelperDialog.workspaceOwner = workspaceDetailPage.workspaceOwner
                sshHelperDialog.open()
            } else if (daType === "port_forwarding_helper") {
                portForwardDialog.agentName = agentName
                portForwardDialog.workspaceName = workspaceDetailPage.workspaceName
                portForwardDialog.workspaceOwner = workspaceDetailPage.workspaceOwner
                portForwardDialog.open()
            }
        } else {
            workspaceDetailPage.selectedAppSlug = appData.appSlug
            workspaceDetailPage.selectedAppUrl = appData.appUrl
            workspaceDetailPage.selectedAppName = appData.appName
            workspaceDetailPage.selectedAgentId = agentId
            workspaceDetailPage.selectedAgentName = agentName
            workspaceDetailPage.selectedAppExternal = appData.appExternal
        }
    }

    // -- Helper: open app in external browser ---------------------------------
    function handleOpenInBrowser(appData, agentName) {
        var url = appBrowser.buildAppUrl(
            sessionManager.currentUrl, appData.appUrl, appData.appSlug,
            workspaceDetailPage.workspaceName, workspaceDetailPage.workspaceOwner,
            agentName, vpnBridge.isRunning, appData.appExternal
        )
        Qt.openUrlExternally(url)
    }

    function loadDetail() {
        detailLoading = true
        apiClient.fetchWorkspaceDetail(workspaceId)
    }

    Component.onCompleted: loadDetail()

    // -- Signal handlers ----------------------------------------------------
    Connections {
        target: apiClient

        function onWorkspaceDetailReceived(workspace) {
            detailLoading = false
            workspaceDetailPage.errorMessage = ""
            agentsModel.clear()

            var build = workspace["latest_build"] || {}
            var resources = build["resources"] || []

            // Collect all agents flat
            var allAgents = []
            for (var r = 0; r < resources.length; r++) {
                var agents = resources[r]["agents"] || []
                for (var a = 0; a < agents.length; a++) {
                    allAgents.push(agents[a])
                }
            }

            // Separate top-level agents from children (by parent_id)
            var topLevel = []
            var children = {}  // parentId -> [agent, ...]
            for (var i = 0; i < allAgents.length; i++) {
                var ag = allAgents[i]
                var pid = ag["parent_id"] || ""
                if (pid === "" || pid === null || pid === undefined) {
                    topLevel.push(ag)
                } else {
                    if (!children[pid]) children[pid] = []
                    children[pid].push(ag)
                }
            }

            // Build model entries for top-level agents (with inline apps + children)
            for (var t = 0; t < topLevel.length; t++) {
                var agent = topLevel[t]
                var agId = agent["id"] || ""
                var kidAgents = children[agId] || []
                var kidArray = []
                for (var k = 0; k < kidAgents.length; k++) {
                    var kid = kidAgents[k]
                    kidArray.push({
                        id: kid["id"] || "",
                        name: kid["name"] || "",
                        status: kid["status"] || "unknown",
                        os: kid["operating_system"] || "",
                        arch: kid["architecture"] || "",
                        directory: kid["expanded_directory"] || kid["directory"] || "",
                        displayApps: (kid["display_apps"] || []).join(","),
                        apps: JSON.stringify(buildAppsList(kid))
                    })
                }

                agentsModel.append({
                    agentId: agId,
                    agentName: agent["name"] || "",
                    agentStatus: agent["status"] || "unknown",
                    agentOs: agent["operating_system"] || "",
                    agentArch: agent["architecture"] || "",
                    agentDirectory: agent["expanded_directory"] || agent["directory"] || "",
                    agentDisplayApps: (agent["display_apps"] || []).join(","),
                    agentApps: JSON.stringify(buildAppsList(agent)),
                    childAgents: JSON.stringify(kidArray)
                })
            }

            // Update status from fresh data
            var latestStatus = (build["status"] || workspaceStatus)
            if (latestStatus.length > 0) {
                workspaceDetailPage.workspaceStatus =
                    latestStatus.charAt(0).toUpperCase() + latestStatus.slice(1)
            }

            workspaceDetailPage.workspaceHealth = workspace["health"]
                ? (workspace["health"]["healthy"] === true ? "Healthy" : "Unhealthy")
                : ""
        }

        function onWorkspaceActionCompleted() {
            // Refresh detail after start/stop
            loadDetail()
        }

        function onRequestFailed(endpoint, statusCode, errorMsg) {
            if (endpoint.indexOf("workspace") >= 0) {
                workspaceDetailPage.errorMessage = errorMsg
            }
        }
    }

    // -- Background fill ----------------------------------------------------
    Rectangle {
        anchors.fill: parent
        color: CoderTheme.background
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // ---- Top bar: back + title ----
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: topBarRow.implicitHeight + 16
            radius: CoderTheme.radius
            color: CoderTheme.surface
            border.color: CoderTheme.divider
            border.width: 1

            RowLayout {
                id: topBarRow
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                CoderButton {
                    text: "← Back"
                    variant: "subtle"
                    onClicked: workspaceDetailPage.backClicked()
                }

                Label {
                    text: workspaceDetailPage.workspaceName
                    font.pixelSize: 20
                    font.bold: true
                    color: CoderTheme.textPrimary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        // ---- Error banner ----
        Rectangle {
            Layout.fillWidth: true
            height: detailErrorLabel.implicitHeight + 16
            radius: CoderTheme.radiusSm
            color: CoderTheme.errorSurface
            border.color: CoderTheme.error
            border.width: 1
            visible: workspaceDetailPage.errorMessage.length > 0

            Label {
                id: detailErrorLabel
                anchors.fill: parent
                anchors.margins: 8
                text: workspaceDetailPage.errorMessage
                color: CoderTheme.error
                wrapMode: Text.WordWrap
                font.pixelSize: 13
            }
        }

        // ---- Header section: owner · template + status ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: "@" + workspaceDetailPage.workspaceOwner + " · " + workspaceDetailPage.workspaceTemplate
                    font.pixelSize: 13
                    color: CoderTheme.textSecondary
                }
            }
        }

        // ---- Scrollable content ----
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: scrollContent.implicitHeight
            clip: true
            flickableDirection: Flickable.VerticalFlick

            ColumnLayout {
                id: scrollContent
                width: parent.width
                spacing: 16

                // ---- Status card ----
                Rectangle {
                    Layout.fillWidth: true
                    height: wsStatusLayout.implicitHeight + 24
                    radius: CoderTheme.radius
                    color: CoderTheme.statusSurface(workspaceDetailPage.workspaceStatus)
                    border.color: CoderTheme.statusColor(workspaceDetailPage.workspaceStatus)
                    border.width: 1

                    ColumnLayout {
                        id: wsStatusLayout
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4

                        RowLayout {
                            spacing: 8

                            StatusChip {
                                status: workspaceDetailPage.workspaceStatus
                            }

                            // Outdated badge
                            Rectangle {
                                visible: workspaceDetailPage.workspaceOutdated
                                width: wsOutdatedBadge.implicitWidth + 12
                                height: wsOutdatedBadge.implicitHeight + 6
                                radius: height / 2
                                color: CoderTheme.warningSurface
                                border.color: CoderTheme.warning
                                border.width: 1

                                Label {
                                    id: wsOutdatedBadge
                                    anchors.centerIn: parent
                                    text: "Update available"
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    color: CoderTheme.warning
                                }
                            }

                            // Health indicator
                            Label {
                                visible: workspaceDetailPage.workspaceHealth.length > 0
                                text: workspaceDetailPage.workspaceHealth
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                color: workspaceDetailPage.workspaceHealth === "Healthy"
                                       ? CoderTheme.success : CoderTheme.error
                            }
                        }
                    }
                }

                // ---- Stop confirmation dialog ----
                Dialog {
                    id: stopConfirmDialog
                    title: "Stop workspace?"
                    anchors.centerIn: Overlay.overlay
                    modal: true
                    width: 340
                    standardButtons: Dialog.Ok | Dialog.Cancel
                    Material.accent: CoderTheme.primary

                    Label {
                        width: stopConfirmDialog.availableWidth
                        text: "Are you sure you want to stop \"" + workspaceDetailPage.workspaceName + "\"?"
                        wrapMode: Text.WordWrap
                    }

                    onAccepted: apiClient.stopWorkspace(workspaceDetailPage.workspaceId)
                }

                // ---- Actions row ----
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    CoderButton {
                        text: workspaceDetailPage.workspaceStatus === "Running"
                              ? "Stop" : "Start"
                        variant: workspaceDetailPage.workspaceStatus === "Running"
                                 ? "destructive" : "default"
                        visible: workspaceDetailPage.workspaceStatus === "Running"
                                 || workspaceDetailPage.workspaceStatus === "Stopped"
                        onClicked: {
                            if (workspaceDetailPage.workspaceStatus === "Running")
                                stopConfirmDialog.open()
                            else
                                apiClient.startWorkspace(workspaceDetailPage.workspaceId)
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: workspaceDetailPage.workspaceStatus === "Running"
                                      ? "Stop workspace" : "Start workspace"
                    }

                    CoderButton {
                        text: "Update"
                        variant: "default"
                        visible: workspaceDetailPage.workspaceOutdated
                        onClicked: apiClient.updateWorkspace(workspaceDetailPage.workspaceId)
                        ToolTip.visible: hovered
                        ToolTip.text: "Update to latest template version"
                    }

                    BusyIndicator {
                        width: 28; height: 28
                        running: visible
                        visible: workspaceDetailPage.workspaceStatus === "Starting"
                                 || workspaceDetailPage.workspaceStatus === "Stopping"
                                 || workspaceDetailPage.workspaceStatus === "Canceling"
                                 || workspaceDetailPage.workspaceStatus === "Deleting"
                    }

                    CoderButton {
                        text: "Open in Browser"
                        variant: "outline"
                        onClicked: {
                            var base = sessionManager.currentUrl.replace(/\/+$/, "")
                            Qt.openUrlExternally(
                                base + "/@" + workspaceDetailPage.workspaceOwner
                                + "/" + workspaceDetailPage.workspaceName)
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: "Open workspace in web browser"
                    }
                }

                // ---- Loading indicator ----
                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: detailLoading
                    visible: detailLoading
                }

                // ---- Agents section ----
                Label {
                    text: "Agents"
                    font.pixelSize: 14
                    font.bold: true
                    color: CoderTheme.textPrimary
                    visible: agentsModel.count > 0
                }

                Repeater {
                    model: agentsModel

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: agentLayout.implicitHeight + 24
                        radius: CoderTheme.radius
                        color: CoderTheme.surface
                        border.color: CoderTheme.border
                        border.width: 1

                        ColumnLayout {
                            id: agentLayout
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 4

                            RowLayout {
                                spacing: 8

                                // Agent status dot
                                Rectangle {
                                    width: 10; height: 10; radius: 5
                                    color: {
                                        var s = model.agentStatus
                                        if (s === "connected")    return CoderTheme.success
                                        if (s === "disconnected") return CoderTheme.error
                                        if (s === "connecting")   return CoderTheme.warning
                                        return CoderTheme.textDisabled
                                    }
                                }

                                Label {
                                    text: model.agentName
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: CoderTheme.textPrimary
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: model.agentStatus
                                    font.pixelSize: 12
                                    color: CoderTheme.textSecondary
                                }
                            }

                            // Hostname info
                            Label {
                                text: model.agentName + "." + workspaceDetailPage.workspaceName
                                      + "." + workspaceDetailPage.workspaceOwner + ".coder"
                                font.pixelSize: 11
                                font.family: "monospace"
                                color: CoderTheme.textDisabled
                            }

                            // OS / Arch info
                            Label {
                                visible: model.agentOs.length > 0
                                text: model.agentOs + (model.agentArch.length > 0
                                                       ? " / " + model.agentArch : "")
                                font.pixelSize: 11
                                color: CoderTheme.textDisabled
                            }

                            // Terminal button — only shown when web_terminal is in display_apps
                            CoderButton {
                                text: "Terminal"
                                variant: "outline"
                                visible: model.agentDisplayApps.length === 0 || ("," + model.agentDisplayApps + ",").indexOf(",web_terminal,") >= 0
                                enabled: model.agentStatus === "connected"
                                font.pixelSize: 12
                                onClicked: {
                                    workspaceDetailPage.selectedTerminalAgentId = model.agentId
                                    workspaceDetailPage.selectedTerminalAgentName = model.agentName
                                }
                            }

                            // Browse Files button
                            CoderButton {
                                text: "Browse Files"
                                variant: "outline"
                                enabled: model.agentStatus === "connected"
                                         && vpnBridge.state === "connected"
                                font.pixelSize: 12
                                onClicked: {
                                    workspaceDetailPage.selectedFileBrowserAgentHost =
                                        model.agentName + "." + workspaceDetailPage.workspaceName
                                            + "." + workspaceDetailPage.workspaceOwner + ".coder"
                                    workspaceDetailPage.selectedFileBrowserWorkspaceName =
                                        workspaceDetailPage.workspaceName
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: {
                                    if (vpnBridge.state !== "connected")
                                        return "Connect to VPN to browse files"
                                    if (model.agentStatus !== "connected")
                                        return "Agent is not connected"
                                    return "Browse workspace file system"
                                }
                            }

                            // ---- Inline apps for this agent ----
                            AgentAppsGrid {
                                Layout.fillWidth: true
                                appsList: model.agentApps ? JSON.parse(model.agentApps) : []
                                agentId: model.agentId
                                agentName: model.agentName
                                onAppClicked: (appData) => handleAppClick(appData, model.agentId, model.agentName, model.agentDirectory || "")
                                onOpenInBrowser: (appData) => handleOpenInBrowser(appData, model.agentName)
                            }

                            // ---- Dev container children ----
                            Repeater {
                                model: {
                                    try { return JSON.parse(childAgents) }
                                    catch(e) { return [] }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: childLayout.implicitHeight + 20
                                    Layout.topMargin: 8
                                    radius: CoderTheme.radius
                                    color: Qt.rgba(CoderTheme.primary.r, CoderTheme.primary.g,
                                                   CoderTheme.primary.b, 0.05)
                                    border.color: CoderTheme.primary
                                    border.width: 1

                                    ColumnLayout {
                                        id: childLayout
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 4

                                        Label {
                                            text: "📦 Dev Container"
                                            font.pixelSize: 11
                                            font.bold: true
                                            color: CoderTheme.primary
                                        }

                                        // Status row
                                        RowLayout {
                                            spacing: 8
                                            Rectangle {
                                                width: 10; height: 10; radius: 5
                                                color: {
                                                    var s = modelData.status
                                                    if (s === "connected") return CoderTheme.success
                                                    if (s === "disconnected") return CoderTheme.error
                                                    if (s === "connecting") return CoderTheme.warning
                                                    return CoderTheme.textDisabled
                                                }
                                            }
                                            Label {
                                                text: modelData.name
                                                font.pixelSize: 13; font.bold: true
                                                color: CoderTheme.textPrimary
                                                Layout.fillWidth: true
                                            }
                                            Label {
                                                text: modelData.status
                                                font.pixelSize: 11
                                                color: CoderTheme.textSecondary
                                            }
                                        }

                                        // Hostname
                                        Label {
                                            text: modelData.name + "." + workspaceDetailPage.workspaceName
                                                  + "." + workspaceDetailPage.workspaceOwner + ".coder"
                                            font.pixelSize: 11; font.family: "monospace"
                                            color: CoderTheme.textDisabled
                                        }

                                        // Terminal button
                                        CoderButton {
                                            text: "Terminal"; variant: "outline"; font.pixelSize: 12
                                            visible: modelData.displayApps.length === 0 ||
                                                     ("," + modelData.displayApps + ",").indexOf(",web_terminal,") >= 0
                                            enabled: modelData.status === "connected"
                                            onClicked: {
                                                workspaceDetailPage.selectedTerminalAgentId = modelData.id
                                                workspaceDetailPage.selectedTerminalAgentName = modelData.name
                                            }
                                        }

                                        // Browse Files button
                                        CoderButton {
                                            text: "Browse Files"; variant: "outline"; font.pixelSize: 12
                                            enabled: modelData.status === "connected" && vpnBridge.state === "connected"
                                            onClicked: {
                                                workspaceDetailPage.selectedFileBrowserAgentHost =
                                                    modelData.name + "." + workspaceDetailPage.workspaceName
                                                    + "." + workspaceDetailPage.workspaceOwner + ".coder"
                                                workspaceDetailPage.selectedFileBrowserWorkspaceName =
                                                    workspaceDetailPage.workspaceName
                                            }
                                        }

                                        // Child agent's apps
                                        AgentAppsGrid {
                                            Layout.fillWidth: true
                                            appsList: {
                                                try { return JSON.parse(modelData.apps) }
                                                catch(e) { return [] }
                                            }
                                            agentId: modelData.id
                                            agentName: modelData.name
                                            onAppClicked: (appData) => handleAppClick(appData, modelData.id, modelData.name, modelData.directory || "")
                                            onOpenInBrowser: (appData) => handleOpenInBrowser(appData, modelData.name)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ---- Info section ----
                Label {
                    text: "Information"
                    font.pixelSize: 14
                    font.bold: true
                    color: CoderTheme.textPrimary
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: infoLayout.implicitHeight + 24
                    radius: CoderTheme.radius
                    color: CoderTheme.surface
                    border.color: CoderTheme.border
                    border.width: 1

                    ColumnLayout {
                        id: infoLayout
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4

                        Label {
                            text: "ID: " + workspaceDetailPage.workspaceId
                            font.pixelSize: 12
                            font.family: "monospace"
                            color: CoderTheme.textSecondary
                        }

                        Label {
                            text: "Agents: " + agentsModel.count
                            font.pixelSize: 12
                            color: CoderTheme.textSecondary
                        }

                        Label {
                            text: "Template: " + workspaceDetailPage.workspaceTemplate
                            font.pixelSize: 12
                            color: CoderTheme.textSecondary
                        }
                    }
                }

                // Bottom spacer
                Item { Layout.fillHeight: true }
            }
        }
    }

    // ---- In-app browser overlay ----
    Loader {
        id: appBrowserLoader
        anchors.fill: parent
        active: workspaceDetailPage.selectedAppSlug.length > 0
        z: 10
        source: "AppBrowserPage.qml"
        onLoaded: {
            item.deploymentUrl = Qt.binding(function() { return sessionManager.currentUrl; });
            item.appSlug = Qt.binding(function() { return workspaceDetailPage.selectedAppSlug; });
            item.appUrl = Qt.binding(function() { return workspaceDetailPage.selectedAppUrl; });
            item.workspaceName = Qt.binding(function() { return workspaceDetailPage.workspaceName; });
            item.ownerName = Qt.binding(function() { return workspaceDetailPage.workspaceOwner; });
            item.agentName = Qt.binding(function() { return workspaceDetailPage.selectedAgentName; });
            item.vpnActive = Qt.binding(function() {
                return vpnBridge.isRunning;
            });
            item.isExternal = Qt.binding(function() { return workspaceDetailPage.selectedAppExternal; });
            item.sessionToken = Qt.binding(function() { return sessionManager.sessionToken(); });
            item.closeRequested.connect(function() { workspaceDetailPage.selectedAppSlug = ""; });
        }
    }

    // ---- In-app terminal overlay ----
    Loader {
        id: terminalLoader
        anchors.fill: parent
        active: workspaceDetailPage.selectedTerminalAgentId.length > 0
        z: 11
        source: "TerminalPage.qml"
        onLoaded: {
            item.agentId = Qt.binding(function() { return workspaceDetailPage.selectedTerminalAgentId; });
            item.agentName = Qt.binding(function() { return workspaceDetailPage.selectedTerminalAgentName; });
            item.workspaceName = Qt.binding(function() { return workspaceDetailPage.workspaceName; });
            item.closeRequested.connect(function() { workspaceDetailPage.selectedTerminalAgentId = ""; });
        }
    }

    // ---- File browser overlay ----
    Loader {
        id: fileBrowserLoader
        anchors.fill: parent
        active: workspaceDetailPage.selectedFileBrowserAgentHost.length > 0
        z: 12
        source: "FileBrowserPage.qml"
        onLoaded: {
            item.agentHostname = Qt.binding(function() { return workspaceDetailPage.selectedFileBrowserAgentHost; });
            item.workspaceName = Qt.binding(function() { return workspaceDetailPage.selectedFileBrowserWorkspaceName; });
            item.backClicked.connect(function() { workspaceDetailPage.selectedFileBrowserAgentHost = ""; });
        }
    }

    // ---- VS Code launch mode dialog ----
    VsCodeLaunchDialog {
        id: vscodeLaunchDialog

        onLaunchNormal: {
            // Use vscode-remote SSH URI to connect via VPN's SSH config directly,
            // avoiding SSH config conflict with the Coder extension's ProxyCommand.
            var scheme = vscodeLaunchDialog.displayAppType === "vscode_insiders"
                ? "vscode-insiders" : "vscode"
            var hostname = vscodeLaunchDialog.agentName + "."
                + workspaceDetailPage.workspaceName + "."
                + workspaceDetailPage.workspaceOwner + ".coder"
            var folder = vscodeLaunchDialog.agentDirectory
            if (folder.length === 0)
                folder = "/home/" + workspaceDetailPage.workspaceOwner
            var uri = scheme + "://vscode-remote/ssh-remote+" + hostname + folder
            Qt.openUrlExternally(uri)
        }

        onLaunchSecure: {
            // Launch VS Code in DLP sandbox using direct SSH remote via VPN.
            var binary = vscodeLaunchDialog.displayAppType === "vscode_insiders"
                ? "code-insiders" : "code"
            var hostname = vscodeLaunchDialog.agentName + "."
                + workspaceDetailPage.workspaceName + "."
                + workspaceDetailPage.workspaceOwner + ".coder"
            var folder = vscodeLaunchDialog.agentDirectory
            if (folder.length === 0)
                folder = "/home/" + workspaceDetailPage.workspaceOwner
            var command = binary + " --remote ssh-remote+" + hostname + " " + folder
            var appName = vscodeLaunchDialog.displayAppType === "vscode_insiders"
                ? "VS Code Insiders" : "VS Code Desktop"
            dlpCompositor.launchApp(command, appName, "", false, false, false, false, false,
                                    [], settingsManager.dlpDbusFilter, settingsManager.dlpDbusAllowedNames)
        }
    }

    // ---- SSH helper dialog ----
    SshHelperDialog {
        id: sshHelperDialog
    }

    // ---- Port forwarding dialog ----
    PortForwardDialog {
        id: portForwardDialog
    }

}
