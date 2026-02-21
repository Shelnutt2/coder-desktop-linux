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

    readonly property string sshHost:
        agentName + "." + workspaceName + "." + workspaceOwner + ".coder"

    title: "SSH Connection"
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
                text: "🔒"
            }

            ColumnLayout {
                spacing: 2

                Label {
                    text: "SSH Connection"
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
            text: "Connect to this workspace via SSH:"
            font.pixelSize: 13
            color: CoderTheme.textPrimary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            spacing: 8
            Layout.fillWidth: true

            TextField {
                id: sshField
                readOnly: true
                text: "ssh " + dialog.sshHost
                font.family: "monospace"
                font.pixelSize: 13
                Layout.fillWidth: true
                selectByMouse: true
                color: CoderTheme.textPrimary
                Material.background: CoderTheme.cardBg
            }

            CoderButton {
                text: copyLabel.visible ? "Copied!" : "Copy"
                variant: "outline"
                onClicked: {
                    sshField.selectAll()
                    sshField.copy()
                    sshField.deselect()
                    copyLabel.visible = true
                    copyTimer.restart()
                }
            }
        }

        Label {
            id: copyLabel
            text: ""
            visible: false
        }

        Timer {
            id: copyTimer
            interval: 2000
            onTriggered: copyLabel.visible = false
        }

        Label {
            text: "Requires VPN to be connected. The hostname is routable " +
                  "directly over the Coder VPN tunnel."
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
