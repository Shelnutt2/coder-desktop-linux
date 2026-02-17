import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Task detail page — shows task header, status, logs, and message input.
// Shown via a Loader from TasksPage when a task is selected.
Item {
    id: taskDetailPage

    property string taskId: ""
    property string taskName: ""
    property string taskStatus: ""
    property string taskStateMessage: ""
    property string taskPrompt: ""
    property string taskOwner: ""
    property string taskTemplate: ""

    signal backClicked()

    // Map task statuses to StatusChip-compatible workspace statuses
    function taskStatusToChipStatus(s) {
        if (s === "Active")       return "Running"
        if (s === "Initializing") return "Starting"
        if (s === "Error")        return "Failed"
        if (s === "Paused")       return "Stopping"
        return "Stopped"  // Pending, Complete, etc.
    }

    // Map task status to CoderTheme status surface
    function taskStatusSurface(s) {
        if (s === "Active")       return CoderTheme.successSurface
        if (s === "Paused")       return CoderTheme.warningSurface
        if (s === "Error")        return CoderTheme.errorSurface
        if (s === "Initializing") return CoderTheme.activeSurface
        return CoderTheme.surface
    }

    // Map task status to CoderTheme color
    function taskStatusColor(s) {
        if (s === "Active")       return CoderTheme.success
        if (s === "Paused")       return CoderTheme.warning
        if (s === "Error")        return CoderTheme.error
        if (s === "Initializing") return CoderTheme.info
        return CoderTheme.textDisabled
    }

    Rectangle {
        anchors.fill: parent
        color: CoderTheme.background
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // ---- Back button + title ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            CoderButton {
                variant: "subtle"
                text: "← Back"
                onClicked: taskDetailPage.backClicked()
            }

            Label {
                text: taskDetailPage.taskName
                font.pixelSize: 20
                font.bold: true
                color: CoderTheme.textPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        // ---- Header: template, owner ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Label {
                text: taskDetailPage.taskTemplate
                font.pixelSize: 13
                color: CoderTheme.textSecondary
            }

            Label {
                text: "·"
                font.pixelSize: 13
                color: CoderTheme.textDisabled
            }

            Label {
                text: taskDetailPage.taskOwner
                font.pixelSize: 13
                color: CoderTheme.textSecondary
            }
        }

        // ---- Status section ----
        Rectangle {
            Layout.fillWidth: true
            height: statusLayout.implicitHeight + 24
            radius: CoderTheme.radius
            color: taskDetailPage.taskStatusSurface(taskDetailPage.taskStatus)
            border.color: taskDetailPage.taskStatusColor(taskDetailPage.taskStatus)
            border.width: 1

            ColumnLayout {
                id: statusLayout
                anchors.fill: parent
                anchors.margins: 12
                spacing: 4

                RowLayout {
                    spacing: 8

                    // Status badge — use StatusChip
                    StatusChip {
                        status: taskDetailPage.taskStatusToChipStatus(taskDetailPage.taskStatus)
                    }
                }

                Label {
                    text: taskDetailPage.taskStateMessage
                    font.pixelSize: 13
                    color: CoderTheme.textSecondary
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    visible: taskDetailPage.taskStateMessage.length > 0
                }
            }
        }

        // ---- Initial prompt ----
        Label {
            text: "Prompt"
            font.pixelSize: 14
            font.bold: true
            color: CoderTheme.textPrimary
            visible: taskDetailPage.taskPrompt.length > 0
        }

        Rectangle {
            Layout.fillWidth: true
            height: promptLabel.implicitHeight + 24
            radius: CoderTheme.radius
            color: CoderTheme.surface
            border.color: CoderTheme.border
            border.width: 1
            visible: taskDetailPage.taskPrompt.length > 0

            Label {
                id: promptLabel
                anchors.fill: parent
                anchors.margins: 12
                text: taskDetailPage.taskPrompt
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                color: CoderTheme.textSecondary
            }
        }

        // ---- View Logs section ----
        Label {
            text: "Logs"
            font.pixelSize: 14
            font.bold: true
            color: CoderTheme.textPrimary
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120

            TextArea {
                id: logArea
                readOnly: true
                font.family: "monospace"
                font.pixelSize: 12
                color: CoderTheme.textPrimary
                placeholderText: "No log output yet…"
                wrapMode: TextEdit.Wrap
                background: Rectangle {
                    color: CoderTheme.surface
                    radius: CoderTheme.radius
                    border.color: CoderTheme.border
                    border.width: 1
                }
            }
        }

        // ---- Send Message section ----
        Label {
            text: "Send Message"
            font.pixelSize: 14
            font.bold: true
            color: CoderTheme.textPrimary
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: messageField
                placeholderText: "Type a message to the task…"
                Layout.fillWidth: true
                selectByMouse: true
                onAccepted: sendButton.clicked()
                background: Rectangle {
                    implicitHeight: 36
                    radius: CoderTheme.radius
                    color: CoderTheme.surface
                    border.color: messageField.activeFocus ? CoderTheme.primary : CoderTheme.border
                    border.width: 1
                }
            }

            CoderButton {
                id: sendButton
                text: "Send"
                enabled: messageField.text.trim().length > 0
                onClicked: {
                    var msg = messageField.text.trim()
                    if (msg.length > 0) {
                        // Append to local log for immediate feedback
                        logArea.text += "> " + msg + "\n"
                        messageField.clear()
                    }
                }
            }
        }
    }
}
