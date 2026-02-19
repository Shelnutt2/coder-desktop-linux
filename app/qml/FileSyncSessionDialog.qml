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
    property bool creating: false
    property int previousSessionCount: 0

    // Build the connected agents list by iterating the peer model.
    property var connectedAgents: []

    function refreshAgents() {
        var agents = [];
        for (var i = 0; i < agentInstantiator.count; i++) {
            var obj = agentInstantiator.objectAt(i);
            // PeerModel::Connected == 2 (see PeerModel.h)
            if (obj && obj.status === 2) {
                agents.push(obj.hostname);
            }
        }
        connectedAgents = agents;
    }

    // Use an Instantiator to reactively track peer model roles. The
    // Instantiator stays active at all times so the agent list is ready
    // when the dialog opens.
    Instantiator {
        id: agentInstantiator
        model: peerModel
        delegate: QtObject {
            required property string hostname
            required property int status

            Component.onCompleted: refreshAgentsDelayed.restart()
            Component.onDestruction: refreshAgentsDelayed.restart()
            onStatusChanged: sessionDialog.refreshAgents()
        }
    }

    // Coalesce rapid Instantiator creation events into a single refresh.
    // During bulk model resets objectAt() may return null for indices that
    // are still being created; the short delay lets Qt finish instantiation.
    Timer {
        id: refreshAgentsDelayed
        interval: 50
        repeat: false
        onTriggered: sessionDialog.refreshAgents()
    }

    onOpened: {
        localPathField.text = "";
        remotePathField.text = "";
        workspaceCombo.currentIndex = 0;
        sessionDialog.errorMessage = "";
        sessionDialog.creating = false;
    }

    Connections {
        target: fileSyncManager
        function onErrorOccurred(message) {
            sessionDialog.errorMessage = message;
            sessionDialog.creating = false;
        }
        function onSessionCountChanged() {
            if (sessionDialog.creating && fileSyncManager.sessionCount > sessionDialog.previousSessionCount) {
                sessionDialog.creating = false;
                sessionDialog.close();
            }
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
                    color: CoderTheme.textPrimary
                    placeholderTextColor: CoderTheme.textDisabled

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
                    color: CoderTheme.textPrimary
                    placeholderTextColor: CoderTheme.textDisabled

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
            text: sessionDialog.creating ? "Creating…" : "Create"
            variant: "default"
            enabled: !sessionDialog.creating && localPathField.text.length > 0 && remotePathField.text.length > 0 && workspaceCombo.currentIndex >= 0 && sessionDialog.connectedAgents.length > 0
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: {
        sessionDialog.errorMessage = "";
        sessionDialog.creating = true;
        sessionDialog.previousSessionCount = fileSyncManager.sessionCount;
        fileSyncManager.createSession(localPathField.text, workspaceCombo.currentText, remotePathField.text);
    }

    // ---- Native folder dialog for local path ----
    Platform.FolderDialog {
        id: localFolderDialog
        title: "Select Local Directory"
        onAccepted: {
            var path = folder.toString();
            // Strip file:// prefix — file:/// means local absolute path
            if (path.startsWith("file:///")) path = path.substring(7);
            else if (path.startsWith("file://")) path = path.substring(6);
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
