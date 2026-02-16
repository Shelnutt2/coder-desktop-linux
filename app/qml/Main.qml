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

    Material.accent: "#7C5CFC"  // Coder brand purple

    StackLayout {
        anchors.fill: parent
        currentIndex: sessionManager.authenticated ? 1 : 0

        LoginPage { id: loginPage }

        ColumnLayout {
            spacing: 0

            TabBar {
                id: tabBar
                Layout.fillWidth: true
                Material.accent: "#7C5CFC"

                TabButton { text: "Workspaces"; icon.name: "computer" }
                TabButton { text: "VPN"; icon.name: "network-vpn" }
                TabButton { text: "Tasks"; icon.name: "dialog-information" }
                TabButton { text: "Deployments"; icon.name: "network-server" }
                TabButton { text: "DLP"; icon.name: "security-high" }
                TabButton { text: "Settings"; icon.name: "preferences-system" }
            }

            StackLayout {
                currentIndex: tabBar.currentIndex
                Layout.fillWidth: true
                Layout.fillHeight: true

                WorkspacesPage {}
                VpnPage {}
                TasksPage {}
                DeploymentPage {}
                DlpPage {}
                SettingsPage {}
            }
        }
    }
}
