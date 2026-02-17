import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

// Task list page — shows all AI tasks from taskModel with status badges,
// state indicators, and initial prompt previews.
Item {
    id: tasksPage

    property string filterText: ""

    // Set this to a task ID to show the detail page.
    property string selectedTaskId: ""
    property string selectedTaskName: ""
    property string selectedTaskStatus: ""
    property string selectedTaskStateMessage: ""
    property string selectedTaskPrompt: ""
    property string selectedTaskOwner: ""
    property string selectedTaskTemplate: ""

    // Map task statuses to StatusChip-compatible workspace statuses
    function taskStatusToChipStatus(s) {
        if (s === "Active")       return "Running"
        if (s === "Initializing") return "Starting"
        if (s === "Error")        return "Failed"
        if (s === "Paused")       return "Stopping"
        return "Stopped"  // Pending, Complete, etc.
    }

    // Map task status to CoderTheme color
    function taskStatusColor(s) {
        if (s === "Active")       return CoderTheme.success
        if (s === "Paused")       return CoderTheme.warning
        if (s === "Error")        return CoderTheme.error
        if (s === "Initializing") return CoderTheme.info
        return CoderTheme.textDisabled
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
                placeholderText: "Search tasks…"
                Layout.fillWidth: true
                selectByMouse: true
                onTextChanged: tasksPage.filterText = text.toLowerCase()
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
                enabled: !taskModel.loading
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

        // ---- Error banner ----
        Rectangle {
            Layout.fillWidth: true
            height: taskErrorLabel.implicitHeight + 16
            radius: CoderTheme.radiusSm
            color: CoderTheme.errorSurface
            visible: taskModel.errorMessage.length > 0

            Label {
                id: taskErrorLabel
                anchors.fill: parent
                anchors.margins: 8
                text: taskModel.errorMessage
                color: CoderTheme.error
                wrapMode: Text.WordWrap
                font.pixelSize: 13
            }
        }

        // ---- Loading indicator ----
        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: taskModel.loading
            visible: taskModel.loading
        }

        // ---- Task list ----
        ListView {
            id: taskList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            // Conditional model binding: when the page is hidden (e.g. tab
            // switch), set model to null so delegates are destroyed.  When
            // visible again, re-bind to taskModel — ListView queries
            // rowCount() and creates fresh delegates.  This works around a
            // Qt issue where delegates destroyed by visibility changes are
            // not reliably recreated from dataChanged signals alone.
            model: tasksPage.visible ? taskModel : null

            delegate: Rectangle {
                width: taskList.width
                radius: CoderTheme.radius
                color: CoderTheme.surface
                border.color: CoderTheme.border
                border.width: 1

                // Tap area to open detail — outside the layout to avoid
                // "anchors on an item managed by a layout" warnings.
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        tasksPage.selectedTaskId = model.id
                        tasksPage.selectedTaskName = model.displayName || model.name
                        tasksPage.selectedTaskStatus = model.statusString
                        tasksPage.selectedTaskStateMessage = model.currentStateMessage
                        tasksPage.selectedTaskPrompt = model.initialPrompt
                        tasksPage.selectedTaskOwner = model.ownerName
                        tasksPage.selectedTaskTemplate = model.templateName
                    }
                    onContainsMouseChanged: {
                        parent.color = containsMouse ? CoderTheme.hoverBg : CoderTheme.surface
                    }
                }

                RowLayout {
                    id: delegateLayout
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    // Hide filtered-out items
                    visible: tasksPage.filterText.length === 0
                             || model.name.toLowerCase().indexOf(tasksPage.filterText) >= 0
                             || model.displayName.toLowerCase().indexOf(tasksPage.filterText) >= 0
                             || model.initialPrompt.toLowerCase().indexOf(tasksPage.filterText) >= 0

                    // Task info
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: model.displayName || model.name
                            font.pixelSize: 15
                            font.bold: true
                            color: CoderTheme.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 8
                            Label {
                                text: model.templateName
                                font.pixelSize: 12
                                color: CoderTheme.textSecondary
                            }
                            Label {
                                text: "·"
                                font.pixelSize: 12
                                color: CoderTheme.textDisabled
                            }
                            Label {
                                text: model.workspaceName
                                font.pixelSize: 12
                                color: CoderTheme.textSecondary
                            }
                        }

                        // Initial prompt preview
                        Label {
                            text: model.initialPrompt
                            font.pixelSize: 12
                            color: CoderTheme.textSecondary
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            Layout.fillWidth: true
                            visible: model.initialPrompt.length > 0
                        }

                        // Status + state indicator row
                        RowLayout {
                            spacing: 6

                            StatusChip {
                                status: tasksPage.taskStatusToChipStatus(model.statusString)
                            }

                            // State indicator
                            Label {
                                font.pixelSize: 11
                                text: {
                                    // currentState is an int matching TaskState enum:
                                    // 0=Working, 1=Idle, 2=Complete, 3=Failed
                                    var st = model.currentState
                                    if (st === 0) return "⟳ Working"
                                    if (st === 2) return "✓ Complete"
                                    if (st === 3) return "✗ Failed"
                                    return "— Idle"
                                }
                                color: {
                                    var st = model.currentState
                                    if (st === 0) return CoderTheme.info
                                    if (st === 2) return CoderTheme.success
                                    if (st === 3) return CoderTheme.error
                                    return CoderTheme.textSecondary
                                }
                            }

                            // State message
                            Label {
                                text: model.currentStateMessage
                                font.pixelSize: 10
                                color: CoderTheme.textSecondary
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                visible: model.currentStateMessage.length > 0
                            }
                        }

                        // Timestamp
                        Label {
                            text: model.createdAt ? Qt.formatDateTime(model.createdAt, "yyyy-MM-dd hh:mm") : ""
                            font.pixelSize: 10
                            color: CoderTheme.textDisabled
                            visible: text.length > 0
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
                visible: taskModel.count === 0 && !taskModel.loading
                text: "No AI tasks found"
                font.pixelSize: 16
                color: CoderTheme.textSecondary
            }
        }
    }

    // ---- Detail page loader ----
    Loader {
        id: detailLoader
        anchors.fill: parent
        active: tasksPage.selectedTaskId.length > 0
        sourceComponent: TaskDetailPage {
            taskId: tasksPage.selectedTaskId
            taskName: tasksPage.selectedTaskName
            taskStatus: tasksPage.selectedTaskStatus
            taskStateMessage: tasksPage.selectedTaskStateMessage
            taskPrompt: tasksPage.selectedTaskPrompt
            taskOwner: tasksPage.selectedTaskOwner
            taskTemplate: tasksPage.selectedTaskTemplate
            onBackClicked: tasksPage.selectedTaskId = ""
        }
    }
}
