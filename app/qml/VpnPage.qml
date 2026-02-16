import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// VPN status and peer list page.
// Connects/disconnects the VPN tunnel and displays reachable workspace agents.
Item {
    id: vpnPage

    property string vpnError: ""

    Connections {
        target: vpnBridge
        function onErrorOccurred(message) {
            vpnPage.vpnError = message
        }
        function onStateChanged() {
            if (vpnBridge.connected)
                vpnPage.vpnError = ""
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // ---- VPN status header ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            // Large status icon
            Label {
                text: vpnBridge.connected ? "🟢" : "🔴"
                font.pixelSize: 48
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: vpnBridge.stateString
                font.pixelSize: 22
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
                color: vpnBridge.connected
                    ? Material.color(Material.Green)
                    : Material.foreground
            }

            // Connect / Disconnect button
            Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 200
                Layout.preferredHeight: 44
                highlighted: true
                text: {
                    switch (vpnBridge.state) {
                    case 0: return "Connect VPN"       // Disconnected
                    case 1: return "Connecting…"        // Connecting
                    case 2: return "Disconnect VPN"    // Connected
                    case 3: return "Disconnecting…"     // Disconnecting
                    }
                    return "Connect VPN"
                }
                enabled: vpnBridge.state === 0 || vpnBridge.state === 2
                Material.background: vpnBridge.connected
                    ? Material.color(Material.Red)
                    : Material.accent
                onClicked: {
                    if (vpnBridge.connected) {
                        vpnBridge.stop()
                    } else {
                        vpnPage.vpnError = ""
                        vpnBridge.start(sessionManager.currentUrl,
                                        "") // Token is retrieved from SecureStorage by VpnBridge
                    }
                }
            }
        }

        // ---- Error banner ----
        Rectangle {
            Layout.fillWidth: true
            height: vpnErrorLabel.implicitHeight + 16
            radius: 4
            color: Material.color(Material.Red, Material.Shade50)
            visible: vpnPage.vpnError.length > 0

            Label {
                id: vpnErrorLabel
                anchors.fill: parent
                anchors.margins: 8
                text: vpnPage.vpnError
                color: Material.color(Material.Red)
                wrapMode: Text.WordWrap
                font.pixelSize: 13
            }
        }

        // ---- Separator ----
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Material.dividerColor
        }

        // ---- Peer list header ----
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Workspace Peers"
                font.pixelSize: 16
                font.bold: true
                Layout.fillWidth: true
            }
            Label {
                text: peerModel.count + " peer" + (peerModel.count !== 1 ? "s" : "")
                font.pixelSize: 13
                opacity: 0.6
            }
        }

        // ---- Peer list ----
        ListView {
            id: peerList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: peerModel

            delegate: Rectangle {
                width: peerList.width
                height: peerDelegateLayout.implicitHeight + 20
                radius: 6
                color: Material.background
                border.color: Material.dividerColor
                border.width: 1

                RowLayout {
                    id: peerDelegateLayout
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    // Status dot
                    Rectangle {
                        width: 10; height: 10; radius: 5
                        color: {
                            if (model.status === 2) return Material.color(Material.Green)
                            if (model.status === 1) return Material.color(Material.Orange)
                            return Material.color(Material.Grey)
                        }
                        Layout.alignment: Qt.AlignVCenter
                    }

                    // Peer info
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: model.hostname
                            font.pixelSize: 14
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Label {
                            text: model.workspaceName + " / " + model.agentName
                            font.pixelSize: 12
                            opacity: 0.6
                        }
                    }

                    // Ping latency
                    Label {
                        text: model.lastPingMs >= 0 ? model.lastPingMs + " ms" : "—"
                        font.pixelSize: 12
                        opacity: 0.7
                        Layout.alignment: Qt.AlignVCenter
                    }

                    // Connection type badge
                    Rectangle {
                        width: connTypeLabel.implicitWidth + 12
                        height: connTypeLabel.implicitHeight + 6
                        radius: 3
                        color: model.isP2P
                            ? Material.color(Material.Green, Material.Shade100)
                            : Material.color(Material.Blue, Material.Shade100)
                        Layout.alignment: Qt.AlignVCenter

                        Label {
                            id: connTypeLabel
                            anchors.centerIn: parent
                            text: model.isP2P ? "P2P" : "Relayed"
                            font.pixelSize: 10
                            font.bold: true
                            color: model.isP2P
                                ? Material.color(Material.Green, Material.Shade900)
                                : Material.color(Material.Blue, Material.Shade900)
                        }
                    }
                }
            }

            // ---- Empty state ----
            ColumnLayout {
                anchors.centerIn: parent
                visible: peerModel.count === 0
                spacing: 8

                Label {
                    text: vpnBridge.connected
                        ? "No peers connected"
                        : "Connect VPN to see workspace peers"
                    font.pixelSize: 15
                    opacity: 0.5
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }
    }
}
