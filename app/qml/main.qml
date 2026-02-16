import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 400
    height: 600
    title: "Coder Desktop"
    visible: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16

        Label {
            text: "Coder Desktop for Linux"
            font.pixelSize: 24
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Item { Layout.preferredHeight: 16 }

        Label {
            text: "VPN Status: " + vpnBridge.stateString
            font.pixelSize: 16
            Layout.alignment: Qt.AlignHCenter
        }

        Item { Layout.preferredHeight: 8 }

        Button {
            text: vpnBridge.connected ? "Disconnect VPN" : "Connect VPN"
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                if (vpnBridge.connected) {
                    vpnBridge.stop()
                }
                // else: would need URL/token — placeholder for Phase 2
            }
        }

        Item { Layout.fillHeight: true }

        Label {
            text: "Phase 1 — Foundation skeleton"
            color: "gray"
            font.pixelSize: 12
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
