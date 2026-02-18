import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

// Workspace list page — shows all workspaces from workspaceModel with
// status chips, actions (start/stop), filter chips, and a search filter.
// Themed with CoderTheme singleton for consistent light/dark support.
Item {
    id: workspacesPage

    property string filterText: ""
    property string statusFilter: ""  // "", "Running", "Stopped", "Failed"

    // Set this to a workspace ID to show the detail page.
    property string selectedWorkspaceId: ""
    property string selectedWorkspaceName: ""
    property string selectedWorkspaceOwner: ""
    property string selectedWorkspaceTemplate: ""
    property string selectedWorkspaceStatus: ""
    property bool   selectedWorkspaceOutdated: false
    property string selectedWorkspaceHealth: ""

    Rectangle {
        anchors.fill: parent
        color: CoderTheme.background
    }

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
                color: CoderTheme.textPrimary
                placeholderTextColor: CoderTheme.textDisabled
                onTextChanged: workspacesPage.filterText = text.toLowerCase()

                background: Rectangle {
                    implicitHeight: 36
                    radius: CoderTheme.radius
                    color: CoderTheme.surface
                    border.color: searchField.activeFocus ? CoderTheme.primary : CoderTheme.border
                    border.width: 1
                }
            }

            CoderButton {
                text: "Refresh"
                variant: "outline"
                enabled: !workspaceModel.loading
                onClicked: pollingController.refreshNow()
            }

            Label {
                text: "Auto-refreshing every " + pollingController.refreshIntervalSec + "s"
                font.pixelSize: 10
                color: CoderTheme.textDisabled
                visible: pollingController.polling
                Layout.alignment: Qt.AlignVCenter
            }
        }

        // ---- Filter chips ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: [
                    { label: "All",     value: "" },
                    { label: "Running", value: "Running" },
                    { label: "Stopped", value: "Stopped" },
                    { label: "Failed",  value: "Failed" }
                ]

                Rectangle {
                    required property var modelData
                    width: chipLabel.implicitWidth + 20
                    height: chipLabel.implicitHeight + 10
                    radius: height / 2
                    color: workspacesPage.statusFilter === modelData.value
                           ? CoderTheme.primary : CoderTheme.surface
                    border.color: workspacesPage.statusFilter === modelData.value
                                  ? CoderTheme.primary : CoderTheme.border
                    border.width: 1

                    Label {
                        id: chipLabel
                        anchors.centerIn: parent
                        text: modelData.label
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: workspacesPage.statusFilter === modelData.value
                               ? CoderTheme.textInvert : CoderTheme.textSecondary
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: workspacesPage.statusFilter = modelData.value
                    }
                }
            }
        }

        // ---- Error banner with retry ----
        Rectangle {
            Layout.fillWidth: true
            height: wsErrorRow.implicitHeight + 16
            radius: CoderTheme.radiusSm
            color: CoderTheme.errorSurface
            border.color: CoderTheme.error
            border.width: 1
            visible: workspaceModel.errorMessage.length > 0

            RowLayout {
                id: wsErrorRow
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Label {
                    id: wsErrorLabel
                    text: workspaceModel.errorMessage
                    color: CoderTheme.error
                    wrapMode: Text.WordWrap
                    font.pixelSize: 13
                    Layout.fillWidth: true
                }

                CoderButton {
                    text: "Retry"
                    variant: "outline"
                    onClicked: pollingController.refreshNow()
                }
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
            // Conditional model binding: when the page is hidden (e.g. tab
            // switch), set model to null so delegates are destroyed.  When
            // visible again, re-bind to workspaceModel — ListView queries
            // rowCount() and creates fresh delegates.  This works around a
            // Qt issue where delegates destroyed by visibility changes are
            // not reliably recreated from dataChanged signals alone.
            model: workspacesPage.visible ? workspaceModel : null

            delegate: Rectangle {
                id: delegateCard
                width: workspaceList.width
                radius: CoderTheme.radius
                color: delegateMouseArea.containsMouse ? CoderTheme.hoverBg : CoderTheme.surface
                border.color: CoderTheme.border
                clip: true

                // Tap area to open detail — outside the layout to avoid
                // "anchors on an item managed by a layout" warnings.
                MouseArea {
                    id: delegateMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
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

                    // Hide filtered-out items (text filter + status filter)
                    visible: {
                        var textMatch = workspacesPage.filterText.length === 0
                                     || model.name.toLowerCase().indexOf(workspacesPage.filterText) >= 0
                                     || model.ownerName.toLowerCase().indexOf(workspacesPage.filterText) >= 0
                        var statusMatch = workspacesPage.statusFilter.length === 0
                                       || model.statusString === workspacesPage.statusFilter
                        return textMatch && statusMatch
                    }

                    // Workspace info
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: model.name
                            font.pixelSize: 15
                            font.bold: true
                            color: CoderTheme.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Label {
                            text: "@" + model.ownerName + " · " + model.templateName
                            font.pixelSize: 12
                            color: CoderTheme.textSecondary
                        }

                        // Status chip + outdated badge
                        RowLayout {
                            spacing: 6

                            StatusChip {
                                status: model.statusString
                            }

                            Rectangle {
                                visible: model.outdated
                                width: outdatedLabel.implicitWidth + 12
                                height: outdatedLabel.implicitHeight + 6
                                radius: height / 2
                                color: CoderTheme.warningSurface
                                border.color: CoderTheme.warning
                                border.width: 1

                                Label {
                                    id: outdatedLabel
                                    anchors.centerIn: parent
                                    text: "Update available"
                                    font.pixelSize: 10
                                    font.weight: Font.Medium
                                    color: CoderTheme.warning
                                }
                            }
                        }
                    }

                    // Action buttons
                    RowLayout {
                        spacing: 4
                        Layout.alignment: Qt.AlignVCenter

                        // Update & Start — shown for outdated+stopped workspaces
                        CoderButton {
                            text: "Update & Start"
                            variant: "default"
                            visible: model.outdated
                                     && model.statusString === "Stopped"
                            onClicked: {
                                apiClient.updateWorkspace(model.id)
                                apiClient.startWorkspace(model.id)
                            }
                        }

                        // Start / Stop toggle
                        CoderButton {
                            text: model.statusString === "Running" ? "Stop" : "Start"
                            variant: model.statusString === "Running" ? "destructive" : "default"
                            enabled: model.statusString === "Running"
                                     || model.statusString === "Stopped"
                            visible: !(model.outdated
                                       && model.statusString === "Stopped")
                                     && (model.statusString === "Running"
                                         || model.statusString === "Stopped")
                            onClicked: {
                                if (model.statusString === "Running")
                                    apiClient.stopWorkspace(model.id)
                                else
                                    apiClient.startWorkspace(model.id)
                            }
                        }

                        // Transitional busy indicator
                        BusyIndicator {
                            width: 28; height: 28
                            running: visible
                            visible: model.statusString === "Starting"
                                     || model.statusString === "Stopping"
                                     || model.statusString === "Canceling"
                                     || model.statusString === "Deleting"
                        }

                        // Overflow menu
                        Button {
                            flat: true
                            text: "⋮"
                            font.pixelSize: 18
                            onClicked: overflowMenu.open()

                            contentItem: Text {
                                text: "⋮"
                                font.pixelSize: 18
                                color: CoderTheme.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                implicitWidth: 36; implicitHeight: 36
                                radius: CoderTheme.radius
                                color: parent.hovered ? CoderTheme.hoverBg : "transparent"
                            }

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
                                    Material.foreground: CoderTheme.error
                                }
                            }
                        }
                    }
                }

                // Collapse the delegate completely when filtered out.
                // Setting height: 0 + clip hides content; border.width: 0
                // avoids a 1px line for collapsed delegates.
                height: delegateLayout.visible ? delegateLayout.implicitHeight + 24 : 0
                visible: delegateLayout.visible
                border.width: delegateLayout.visible ? 1 : 0
            }

            // ---- Empty state ----
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                visible: workspaceModel.count === 0 && !workspaceModel.loading
                opacity: 0.5

                Label {
                    text: "No workspaces"
                    font.pixelSize: 16
                    color: CoderTheme.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                }

                Label {
                    text: "Create a workspace to get started"
                    font.pixelSize: 13
                    color: CoderTheme.textDisabled
                    Layout.alignment: Qt.AlignHCenter
                }

                CoderButton {
                    text: "Create Workspace"
                    variant: "default"
                    Layout.alignment: Qt.AlignHCenter
                    opacity: 1.0 / 0.5  // counteract parent opacity for button
                    onClicked: {
                        var base = sessionManager.currentUrl.replace(/\/+$/, "")
                        Qt.openUrlExternally(base + "/workspaces")
                    }
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
