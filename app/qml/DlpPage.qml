import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: dlpPage

    header: Label {
        text: "Data Loss Prevention"
        font.pixelSize: 20
        font.bold: true
        padding: 16
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 12

            // ---- Availability banner ----
            Rectangle {
                Layout.fillWidth: true
                Layout.margins: 16
                height: availBannerCol.implicitHeight + 24
                radius: 8
                color: dlpCompositor.available ? "#E8F5E9" : "#FFF3E0"
                visible: true

                ColumnLayout {
                    id: availBannerCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    Label {
                        text: dlpCompositor.available
                            ? "✓ Wayland session detected — DLP is available"
                            : "⚠ DLP requires a Wayland session"
                        font.bold: true
                        color: dlpCompositor.available ? "#2E7D32" : "#E65100"
                    }
                    Label {
                        text: dlpCompositor.available
                            ? "The nested compositor can enforce clipboard, screenshot, and sandbox policies."
                            : "You are running under X11 or a non-Wayland display server. DLP features are disabled."
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        opacity: 0.8
                    }
                }
            }

            // ---- Enable/Disable toggle ----
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: dlpCompositor.available

                Label { text: "Enable DLP"; font.pixelSize: 15 }
                Item { Layout.fillWidth: true }
                Switch {
                    id: dlpSwitch
                    checked: settingsManager.dlpEnabled
                    enabled: !settingsManager.dlpEnabledLocked
                    onToggled: settingsManager.setUserPreference("dlpEnabled", checked)
                }
                Label {
                    text: "🔒"
                    visible: settingsManager.dlpEnabledLocked
                    font.pixelSize: 12
                    opacity: 0.6
                }
            }

            // ---- Compositor status & controls (only when DLP is available + enabled) ----
            GroupBox {
                title: "Compositor"
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: dlpCompositor.available && settingsManager.dlpEnabled

                ColumnLayout {
                    width: parent.width
                    spacing: 8

                    RowLayout {
                        Label { text: "Status:" }
                        Label {
                            text: dlpCompositor.running ? "Running" : "Stopped"
                            color: dlpCompositor.running ? "#2E7D32" : "#C62828"
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: dlpCompositor.running ? "Stop" : "Start"
                            Material.background: dlpCompositor.running
                                ? "#C62828" : Material.accent
                            onClicked: {
                                if (dlpCompositor.running)
                                    dlpCompositor.stop();
                                else
                                    dlpCompositor.start();
                            }
                        }
                    }

                    Label {
                        text: "Launched apps: " + dlpCompositor.launchedAppCount
                        opacity: 0.7
                    }
                }
            }

            // ---- Policy display ----
            GroupBox {
                title: "Active Policy"
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: dlpCompositor.available && settingsManager.dlpEnabled

                GridLayout {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 6
                    width: parent.width

                    Label { text: "Clipboard block:" }
                    Label {
                        text: settingsManager.dlpClipboardBlock ? "ON" : "OFF"
                        font.bold: true
                        color: settingsManager.dlpClipboardBlock ? "#2E7D32" : "#757575"
                    }

                    Label { text: "Screenshot block:" }
                    Label {
                        text: "ON (always)"
                        font.bold: true
                        color: "#2E7D32"
                    }

                    Label { text: "File sandbox:" }
                    Label {
                        text: settingsManager.dlpFileSandbox ? "ON" : "OFF"
                        font.bold: true
                        color: settingsManager.dlpFileSandbox ? "#2E7D32" : "#757575"
                    }

                    Label { text: "Network sandbox:" }
                    Label {
                        text: settingsManager.dlpNetworkSandbox ? "ON" : "OFF"
                        font.bold: true
                        color: settingsManager.dlpNetworkSandbox ? "#2E7D32" : "#757575"
                    }
                }
            }

            // ---- Launch App section ----
            GroupBox {
                title: "Launch App in DLP Sandbox"
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: dlpCompositor.available && settingsManager.dlpEnabled
                         && dlpCompositor.running

                ColumnLayout {
                    width: parent.width
                    spacing: 8

                    TextField {
                        id: commandField
                        placeholderText: "Command (e.g. firefox, code, xterm)"
                        Layout.fillWidth: true
                    }

                    TextField {
                        id: workspacePathField
                        placeholderText: "Workspace path (optional)"
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        spacing: 16

                        CheckBox {
                            id: pidCheck
                            text: "Isolate PID"
                            checked: true
                        }
                        CheckBox {
                            id: ipcCheck
                            text: "Isolate IPC"
                            checked: true
                        }
                        CheckBox {
                            id: netCheck
                            text: "Isolate Network"
                            checked: false
                        }
                    }

                    Button {
                        text: "Launch"
                        enabled: commandField.text.length > 0
                        Material.background: Material.accent
                        onClicked: {
                            dlpCompositor.launchApp(
                                commandField.text,
                                workspacePathField.text,
                                pidCheck.checked,
                                ipcCheck.checked,
                                netCheck.checked
                            );
                            commandField.text = "";
                        }
                    }
                }
            }

            // ---- Spacer ----
            Item { Layout.fillHeight: true }
        }
    }

    // ---- Error handling ----
    Connections {
        target: dlpCompositor
        function onErrorOccurred(message) {
            errorLabel.text = message;
            errorLabel.visible = true;
            errorTimer.restart();
        }
    }

    Label {
        id: errorLabel
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        visible: false
        color: "#C62828"
        wrapMode: Text.WordWrap
        font.bold: true
        padding: 8
        background: Rectangle { color: "#FFEBEE"; radius: 4 }
    }

    Timer {
        id: errorTimer
        interval: 5000
        onTriggered: errorLabel.visible = false
    }
}
