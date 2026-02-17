import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

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
            if (vpnBridge.isRunning)
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
            spacing: 12

            // Animated status circle
            Rectangle {
                id: statusCircle
                width: 120; height: 120; radius: 60
                Layout.alignment: Qt.AlignHCenter
                color: {
                    var s = vpnBridge.state
                    if (s === "connected") return CoderTheme.success
                    if (s === "connecting" || s === "disconnecting") return CoderTheme.warning
                    if (s === "error") return CoderTheme.error
                    return CoderTheme.surfaceSecondary  // Disconnected
                }

                // Pulsing animation for transitioning states
                SequentialAnimation on opacity {
                    running: vpnBridge.state === "connecting" || vpnBridge.state === "disconnecting"
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.4; duration: 800; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
                    onRunningChanged: if (!running) statusCircle.opacity = 1.0
                }

                Text {
                    anchors.centerIn: parent
                    text: "VPN"
                    font.pixelSize: 28
                    font.bold: true
                    color: {
                        var s = vpnBridge.state
                        if (s === "connected" || s === "connecting" || s === "disconnecting" || s === "error")
                            return CoderTheme.textInvert
                        return CoderTheme.textSecondary
                    }
                }
            }

            // State text
            Label {
                text: {
                    var s = vpnBridge.state
                    if (s === "connected") return "Connected"
                    if (s === "connecting") return "Connecting..."
                    if (s === "disconnecting") return "Disconnecting..."
                    if (s === "error") return "Error"
                    return "Disconnected"
                }
                font.pixelSize: 22
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
                color: CoderTheme.textPrimary
            }

            // Description text
            Label {
                text: "Coder Connect creates a secure tunnel to your workspaces"
                font.pixelSize: 13
                color: CoderTheme.textSecondary
                Layout.alignment: Qt.AlignHCenter
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            // Connect / Disconnect button
            CoderButton {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 200
                Layout.preferredHeight: 44
                variant: vpnBridge.isRunning ? "destructive" : "default"
                text: {
                    switch (vpnBridge.state) {
                    case "disconnected":  return "Connect"
                    case "connecting":    return "Connecting…"
                    case "connected":     return "Disconnect"
                    case "disconnecting": return "Disconnecting…"
                    }
                    return "Connect"
                }
                enabled: vpnBridge.state === "disconnected" || vpnBridge.state === "connected"
                onClicked: {
                    if (vpnBridge.isRunning) {
                        vpnBridge.stop()
                    } else {
                        vpnPage.vpnError = ""
                        vpnBridge.start(sessionManager.currentUrl,
                                        sessionManager.sessionToken())
                    }
                }
            }
        }

        // ---- Error banner ----
        Rectangle {
            Layout.fillWidth: true
            height: vpnErrorLabel.implicitHeight + 16
            radius: CoderTheme.radiusSm
            color: CoderTheme.errorSurface
            visible: vpnPage.vpnError.length > 0

            Label {
                id: vpnErrorLabel
                anchors.fill: parent
                anchors.margins: 8
                text: vpnPage.vpnError
                color: CoderTheme.error
                wrapMode: Text.WordWrap
                font.pixelSize: 13
            }
        }

        // ---- Separator ----
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: CoderTheme.divider
        }

        // ---- Peer list header ----
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "CONNECTED WORKSPACES"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                font.letterSpacing: 1.0
                color: CoderTheme.textSecondary
                Layout.fillWidth: true
            }
            Label {
                text: peerModel.count + " peer" + (peerModel.count !== 1 ? "s" : "")
                font.pixelSize: 12
                color: CoderTheme.textSecondary
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
                radius: CoderTheme.radius
                color: CoderTheme.surface
                border.color: CoderTheme.border
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
                            if (model.status === 2) return CoderTheme.success
                            if (model.status === 1) return CoderTheme.warning
                            return CoderTheme.textDisabled
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
                            color: CoderTheme.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Label {
                            text: model.workspaceName + " / " + model.agentName
                            font.pixelSize: 12
                            color: CoderTheme.textSecondary
                        }
                    }

                    // Ping latency
                    Label {
                        text: model.lastPingMs >= 0 ? model.lastPingMs + " ms" : "—"
                        font.pixelSize: 12
                        color: CoderTheme.textSecondary
                        Layout.alignment: Qt.AlignVCenter
                    }

                    // Connection type badge
                    Rectangle {
                        width: connTypeLabel.implicitWidth + 12
                        height: connTypeLabel.implicitHeight + 6
                        radius: CoderTheme.radiusSm
                        color: model.isP2P
                            ? CoderTheme.successSurface
                            : CoderTheme.activeSurface
                        Layout.alignment: Qt.AlignVCenter

                        Label {
                            id: connTypeLabel
                            anchors.centerIn: parent
                            text: model.isP2P ? "P2P" : "Relayed"
                            font.pixelSize: 10
                            font.bold: true
                            color: model.isP2P
                                ? CoderTheme.success
                                : CoderTheme.info
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
                    text: vpnBridge.isRunning
                        ? "No peers connected"
                        : "Connect VPN to see workspace peers"
                    font.pixelSize: 15
                    color: CoderTheme.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }
    }
}
