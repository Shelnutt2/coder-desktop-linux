import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

Dialog {
    id: dialog

    property string workspaceName: ""
    property string workspaceOwner: ""
    property string agentName: ""
    property string agentId: ""
    property string agentDirectory: ""
    property string displayAppType: "vscode"  // "vscode" or "vscode_insiders"

    signal launchNormal()
    signal launchSecure()

    title: "Launch " + (displayAppType === "vscode_insiders" ? "VS Code Insiders" : "VS Code Desktop")
    modal: true
    standardButtons: Dialog.NoButton
    anchors.centerIn: parent
    width: 420

    Material.background: CoderTheme.surface

    contentItem: ColumnLayout {
        width: dialog.availableWidth
        spacing: 12

        // Header
        RowLayout {
            spacing: 12

            Label {
                font.pixelSize: 32
                text: "🖥️"
            }

            ColumnLayout {
                spacing: 2

                Label {
                    text: dialog.displayAppType === "vscode_insiders"
                          ? "VS Code Insiders" : "VS Code Desktop"
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
            text: "Choose how to open " + (dialog.displayAppType === "vscode_insiders"
                  ? "VS Code Insiders" : "VS Code Desktop") + ":"
            font.pixelSize: 13
            color: CoderTheme.textPrimary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            text: "<b>Normal mode</b> opens VS Code directly on your desktop " +
                  "and connects to the remote workspace."
            font.pixelSize: 12
            color: CoderTheme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            text: "<b>Secure mode</b> runs VS Code inside the DLP compositor " +
                  "sandbox with watermarking and data loss prevention."
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
                text: "Cancel"
                variant: "outline"
                onClicked: dialog.close()
            }

            CoderButton {
                text: "Open Normally"
                variant: "outline"
                onClicked: {
                    dialog.launchNormal()
                    dialog.close()
                }
            }

            CoderButton {
                text: "Secure Mode"
                variant: "default"
                onClicked: {
                    dialog.launchSecure()
                    dialog.close()
                }
            }
        }
    }
}
