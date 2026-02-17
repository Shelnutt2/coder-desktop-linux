import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

Page {
    id: secureDevPage

    header: Label {
        text: "Secure Development"
        font.pixelSize: 20
        font.bold: true
        color: CoderTheme.textPrimary
        padding: 16
    }

    background: Rectangle { color: CoderTheme.background }

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
                radius: CoderTheme.radius
                color: dlpCompositor.available ? CoderTheme.successSurface : CoderTheme.warningSurface
                visible: true

                ColumnLayout {
                    id: availBannerCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    Label {
                        text: dlpCompositor.available
                            ? "✓ Wayland session detected — Secure Development is available"
                            : "⚠ Secure Development requires a Wayland session"
                        font.bold: true
                        color: dlpCompositor.available ? CoderTheme.success : CoderTheme.warning
                    }
                    Label {
                        text: dlpCompositor.available
                            ? "The nested compositor can enforce clipboard, screenshot, and sandbox policies."
                            : "You are running under X11 or a non-Wayland display server. Secure Development features are disabled."
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        color: CoderTheme.textSecondary
                    }
                }
            }

            // ---- Compositor status & controls (only when available + enabled) ----
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                radius: CoderTheme.radius
                color: CoderTheme.surface
                border.color: CoderTheme.border
                border.width: 1
                implicitHeight: compositorCol.implicitHeight + 24
                visible: dlpCompositor.available && settingsManager.dlpEnabled

                ColumnLayout {
                    id: compositorCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Label {
                        text: "COMPOSITOR"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: CoderTheme.textSecondary
                    }

                    RowLayout {
                        Label {
                            text: "Status:"
                            color: CoderTheme.textPrimary
                        }
                        Label {
                            text: dlpCompositor.running ? "Running" : "Stopped"
                            color: dlpCompositor.running ? CoderTheme.success : CoderTheme.error
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        CoderButton {
                            text: dlpCompositor.running ? "Stop" : "Start"
                            variant: dlpCompositor.running ? "destructive" : "default"
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
                        color: CoderTheme.textSecondary
                    }
                }
            }

            // ---- Active Policies (read-only summary) ----
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                radius: CoderTheme.radius
                color: CoderTheme.surface
                border.color: CoderTheme.border
                border.width: 1
                implicitHeight: policyCol.implicitHeight + 24
                visible: dlpCompositor.available && settingsManager.dlpEnabled

                ColumnLayout {
                    id: policyCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Label {
                        text: "ACTIVE POLICIES"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: CoderTheme.textSecondary
                    }

                    GridLayout {
                        columns: 2
                        columnSpacing: 16
                        rowSpacing: 6
                        Layout.fillWidth: true

                        Label { text: "Clipboard:"; color: CoderTheme.textPrimary }
                        Label {
                            text: settingsManager.dlpClipboardBlock ? "Blocked" : "Allowed"
                            font.bold: true
                            color: settingsManager.dlpClipboardBlock ? CoderTheme.success : CoderTheme.textSecondary
                        }

                        Label { text: "Screenshots:"; color: CoderTheme.textPrimary }
                        Label {
                            text: "Blocked (always)"
                            font.bold: true
                            color: CoderTheme.success
                        }

                        Label { text: "File sandbox:"; color: CoderTheme.textPrimary }
                        Label {
                            text: settingsManager.dlpFileSandbox ? "Enabled" : "Disabled"
                            font.bold: true
                            color: settingsManager.dlpFileSandbox ? CoderTheme.success : CoderTheme.textSecondary
                        }

                        Label { text: "Network sandbox:"; color: CoderTheme.textPrimary }
                        Label {
                            text: settingsManager.dlpNetworkSandbox ? "Enabled" : "Disabled"
                            font.bold: true
                            color: settingsManager.dlpNetworkSandbox ? CoderTheme.success : CoderTheme.textSecondary
                        }

                        Label { text: "In-app browser:"; color: CoderTheme.textPrimary }
                        Label {
                            text: settingsManager.dlpForceInAppBrowser ? "Forced" : "Optional"
                            font.bold: true
                            color: CoderTheme.textPrimary
                        }

                        Label { text: "External browser:"; color: CoderTheme.textPrimary }
                        Label {
                            text: settingsManager.externalBrowserAllowed ? "Allowed" : "Blocked"
                            font.bold: true
                            color: CoderTheme.textPrimary
                        }
                    }

                    // Configure in Settings →
                    Label {
                        text: "Configure in Settings →"
                        font.pixelSize: 13
                        color: CoderTheme.primary
                        Layout.topMargin: 4

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            // Navigation handled by parent
                        }
                    }
                }
            }

            // ---- Launch App in Sandbox section ----
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                radius: CoderTheme.radius
                color: CoderTheme.surface
                border.color: CoderTheme.border
                border.width: 1
                implicitHeight: launchCol.implicitHeight + 24
                visible: dlpCompositor.available && settingsManager.dlpEnabled
                         && dlpCompositor.running

                ColumnLayout {
                    id: launchCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Label {
                        text: "LAUNCH APP IN SANDBOX"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: CoderTheme.textSecondary
                    }

                    TextField {
                        id: commandField
                        placeholderText: "Command (e.g. firefox, code, xterm)"
                        Layout.fillWidth: true
                        color: CoderTheme.textPrimary
                        background: Rectangle {
                            radius: CoderTheme.radiusSm
                            color: CoderTheme.background
                            border.color: commandField.activeFocus ? CoderTheme.primary : CoderTheme.border
                            border.width: 1
                        }
                    }

                    TextField {
                        id: workspacePathField
                        placeholderText: "Workspace path (optional)"
                        Layout.fillWidth: true
                        color: CoderTheme.textPrimary
                        background: Rectangle {
                            radius: CoderTheme.radiusSm
                            color: CoderTheme.background
                            border.color: workspacePathField.activeFocus ? CoderTheme.primary : CoderTheme.border
                            border.width: 1
                        }
                    }

                    RowLayout {
                        spacing: 16

                        CheckBox {
                            id: pidCheck
                            text: "Isolate PID"
                            checked: true
                            Material.accent: CoderTheme.primary
                        }
                        CheckBox {
                            id: ipcCheck
                            text: "Isolate IPC"
                            checked: true
                            Material.accent: CoderTheme.primary
                        }
                        CheckBox {
                            id: netCheck
                            text: "Isolate Network"
                            checked: false
                            Material.accent: CoderTheme.primary
                        }
                    }

                    CoderButton {
                        text: "Launch"
                        variant: "default"
                        enabled: commandField.text.length > 0
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

            // ---- Not enabled message ----
            Rectangle {
                Layout.fillWidth: true
                Layout.margins: 16
                height: notEnabledCol.implicitHeight + 24
                radius: CoderTheme.radius
                color: CoderTheme.warningSurface
                visible: dlpCompositor.available && !settingsManager.dlpEnabled

                ColumnLayout {
                    id: notEnabledCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    Label {
                        text: "Secure Development is not enabled"
                        font.bold: true
                        color: CoderTheme.warning
                    }
                    Label {
                        text: "Enable Data Loss Prevention in Settings to use the secure compositor."
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        color: CoderTheme.textSecondary
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
        color: CoderTheme.error
        wrapMode: Text.WordWrap
        font.bold: true
        padding: 8
        background: Rectangle { color: CoderTheme.errorSurface; radius: CoderTheme.radiusSm }
    }

    Timer {
        id: errorTimer
        interval: 5000
        onTriggered: errorLabel.visible = false
    }
}
