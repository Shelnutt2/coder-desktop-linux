import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Qt.labs.platform as Platform
import CoderDesktop

Dialog {
    id: sessionDialog

    title: "New Sync Session"
    modal: true
    anchors.centerIn: parent
    width: 440
    padding: 20

    Material.background: CoderTheme.surface
    Material.foreground: CoderTheme.textPrimary

    property string errorMessage: ""

    // Build the connected agents list by iterating the peer model.
    // We use a ListModel proxy and refresh on open, since we cannot
    // reliably access role data by index outside a delegate context.
    property var connectedAgents: []

    function refreshAgents() {
        connectedAgents = [];
    }

    // Use an Instantiator to reactively read model roles into our JS array.
    // The Instantiator creates lightweight QtObjects for each peer; the
    // Component.onCompleted of each checks whether the peer is connected.
    Instantiator {
        id: agentInstantiator
        model: peerModel
        active: sessionDialog.visible
        delegate: QtObject {
            required property string hostname
            required property int status

            Component.onCompleted: updateAgents()
            Component.onDestruction: updateAgents()
            onStatusChanged: updateAgents()

            function updateAgents() {
                // Rebuild the whole list from instantiator children
                var agents = [];
                for (var i = 0; i < agentInstantiator.count; i++) {
                    var obj = agentInstantiator.objectAt(i);
                    if (obj && obj.status === 2) {
                        agents.push(obj.hostname);
                    }
                }
                sessionDialog.connectedAgents = agents;
            }
        }
    }

    onOpened: {
        localPathField.text = "";
        remotePathField.text = "";
        workspaceCombo.currentIndex = 0;
        sessionDialog.errorMessage = "";
    }

    Connections {
        target: fileSyncManager
        function onErrorOccurred(message) {
            sessionDialog.errorMessage = message;
        }
    }

    contentItem: ColumnLayout {
        spacing: 16

        // ---- Error display ----
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: errorLabel.implicitHeight + 16
            radius: CoderTheme.radiusSm
            color: CoderTheme.errorSurface
            border.color: CoderTheme.error
            border.width: 1
            visible: sessionDialog.errorMessage.length > 0

            Label {
                id: errorLabel
                anchors.fill: parent
                anchors.margins: 8
                text: sessionDialog.errorMessage
                color: CoderTheme.error
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }

        // ---- Local directory ----
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                text: "Local Directory"
                font.pixelSize: 12
                font.weight: Font.Medium
                color: CoderTheme.textSecondary
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: localPathField
                    Layout.fillWidth: true
                    placeholderText: "/home/user/project"
                    selectByMouse: true

                    background: Rectangle {
                        implicitHeight: 36
                        radius: CoderTheme.radius
                        color: CoderTheme.surface
                        border.color: localPathField.activeFocus ? CoderTheme.primary : CoderTheme.border
                    }
                }

                CoderButton {
                    text: "Browse"
                    variant: "outline"
                    onClicked: localFolderDialog.open()
                }
            }
        }

        // ---- Workspace ----
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                text: "Workspace"
                font.pixelSize: 12
                font.weight: Font.Medium
                color: CoderTheme.textSecondary
            }

            ComboBox {
                id: workspaceCombo
                Layout.fillWidth: true
                model: sessionDialog.connectedAgents
                enabled: sessionDialog.connectedAgents.length > 0
                displayText: sessionDialog.connectedAgents.length > 0 ? currentText : "No workspaces online"

                Material.background: CoderTheme.surface
                Material.foreground: CoderTheme.textPrimary

                background: Rectangle {
                    implicitHeight: 36
                    radius: CoderTheme.radius
                    color: CoderTheme.surface
                    border.color: workspaceCombo.activeFocus ? CoderTheme.primary : CoderTheme.border
                }
            }

            Label {
                text: "No workspaces are currently online. Connect to VPN and ensure workspaces are running."
                visible: sessionDialog.connectedAgents.length === 0
                font.pixelSize: 11
                color: CoderTheme.textDisabled
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        // ---- Remote directory ----
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                text: "Remote Directory"
                font.pixelSize: 12
                font.weight: Font.Medium
                color: CoderTheme.textSecondary
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: remotePathField
                    Layout.fillWidth: true
                    placeholderText: "~/project"
                    selectByMouse: true

                    background: Rectangle {
                        implicitHeight: 36
                        radius: CoderTheme.radius
                        color: CoderTheme.surface
                        border.color: remotePathField.activeFocus ? CoderTheme.primary : CoderTheme.border
                    }
                }

                CoderButton {
                    text: "Browse"
                    variant: "outline"
                    enabled: workspaceCombo.currentIndex >= 0 && sessionDialog.connectedAgents.length > 0
                    onClicked: {
                        remoteDirPicker.agentHostname = workspaceCombo.currentText;
                        remoteDirPicker.open();
                    }
                }
            }
        }
    }

    footer: DialogButtonBox {
        padding: 20

        CoderButton {
            text: "Cancel"
            variant: "outline"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        CoderButton {
            text: "Create"
            variant: "default"
            enabled: localPathField.text.length > 0 && remotePathField.text.length > 0 && workspaceCombo.currentIndex >= 0 && sessionDialog.connectedAgents.length > 0
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: {
        sessionDialog.errorMessage = "";
        fileSyncManager.createSession(localPathField.text, workspaceCombo.currentText, remotePathField.text);
    }

    // ---- Native folder dialog for local path ----
    Platform.FolderDialog {
        id: localFolderDialog
        title: "Select Local Directory"
        onAccepted: {
            var path = folder.toString();
            // Strip file:// prefix
            if (path.startsWith("file://")) {
                path = path.substring(7);
            }
            localPathField.text = path;
        }
    }

    // ---- Remote directory picker ----
    RemoteDirectoryPicker {
        id: remoteDirPicker

        onDirectorySelected: function (path) {
            remotePathField.text = path;
        }
    }
}
