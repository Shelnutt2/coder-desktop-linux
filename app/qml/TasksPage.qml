import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

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
            }

            Button {
                text: "Refresh"
                icon.name: "view-refresh"
                enabled: !taskModel.loading
                onClicked: pollingController.refreshNow()
            }

            Label {
                text: "Auto-refreshing every " + pollingController.refreshIntervalSec + "s"
                font.pixelSize: 10
                opacity: 0.4
                visible: pollingController.polling
                Layout.alignment: Qt.AlignVCenter
            }
        }

        // ---- Error banner ----
        Rectangle {
            Layout.fillWidth: true
            height: taskErrorLabel.implicitHeight + 16
            radius: 4
            color: Material.color(Material.Red, Material.Shade50)
            visible: taskModel.errorMessage.length > 0

            Label {
                id: taskErrorLabel
                anchors.fill: parent
                anchors.margins: 8
                text: taskModel.errorMessage
                color: Material.color(Material.Red)
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
            model: taskModel

            delegate: Rectangle {
                width: taskList.width
                radius: 8
                color: Material.background
                border.color: Material.dividerColor
                border.width: 1

                // Tap area to open detail — outside the layout to avoid
                // "anchors on an item managed by a layout" warnings.
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        tasksPage.selectedTaskId = model.id
                        tasksPage.selectedTaskName = model.displayName || model.name
                        tasksPage.selectedTaskStatus = model.statusString
                        tasksPage.selectedTaskStateMessage = model.currentStateMessage
                        tasksPage.selectedTaskPrompt = model.initialPrompt
                        tasksPage.selectedTaskOwner = model.ownerName
                        tasksPage.selectedTaskTemplate = model.templateName
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

                    // Status indicator dot
                    Rectangle {
                        width: 12; height: 12; radius: 6
                        color: {
                            var s = model.statusString
                            if (s === "Active")       return Material.color(Material.Green)
                            if (s === "Paused")       return Material.color(Material.Orange)
                            if (s === "Error")        return Material.color(Material.Red)
                            if (s === "Initializing") return Material.color(Material.Blue)
                            if (s === "Pending")      return Material.color(Material.Grey)
                            return Material.color(Material.Grey)
                        }
                        Layout.alignment: Qt.AlignVCenter
                    }

                    // Task info
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: model.displayName || model.name
                            font.pixelSize: 15
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 8
                            Label {
                                text: model.templateName
                                font.pixelSize: 12
                                opacity: 0.6
                            }
                            Label {
                                text: "·"
                                font.pixelSize: 12
                                opacity: 0.4
                            }
                            Label {
                                text: model.workspaceName
                                font.pixelSize: 12
                                opacity: 0.6
                            }
                        }

                        // Initial prompt preview
                        Label {
                            text: model.initialPrompt
                            font.pixelSize: 12
                            opacity: 0.5
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            Layout.fillWidth: true
                            visible: model.initialPrompt.length > 0
                        }

                        // Status + state indicator row
                        RowLayout {
                            spacing: 6

                            Label {
                                text: model.statusString
                                font.pixelSize: 11
                                color: {
                                    var s = model.statusString
                                    if (s === "Active") return Material.color(Material.Green)
                                    if (s === "Error")  return Material.color(Material.Red)
                                    return Material.foreground
                                }
                            }

                            // State indicator
                            Label {
                                font.pixelSize: 11
                                opacity: 0.7
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
                                    if (st === 0) return Material.color(Material.Blue)
                                    if (st === 2) return Material.color(Material.Green)
                                    if (st === 3) return Material.color(Material.Red)
                                    return Material.foreground
                                }
                            }

                            // State message
                            Label {
                                text: model.currentStateMessage
                                font.pixelSize: 10
                                opacity: 0.5
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                visible: model.currentStateMessage.length > 0
                            }
                        }

                        // Timestamp
                        Label {
                            text: model.createdAt ? Qt.formatDateTime(model.createdAt, "yyyy-MM-dd hh:mm") : ""
                            font.pixelSize: 10
                            opacity: 0.4
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
                opacity: 0.5
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
