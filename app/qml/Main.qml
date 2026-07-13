import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

ApplicationWindow {
    id: root
    width: 480
    height: 700
    title: sessionManager.authenticated
        ? "Coder Desktop — " + sessionManager.currentUsername
        : "Coder Desktop"
    visible: true
    color: CoderTheme.background
    readonly property bool dlpAvailable: typeof hasDlp !== "undefined" && hasDlp

    Material.theme: CoderTheme.isDark ? Material.Dark : Material.Light
    Material.accent: CoderTheme.primary
    Material.primary: CoderTheme.primary
    Material.background: CoderTheme.background
    Material.foreground: CoderTheme.textPrimary

    Component.onCompleted: {
        CoderTheme.mode = Qt.binding(function() { return settingsManager.theme })
    }

    // Invoked from C++ (tray "Open Agents", agent notification clicks).
    function openAgentsTab() {
        tabBar.currentIndex = 1
    }

    // ---- Keyboard shortcuts ----
    Shortcut { sequence: "Ctrl+1"; onActivated: tabBar.currentIndex = 0 }
    Shortcut { sequence: "Ctrl+2"; onActivated: tabBar.currentIndex = 1 }
    Shortcut { sequence: "Ctrl+3"; onActivated: if (typeof fileSyncManager !== "undefined" && (fileSyncManager.available || fileSyncManager.sessionCount > 0)) tabBar.currentIndex = 2 }
    Shortcut { sequence: "Ctrl+4"; onActivated: tabBar.currentIndex = 3 }
    Shortcut { sequence: "Ctrl+5"; onActivated: if (root.dlpAvailable) tabBar.currentIndex = 4 }
    Shortcut { sequence: "Ctrl+6"; onActivated: tabBar.currentIndex = 5 }
    Shortcut {
        sequence: "Ctrl+R"
        onActivated: {
            if (typeof pollingController !== "undefined")
                pollingController.refreshNow()
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: sessionManager.authenticated ? 1 : 0

        LoginPage { id: loginPage }

        ColumnLayout {
            spacing: 0

            // ---- Top bar: Coder logo + VPN status ----
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 56
                color: CoderTheme.surface

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    Image {
                        source: "qrc:/CoderDesktop/assets/coder-logo.svg"
                        sourceSize.height: 20
                        fillMode: Image.PreserveAspectFit
                    }

                    Label {
                        text: sessionManager.currentDeploymentUrl || ""
                        color: CoderTheme.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }

                    // VPN status dot — always visible
                    Rectangle {
                        width: 10; height: 10; radius: 5
                        color: {
                            var s = vpnBridge.state
                            if (s === "connected") return CoderTheme.success
                            if (s === "connecting" || s === "disconnecting") return CoderTheme.warning
                            if (s === "error") return CoderTheme.error
                            return CoderTheme.textDisabled
                        }
                        ToolTip.visible: vpnDotMa.containsMouse
                        ToolTip.text: "VPN: " + (vpnBridge.state || "Disconnected")
                        MouseArea {
                            id: vpnDotMa
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }
                }

                // Bottom divider
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: CoderTheme.divider
                }
            }

            // ---- Page content (visibility-based switching) ----
            // Use visibility-based switching instead of StackLayout.
            // StackLayout resizes non-current children to 0×0, which causes
            // ListView delegates to be culled.  When the tab is re-selected
            // only dataChanged fires (no rowsInserted), so delegates never
            // come back.  Visibility toggling keeps every page at full size
            // so delegates survive tab switches.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                WorkspacesPage { anchors.fill: parent; visible: tabBar.currentIndex === 0 }
                AgentsPage     { anchors.fill: parent; visible: tabBar.currentIndex === 1 }
                FileSyncPage   { anchors.fill: parent; visible: tabBar.currentIndex === 2 }
                VpnPage        { anchors.fill: parent; visible: tabBar.currentIndex === 3 }
                Loader {
                    anchors.fill: parent
                    active: root.dlpAvailable
                    visible: active && tabBar.currentIndex === 4
                    source: "SecureDevPage.qml"
                }
                SettingsPage   {
                    id: settingsPageInstance
                    anchors.fill: parent
                    visible: tabBar.currentIndex === 5
                    onNavigateToConnect: tabBar.currentIndex = 3
                }
            }

            // ---- Bottom divider above tab bar ----
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: CoderTheme.divider
            }

            // ---- Bottom tab bar ----
            TabBar {
                id: tabBar
                Layout.fillWidth: true
                Material.background: CoderTheme.surface
                Material.accent: CoderTheme.primary

                TabButton {
                    text: "Workspaces"
                    icon.name: "computer"
                    icon.color: tabBar.currentIndex === 0 ? CoderTheme.primary : CoderTheme.textSecondary
                    display: AbstractButton.TextUnderIcon
                    font.pixelSize: 10
                    font.capitalization: Font.MixedCase
                }
                TabButton {
                    text: "Agents"
                    icon.name: "mail-message-new"
                    icon.color: tabBar.currentIndex === 1 ? CoderTheme.primary : CoderTheme.textSecondary
                    display: AbstractButton.TextUnderIcon
                    font.pixelSize: 10
                    font.capitalization: Font.MixedCase

                    // Attention badge: unread + running + requires-action count.
                    Rectangle {
                        visible: agentsController.badgeCount > 0
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.topMargin: 4
                        anchors.rightMargin: 10
                        width: Math.max(14, badgeLabel.implicitWidth + 6)
                        height: 14
                        radius: 7
                        color: agentsController.requiresActionCount > 0
                            ? CoderTheme.warning : CoderTheme.primary
                        Label {
                            id: badgeLabel
                            anchors.centerIn: parent
                            text: agentsController.badgeCount > 99 ? "99+" : agentsController.badgeCount
                            color: "#ffffff"
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }
                }
                TabButton {
                    text: "File Sync"
                    icon.name: "folder-sync"
                    icon.color: tabBar.currentIndex === 2 ? CoderTheme.primary : CoderTheme.textSecondary
                    display: AbstractButton.TextUnderIcon
                    font.pixelSize: 10
                    font.capitalization: Font.MixedCase
                    visible: fileSyncManager.available || fileSyncManager.sessionCount > 0
                    width: visible ? implicitWidth : 0
                }
                TabButton {
                    text: "Connect"
                    icon.name: "network-vpn"
                    icon.color: tabBar.currentIndex === 3 ? CoderTheme.primary : CoderTheme.textSecondary
                    display: AbstractButton.TextUnderIcon
                    font.pixelSize: 10
                    font.capitalization: Font.MixedCase
                }
                TabButton {
                    text: "Secure Dev"
                    icon.name: "security-high"
                    icon.color: tabBar.currentIndex === 4 ? CoderTheme.primary : CoderTheme.textSecondary
                    display: AbstractButton.TextUnderIcon
                    font.pixelSize: 10
                    font.capitalization: Font.MixedCase
                    visible: root.dlpAvailable
                    width: root.dlpAvailable ? undefined : 0
                }
                TabButton {
                    text: "Settings"
                    icon.name: "preferences-system"
                    icon.color: tabBar.currentIndex === 5 ? CoderTheme.primary : CoderTheme.textSecondary
                    display: AbstractButton.TextUnderIcon
                    font.pixelSize: 10
                    font.capitalization: Font.MixedCase
                }
            }
        }
    }
}
