import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

Dialog {
    id: dialog

    property string agentName: ""
    property string workspaceName: ""
    property string workspaceOwner: ""

    readonly property string hostname:
        agentName + "." + workspaceName + "." + workspaceOwner + ".coder"

    title: "Port Forwarding"
    modal: true
    standardButtons: Dialog.NoButton
    anchors.centerIn: parent
    width: 420

    Material.background: CoderTheme.surface

    contentItem: ColumnLayout {
        spacing: 12

        // Header
        RowLayout {
            spacing: 12

            Label {
                font.pixelSize: 32
                text: "🔌"
            }

            ColumnLayout {
                spacing: 2

                Label {
                    text: "Port Forwarding"
                    font.pixelSize: 16
                    font.bold: true
                    color: CoderTheme.textPrimary
                }
                Label {
                    text: dialog.workspaceName + " / " + dialog.agentName
                    font.pixelSize: 12
                    color: CoderTheme.textSecondary
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: CoderTheme.divider
        }

        Label {
            text: "With VPN active, workspace ports are directly accessible:"
            font.pixelSize: 13
            color: CoderTheme.textPrimary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true

            TextField {
                id: hostField
                readOnly: true
                text: dialog.hostname + ":PORT"
                font.family: "monospace"
                font.pixelSize: 13
                Layout.fillWidth: true
                selectByMouse: true
                color: CoderTheme.textPrimary
                Material.background: CoderTheme.cardBg
            }

            CoderButton {
                text: pfCopyLabel.visible ? "Copied!" : "Copy"
                variant: "outline"
                onClicked: {
                    hostField.selectAll()
                    hostField.copy()
                    hostField.deselect()
                    pfCopyLabel.visible = true
                    pfCopyTimer.restart()
                }
            }
        }

        Label {
            id: pfCopyLabel
            text: ""
            visible: false
        }

        Timer {
            id: pfCopyTimer
            interval: 2000
            onTriggered: pfCopyLabel.visible = false
        }

        Label {
            text: "Replace <b>PORT</b> with the port number your service is running on " +
                  "(e.g. 8080, 3000). No explicit port forwarding is needed — " +
                  "the VPN tunnel provides direct connectivity."
            font.pixelSize: 12
            color: CoderTheme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            CoderButton {
                text: "Close"
                variant: "outline"
                onClicked: dialog.close()
            }
        }
    }
}
