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
                onClicked: taskDetailPage.backClicked()
            }

            Label {
                text: taskDetailPage.taskName
                font.pixelSize: 20
                font.bold: true
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
                opacity: 0.6
            }

            Label {
                text: "·"
                font.pixelSize: 13
                opacity: 0.4
            }

            Label {
                text: taskDetailPage.taskOwner
                font.pixelSize: 13
                opacity: 0.6
            }
        }

        // ---- Status section ----
        Rectangle {
            Layout.fillWidth: true
            height: statusLayout.implicitHeight + 24
            radius: 8
            color: {
                var s = taskDetailPage.taskStatus
                if (s === "Active")       return Qt.rgba(0.0, 0.7, 0.0, 0.08)
                if (s === "Paused")       return Qt.rgba(1.0, 0.6, 0.0, 0.08)
                if (s === "Error")        return Qt.rgba(1.0, 0.0, 0.0, 0.08)
                if (s === "Initializing") return Qt.rgba(0.0, 0.4, 1.0, 0.08)
                return Qt.rgba(0.5, 0.5, 0.5, 0.08)
            }
            border.color: {
                var s = taskDetailPage.taskStatus
                if (s === "Active")       return Material.color(Material.Green)
                if (s === "Paused")       return Material.color(Material.Orange)
                if (s === "Error")        return Material.color(Material.Red)
                if (s === "Initializing") return Material.color(Material.Blue)
                return Material.color(Material.Grey)
            }
            border.width: 1

            ColumnLayout {
                id: statusLayout
                anchors.fill: parent
                anchors.margins: 12
                spacing: 4

                RowLayout {
                    spacing: 8

                    // Status badge
                    Rectangle {
                        width: statusBadgeLabel.implicitWidth + 16
                        height: statusBadgeLabel.implicitHeight + 8
                        radius: 4
                        color: {
                            var s = taskDetailPage.taskStatus
                            if (s === "Active")       return Material.color(Material.Green)
                            if (s === "Paused")       return Material.color(Material.Orange)
                            if (s === "Error")        return Material.color(Material.Red)
                            if (s === "Initializing") return Material.color(Material.Blue)
                            return Material.color(Material.Grey)
                        }

                        Label {
                            id: statusBadgeLabel
                            anchors.centerIn: parent
                            text: taskDetailPage.taskStatus
                            font.pixelSize: 12
                            font.bold: true
                            color: "white"
                        }
                    }
                }

                Label {
                    text: taskDetailPage.taskStateMessage
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    visible: taskDetailPage.taskStateMessage.length > 0
                    opacity: 0.8
                }
            }
        }

        // ---- Initial prompt ----
        Label {
            text: "Prompt"
            font.pixelSize: 14
            font.bold: true
            visible: taskDetailPage.taskPrompt.length > 0
        }

        Rectangle {
            Layout.fillWidth: true
            height: promptLabel.implicitHeight + 24
            radius: 6
            color: Material.background
            border.color: Material.dividerColor
            border.width: 1
            visible: taskDetailPage.taskPrompt.length > 0

            Label {
                id: promptLabel
                anchors.fill: parent
                anchors.margins: 12
                text: taskDetailPage.taskPrompt
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                opacity: 0.8
            }
        }

        // ---- View Logs section ----
        Label {
            text: "Logs"
            font.pixelSize: 14
            font.bold: true
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
                placeholderText: "No log output yet…"
                wrapMode: TextEdit.Wrap
                background: Rectangle {
                    color: Qt.rgba(0, 0, 0, 0.03)
                    radius: 6
                    border.color: Material.dividerColor
                    border.width: 1
                }
            }
        }

        // ---- Send Message section ----
        Label {
            text: "Send Message"
            font.pixelSize: 14
            font.bold: true
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
            }

            Button {
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
