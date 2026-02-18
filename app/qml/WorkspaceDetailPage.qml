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
    ListModel { id: appsModel }

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
            appsModel.clear()

            // Parse latest_build → resources → agents
            var build = workspace["latest_build"] || {}
            var resources = build["resources"] || []
            for (var r = 0; r < resources.length; r++) {
                var agents = resources[r]["agents"] || []
                for (var a = 0; a < agents.length; a++) {
                    var agent = agents[a]
                    agentsModel.append({
                        agentId:     agent["id"] || "",
                        agentName:   agent["name"] || "",
                        agentStatus: agent["status"] || "unknown",
                        agentOs:     agent["operating_system"] || "",
                        agentArch:   agent["architecture"] || ""
                    })

                    // Collect apps from each agent
                    var agentApps = agent["apps"] || []
                    for (var p = 0; p < agentApps.length; p++) {
                        var app = agentApps[p]
                        appsModel.append({
                            appName:    app["display_name"] || app["slug"] || "",
                            appUrl:     app["url"] || "",
                            appIcon:    app["icon"] || "",
                            appCommand: app["command"] || "",
                            appSlug:    app["slug"] || "",
                            agentId:    agent["id"] || "",
                            agentName:  agent["name"] || "",
                            appSubdomain: app["subdomain"] || false,
                            appExternal:  app["external"] || false
                        })
                    }
                }
            }

            // Update status from fresh data
            var latestStatus = (build["status"] || workspaceStatus)
            // Capitalize first letter to match model convention
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
                                apiClient.stopWorkspace(workspaceDetailPage.workspaceId)
                            else
                                apiClient.startWorkspace(workspaceDetailPage.workspaceId)
                        }
                    }

                    CoderButton {
                        text: "Update"
                        variant: "default"
                        visible: workspaceDetailPage.workspaceOutdated
                        onClicked: apiClient.updateWorkspace(workspaceDetailPage.workspaceId)
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
                        height: agentLayout.implicitHeight + 24
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

                            // Terminal button
                            CoderButton {
                                text: "Terminal"
                                variant: "outline"
                                enabled: model.agentStatus === "connected"
                                font.pixelSize: 12
                                onClicked: {
                                    workspaceDetailPage.selectedTerminalAgentId = model.agentId
                                    workspaceDetailPage.selectedTerminalAgentName = model.agentName
                                }
                            }
                        }
                    }
                }

                // ---- Apps section ----
                Label {
                    text: "Apps"
                    font.pixelSize: 14
                    font.bold: true
                    color: CoderTheme.textPrimary
                    visible: appsModel.count > 0
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    columnSpacing: 8
                    rowSpacing: 8
                    visible: appsModel.count > 0

                    Repeater {
                        model: appsModel

                        Rectangle {
                            id: appCard
                            Layout.fillWidth: true
                            height: appItemLayout.implicitHeight + 16
                            radius: CoderTheme.radius
                            color: appMouseArea.containsMouse ? CoderTheme.hoverBg : CoderTheme.surface
                            border.color: CoderTheme.border
                            border.width: 1

                            MouseArea {
                                id: appMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                enabled: model.appUrl.length > 0
                                cursorShape: enabled ? Qt.PointingHandCursor
                                                     : Qt.ArrowCursor
                                onClicked: {
                                    workspaceDetailPage.selectedAppSlug = model.appSlug
                                    workspaceDetailPage.selectedAppUrl = model.appUrl
                                    workspaceDetailPage.selectedAppName = model.appName
                                    workspaceDetailPage.selectedAgentId = model.agentId
                                    workspaceDetailPage.selectedAgentName = model.agentName
                                    workspaceDetailPage.selectedAppExternal = model.appExternal
                                }
                            }

                            ColumnLayout {
                                id: appItemLayout
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                // App icon — loaded from Coder deployment, with letter fallback
                                Item {
                                    width: 32; height: 32
                                    Layout.alignment: Qt.AlignHCenter

                                    Image {
                                        id: appIconImage
                                        anchors.fill: parent
                                        visible: status === Image.Ready
                                        source: {
                                            if (!model.appIcon || model.appIcon.length === 0)
                                                return "";
                                            // Relative paths (e.g. "/icon/code.svg") need the deployment base URL
                                            if (model.appIcon.startsWith("/")) {
                                                var base = sessionManager.currentUrl.replace(/\/+$/, "");
                                                return base + model.appIcon;
                                            }
                                            return model.appIcon;
                                        }
                                        sourceSize: Qt.size(32, 32)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        cache: true
                                    }

                                    // Fallback: letter initial in a colored rectangle
                                    Rectangle {
                                        anchors.fill: parent
                                        radius: CoderTheme.radiusSm
                                        color: CoderTheme.activeSurface
                                        visible: appIconImage.status !== Image.Ready

                                        Label {
                                            anchors.centerIn: parent
                                            text: model.appName.charAt(0).toUpperCase()
                                            font.pixelSize: 16
                                            font.bold: true
                                            color: CoderTheme.primary
                                        }
                                    }
                                }

                                Label {
                                    text: model.appName
                                    font.pixelSize: 12
                                    color: CoderTheme.textPrimary
                                    elide: Text.ElideRight
                                    horizontalAlignment: Text.AlignHCenter
                                    Layout.fillWidth: true
                                }
                            }

                            // Overflow menu — only shown when external browser is allowed
                            Button {
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.margins: 2
                                flat: true
                                text: "⋮"
                                font.pixelSize: 14
                                z: 2
                                visible: settingsManager.externalBrowserAllowed
                                onClicked: appOverflowMenu.open()

                                contentItem: Text {
                                    text: "⋮"
                                    font.pixelSize: 14
                                    color: CoderTheme.textSecondary
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                background: Rectangle {
                                    implicitWidth: 28; implicitHeight: 28
                                    radius: CoderTheme.radiusSm
                                    color: parent.hovered ? CoderTheme.hoverBg : "transparent"
                                }

                                Menu {
                                    id: appOverflowMenu
                                    MenuItem {
                                        text: qsTr("Open in Browser")
                                        onTriggered: {
                                            var url = appBrowser.buildAppUrl(
                                                sessionManager.currentUrl,
                                                model.appUrl,
                                                model.appSlug,
                                                workspaceDetailPage.workspaceName,
                                                workspaceDetailPage.workspaceOwner,
                                                model.agentName,
                                                vpnBridge.isRunning,
                                                model.appExternal
                                            );
                                            Qt.openUrlExternally(url);
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
                            text: "Apps: " + appsModel.count
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
}
