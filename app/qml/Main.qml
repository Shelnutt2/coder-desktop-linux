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

            // Use visibility-based switching instead of StackLayout so that
            // pages keep their full dimensions when not active.  StackLayout
            // resizes non-current children to 0×0 which causes ListView to
            // cull all delegates; a model reset while at 0×0 then leaves the
            // list visually empty when the user switches back.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                WorkspacesPage  { anchors.fill: parent; visible: tabBar.currentIndex === 0 }
                VpnPage         { anchors.fill: parent; visible: tabBar.currentIndex === 1 }
                TasksPage       { anchors.fill: parent; visible: tabBar.currentIndex === 2 }
                DeploymentPage  { anchors.fill: parent; visible: tabBar.currentIndex === 3 }
                DlpPage         { anchors.fill: parent; visible: tabBar.currentIndex === 4 }
                SettingsPage    { anchors.fill: parent; visible: tabBar.currentIndex === 5 }
            }
        }
    }
}
