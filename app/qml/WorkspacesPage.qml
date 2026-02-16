import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Workspace list page — shows all workspaces from workspaceModel with
// status badges, actions (start/stop), and a search filter.
Item {
    id: workspacesPage

    property string filterText: ""

    // Set this to a workspace ID to show the detail page.
    property string selectedWorkspaceId: ""
    property string selectedWorkspaceName: ""
    property string selectedWorkspaceOwner: ""
    property string selectedWorkspaceTemplate: ""
    property string selectedWorkspaceStatus: ""
    property bool   selectedWorkspaceOutdated: false
    property string selectedWorkspaceHealth: ""

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // ---- Header row: search + refresh ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: searchField
                placeholderText: "Search workspaces…"
                Layout.fillWidth: true
                selectByMouse: true
                onTextChanged: workspacesPage.filterText = text.toLowerCase()
            }

            Button {
                text: "Refresh"
                icon.name: "view-refresh"
                enabled: !workspaceModel.loading
                onClicked: apiClient.fetchWorkspaces()
            }
        }

        // ---- Error banner ----
        Rectangle {
            Layout.fillWidth: true
            height: wsErrorLabel.implicitHeight + 16
            radius: 4
            color: Material.color(Material.Red, Material.Shade50)
            visible: workspaceModel.errorMessage.length > 0

            Label {
                id: wsErrorLabel
                anchors.fill: parent
                anchors.margins: 8
                text: workspaceModel.errorMessage
                color: Material.color(Material.Red)
                wrapMode: Text.WordWrap
                font.pixelSize: 13
            }
        }

        // ---- Loading indicator ----
        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: workspaceModel.loading
            visible: workspaceModel.loading
        }

        // ---- Workspace list ----
        ListView {
            id: workspaceList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: workspaceModel

            delegate: Rectangle {
                width: workspaceList.width
                radius: 8
                color: Material.background
                border.color: Material.dividerColor
                border.width: 1

                // Tap area to open detail — outside the layout to avoid
                // "anchors on an item managed by a layout" warnings.
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        workspacesPage.selectedWorkspaceId = model.id
                        workspacesPage.selectedWorkspaceName = model.name
                        workspacesPage.selectedWorkspaceOwner = model.ownerName
                        workspacesPage.selectedWorkspaceTemplate = model.templateName
                        workspacesPage.selectedWorkspaceStatus = model.statusString
                        workspacesPage.selectedWorkspaceOutdated = model.outdated
                        workspacesPage.selectedWorkspaceHealth = model.health || ""
                    }
                }

                RowLayout {
                    id: delegateLayout
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    // Hide filtered-out items
                    visible: workspacesPage.filterText.length === 0
                             || model.name.toLowerCase().indexOf(workspacesPage.filterText) >= 0
                             || model.ownerName.toLowerCase().indexOf(workspacesPage.filterText) >= 0

                    // Status indicator dot
                    Rectangle {
                        width: 12; height: 12; radius: 6
                        color: {
                            var s = model.statusString
                            if (s === "Running") return Material.color(Material.Green)
                            if (s === "Stopped") return Material.color(Material.Grey)
                            if (s === "Failed")  return Material.color(Material.Red)
                            // Starting, Stopping, Canceling, Deleting
                            return Material.color(Material.Orange)
                        }
                        Layout.alignment: Qt.AlignVCenter
                    }

                    // Workspace info
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: model.name
                            font.pixelSize: 15
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 8
                            Label {
                                text: model.ownerName
                                font.pixelSize: 12
                                opacity: 0.6
                            }
                            Label {
                                text: "·"
                                font.pixelSize: 12
                                opacity: 0.4
                            }
                            Label {
                                text: model.templateName
                                font.pixelSize: 12
                                opacity: 0.6
                            }
                        }

                        // Status + outdated badge
                        RowLayout {
                            spacing: 6

                            Label {
                                text: model.statusString
                                font.pixelSize: 11
                                color: {
                                    var s = model.statusString
                                    if (s === "Running") return Material.color(Material.Green)
                                    if (s === "Failed")  return Material.color(Material.Red)
                                    return Material.foreground
                                }
                            }

                            Rectangle {
                                visible: model.outdated
                                width: outdatedLabel.implicitWidth + 12
                                height: outdatedLabel.implicitHeight + 4
                                radius: 3
                                color: Material.color(Material.Orange, Material.Shade100)

                                Label {
                                    id: outdatedLabel
                                    anchors.centerIn: parent
                                    text: "Update available"
                                    font.pixelSize: 10
                                    color: Material.color(Material.Orange, Material.Shade900)
                                }
                            }
                        }
                    }

                    // Action buttons
                    RowLayout {
                        spacing: 4
                        Layout.alignment: Qt.AlignVCenter

                        // Update & Start — shown for outdated+stopped workspaces
                        Button {
                            flat: true
                            text: "Update & Start"
                            visible: model.outdated
                                     && model.statusString === "Stopped"
                            onClicked: {
                                apiClient.updateWorkspace(model.id)
                                apiClient.startWorkspace(model.id)
                            }
                        }

                        // Start / Stop toggle
                        Button {
                            flat: true
                            text: model.statusString === "Running" ? "Stop" : "Start"
                            enabled: model.statusString === "Running"
                                     || model.statusString === "Stopped"
                            visible: !(model.outdated
                                       && model.statusString === "Stopped")
                            onClicked: {
                                if (model.statusString === "Running")
                                    apiClient.stopWorkspace(model.id)
                                else
                                    apiClient.startWorkspace(model.id)
                            }
                        }

                        // Overflow menu
                        Button {
                            flat: true
                            text: "⋮"
                            font.pixelSize: 18
                            onClicked: overflowMenu.open()

                            Menu {
                                id: overflowMenu

                                MenuItem {
                                    text: "Open in Browser"
                                    onTriggered: {
                                        var base = sessionManager.currentUrl.replace(/\/+$/, "")
                                        Qt.openUrlExternally(
                                            base + "/@" + model.ownerName + "/" + model.name)
                                    }
                                }
                                MenuItem {
                                    text: "Update"
                                    enabled: model.outdated
                                    onTriggered: apiClient.updateWorkspace(model.id)
                                }
                                MenuSeparator {}
                                MenuItem {
                                    text: "Delete"
                                    onTriggered: apiClient.deleteWorkspace(model.id)
                                    Material.foreground: Material.Red
                                }
                            }
                        }
                    }
                }

                // Hide the whole delegate when filtered out
                height: delegateLayout.visible ? delegateLayout.implicitHeight + 24 : 0
                visible: delegateLayout.visible
            }

            // ---- Empty state ----
            Label {
                anchors.centerIn: parent
                visible: workspaceModel.count === 0 && !workspaceModel.loading
                text: "No workspaces found"
                font.pixelSize: 16
                opacity: 0.5
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.verticalCenter
                anchors.topMargin: 24
                visible: workspaceModel.count === 0 && !workspaceModel.loading
                text: "<a href='#'>Create a workspace in your browser</a>"
                font.pixelSize: 13
                color: Material.accent
                onLinkActivated: {
                    var base = sessionManager.currentUrl.replace(/\/+$/, "")
                    Qt.openUrlExternally(base + "/workspaces")
                }
            }
        }
    }

    // ---- Detail page loader ----
    Loader {
        id: wsDetailLoader
        anchors.fill: parent
        active: workspacesPage.selectedWorkspaceId.length > 0
        sourceComponent: WorkspaceDetailPage {
            workspaceId: workspacesPage.selectedWorkspaceId
            workspaceName: workspacesPage.selectedWorkspaceName
            workspaceOwner: workspacesPage.selectedWorkspaceOwner
            workspaceTemplate: workspacesPage.selectedWorkspaceTemplate
            workspaceStatus: workspacesPage.selectedWorkspaceStatus
            workspaceOutdated: workspacesPage.selectedWorkspaceOutdated
            workspaceHealth: workspacesPage.selectedWorkspaceHealth
            onBackClicked: workspacesPage.selectedWorkspaceId = ""
        }
    }
}
