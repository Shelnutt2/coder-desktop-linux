import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

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

    // -- In-app browser state ------------------------------------------------
    property string selectedAppSlug: ""
    property string selectedAppUrl: ""
    property string selectedAppName: ""
    property string selectedAgentId: ""
    property string selectedAgentName: ""

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
                            agentName:  agent["name"] || ""
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
    }

    // -- Background fill ----------------------------------------------------
    Rectangle {
        anchors.fill: parent
        color: Material.background
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // ---- Back button + title ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                flat: true
                text: "← Back"
                onClicked: workspaceDetailPage.backClicked()
            }

            Label {
                text: workspaceDetailPage.workspaceName
                font.pixelSize: 20
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        // ---- Subtitle: owner · template ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Label {
                text: workspaceDetailPage.workspaceOwner
                font.pixelSize: 13
                opacity: 0.6
            }

            Label {
                text: "·"
                font.pixelSize: 13
                opacity: 0.4
            }

            Label {
                text: workspaceDetailPage.workspaceTemplate
                font.pixelSize: 13
                opacity: 0.6
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
                    radius: 8
                    color: {
                        var s = workspaceDetailPage.workspaceStatus
                        if (s === "Running")  return Qt.rgba(0.0, 0.7, 0.0, 0.08)
                        if (s === "Stopped")  return Qt.rgba(0.5, 0.5, 0.5, 0.08)
                        if (s === "Failed")   return Qt.rgba(1.0, 0.0, 0.0, 0.08)
                        return Qt.rgba(1.0, 0.6, 0.0, 0.08)
                    }
                    border.color: {
                        var s = workspaceDetailPage.workspaceStatus
                        if (s === "Running")  return Material.color(Material.Green)
                        if (s === "Stopped")  return Material.color(Material.Grey)
                        if (s === "Failed")   return Material.color(Material.Red)
                        return Material.color(Material.Orange)
                    }
                    border.width: 1

                    ColumnLayout {
                        id: wsStatusLayout
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4

                        RowLayout {
                            spacing: 8

                            // Status badge
                            Rectangle {
                                width: wsStatusBadgeLabel.implicitWidth + 16
                                height: wsStatusBadgeLabel.implicitHeight + 8
                                radius: 4
                                color: {
                                    var s = workspaceDetailPage.workspaceStatus
                                    if (s === "Running")  return Material.color(Material.Green)
                                    if (s === "Stopped")  return Material.color(Material.Grey)
                                    if (s === "Failed")   return Material.color(Material.Red)
                                    return Material.color(Material.Orange)
                                }

                                Label {
                                    id: wsStatusBadgeLabel
                                    anchors.centerIn: parent
                                    text: workspaceDetailPage.workspaceStatus
                                    font.pixelSize: 12
                                    font.bold: true
                                    color: "white"
                                }
                            }

                            // Outdated badge
                            Rectangle {
                                visible: workspaceDetailPage.workspaceOutdated
                                width: wsOutdatedBadge.implicitWidth + 12
                                height: wsOutdatedBadge.implicitHeight + 6
                                radius: 4
                                color: Material.color(Material.Orange, Material.Shade100)

                                Label {
                                    id: wsOutdatedBadge
                                    anchors.centerIn: parent
                                    text: "Update available"
                                    font.pixelSize: 11
                                    color: Material.color(Material.Orange, Material.Shade900)
                                }
                            }

                            // Health indicator
                            Label {
                                visible: workspaceDetailPage.workspaceHealth.length > 0
                                text: workspaceDetailPage.workspaceHealth
                                font.pixelSize: 12
                                opacity: 0.7
                                color: workspaceDetailPage.workspaceHealth === "Healthy"
                                       ? Material.color(Material.Green)
                                       : Material.color(Material.Red)
                            }
                        }
                    }
                }

                // ---- Actions row ----
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        text: workspaceDetailPage.workspaceStatus === "Running"
                              ? "Stop" : "Start"
                        enabled: workspaceDetailPage.workspaceStatus === "Running"
                                 || workspaceDetailPage.workspaceStatus === "Stopped"
                        onClicked: {
                            if (workspaceDetailPage.workspaceStatus === "Running")
                                apiClient.stopWorkspace(workspaceDetailPage.workspaceId)
                            else
                                apiClient.startWorkspace(workspaceDetailPage.workspaceId)
                        }
                    }

                    Button {
                        text: "Open in Browser"
                        flat: true
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
                    visible: agentsModel.count > 0
                }

                Repeater {
                    model: agentsModel

                    Rectangle {
                        Layout.fillWidth: true
                        height: agentLayout.implicitHeight + 24
                        radius: 6
                        color: Material.background
                        border.color: Material.dividerColor
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
                                        if (s === "connected")    return Material.color(Material.Green)
                                        if (s === "disconnected") return Material.color(Material.Red)
                                        if (s === "connecting")   return Material.color(Material.Orange)
                                        return Material.color(Material.Grey)
                                    }
                                }

                                Label {
                                    text: model.agentName
                                    font.pixelSize: 14
                                    font.bold: true
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: model.agentStatus
                                    font.pixelSize: 12
                                    opacity: 0.7
                                }
                            }

                            // Hostname info
                            Label {
                                text: model.agentName + "." + workspaceDetailPage.workspaceName
                                      + "." + workspaceDetailPage.workspaceOwner + ".coder"
                                font.pixelSize: 11
                                font.family: "monospace"
                                opacity: 0.5
                            }

                            // OS / Arch info
                            Label {
                                visible: model.agentOs.length > 0
                                text: model.agentOs + (model.agentArch.length > 0
                                                       ? " / " + model.agentArch : "")
                                font.pixelSize: 11
                                opacity: 0.5
                            }

                            // Terminal button (placeholder)
                            Button {
                                flat: true
                                text: "Terminal"
                                enabled: model.agentStatus === "connected"
                                font.pixelSize: 12
                                // TODO: open terminal session
                            }
                        }
                    }
                }

                // ---- Apps section ----
                Label {
                    text: "Apps"
                    font.pixelSize: 14
                    font.bold: true
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
                            Layout.fillWidth: true
                            height: appItemLayout.implicitHeight + 16
                            radius: 6
                            color: Material.background
                            border.color: Material.dividerColor
                            border.width: 1

                            MouseArea {
                                anchors.fill: parent
                                enabled: model.appUrl.length > 0
                                cursorShape: enabled ? Qt.PointingHandCursor
                                                     : Qt.ArrowCursor
                                onClicked: {
                                    workspaceDetailPage.selectedAppSlug = model.appSlug
                                    workspaceDetailPage.selectedAppUrl = model.appUrl
                                    workspaceDetailPage.selectedAppName = model.appName
                                    workspaceDetailPage.selectedAgentId = model.agentId
                                    workspaceDetailPage.selectedAgentName = model.agentName
                                }
                            }

                            ColumnLayout {
                                id: appItemLayout
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                // Icon placeholder
                                Rectangle {
                                    width: 32; height: 32; radius: 4
                                    color: Material.color(Material.Blue, Material.Shade50)
                                    Layout.alignment: Qt.AlignHCenter

                                    Label {
                                        anchors.centerIn: parent
                                        text: model.appName.charAt(0).toUpperCase()
                                        font.pixelSize: 16
                                        font.bold: true
                                        color: Material.color(Material.Blue)
                                    }
                                }

                                Label {
                                    text: model.appName
                                    font.pixelSize: 12
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

                                Menu {
                                    id: appOverflowMenu
                                    MenuItem {
                                        text: qsTr("Open in Browser")
                                        onTriggered: Qt.openUrlExternally(model.appUrl)
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
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: infoLayout.implicitHeight + 24
                    radius: 6
                    color: Material.background
                    border.color: Material.dividerColor
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
                            opacity: 0.6
                        }

                        Label {
                            text: "Agents: " + agentsModel.count
                            font.pixelSize: 12
                            opacity: 0.6
                        }

                        Label {
                            text: "Apps: " + appsModel.count
                            font.pixelSize: 12
                            opacity: 0.6
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
        sourceComponent: AppBrowserPage {
            deploymentUrl: sessionManager.currentUrl
            agentId: workspaceDetailPage.selectedAgentId
            appSlug: workspaceDetailPage.selectedAppSlug
            workspaceName: workspaceDetailPage.workspaceName
            agentName: workspaceDetailPage.selectedAgentName
            vpnActive: typeof vpnManager !== "undefined" && vpnManager ? vpnManager.connected : false
            sessionToken: sessionManager.sessionToken || ""
            onCloseRequested: workspaceDetailPage.selectedAppSlug = ""
        }
    }
}
