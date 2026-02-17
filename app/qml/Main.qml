import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 480
    height: 700
    title: sessionManager.authenticated
        ? "Coder Desktop — " + sessionManager.currentUsername
        : "Coder Desktop"
    visible: true
    color: CoderTheme.background

    Material.theme: CoderTheme.isDark ? Material.Dark : Material.Light
    Material.accent: CoderTheme.primary
    Material.primary: CoderTheme.primary
    Material.background: CoderTheme.background
    Material.foreground: CoderTheme.textPrimary

    Component.onCompleted: {
        CoderTheme.mode = Qt.binding(function() { return settingsManager.theme })
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
                            if (s === "Connected") return CoderTheme.success
                            if (s === "Connecting" || s === "Disconnecting") return CoderTheme.warning
                            if (s === "Error") return CoderTheme.error
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
                TasksPage      { anchors.fill: parent; visible: tabBar.currentIndex === 1 }
                VpnPage        { anchors.fill: parent; visible: tabBar.currentIndex === 2 }
                SecureDevPage  { anchors.fill: parent; visible: tabBar.currentIndex === 3 }
                SettingsPage   { anchors.fill: parent; visible: tabBar.currentIndex === 4 }
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
                    text: "Tasks"
                    icon.name: "dialog-information"
                    icon.color: tabBar.currentIndex === 1 ? CoderTheme.primary : CoderTheme.textSecondary
                    display: AbstractButton.TextUnderIcon
                    font.pixelSize: 10
                    font.capitalization: Font.MixedCase
                }
                TabButton {
                    text: "Connect"
                    icon.name: "network-vpn"
                    icon.color: tabBar.currentIndex === 2 ? CoderTheme.primary : CoderTheme.textSecondary
                    display: AbstractButton.TextUnderIcon
                    font.pixelSize: 10
                    font.capitalization: Font.MixedCase
                }
                TabButton {
                    text: "Secure Dev"
                    icon.name: "security-high"
                    icon.color: tabBar.currentIndex === 3 ? CoderTheme.primary : CoderTheme.textSecondary
                    display: AbstractButton.TextUnderIcon
                    font.pixelSize: 10
                    font.capitalization: Font.MixedCase
                }
                TabButton {
                    text: "Settings"
                    icon.name: "preferences-system"
                    icon.color: tabBar.currentIndex === 4 ? CoderTheme.primary : CoderTheme.textSecondary
                    display: AbstractButton.TextUnderIcon
                    font.pixelSize: 10
                    font.capitalization: Font.MixedCase
                }
            }
        }
    }
}
