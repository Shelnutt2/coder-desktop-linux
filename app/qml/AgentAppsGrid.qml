import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

// Reusable apps grid for a single agent — renders app tiles in a 3-column grid.
// Used inline inside each agent card (parent and child agents).
GridLayout {
    id: appsGrid
    columns: 3
    columnSpacing: 8
    rowSpacing: 8

    property var appsList: []
    property string agentId: ""
    property string agentName: ""

    signal appClicked(var appData)
    signal openInBrowser(var appData)

    visible: appsList.length > 0

    Repeater {
        model: appsGrid.appsList

        Rectangle {
            Layout.fillWidth: true
            height: appItemCol.implicitHeight + 16
            radius: CoderTheme.radius
            color: tileMouseArea.containsMouse ? CoderTheme.hoverBg : CoderTheme.surface
            border.color: CoderTheme.border
            border.width: 1

            MouseArea {
                id: tileMouseArea
                anchors.fill: parent
                hoverEnabled: true
                enabled: {
                    if (modelData.isDisplayApp === true) {
                        var daType = modelData.displayAppType
                        if (daType === "ssh_helper" || daType === "port_forwarding_helper")
                            return vpnBridge.isRunning
                        return true
                    }
                    return (modelData.appUrl || "").length > 0
                }
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: appsGrid.appClicked(modelData)
            }

            ColumnLayout {
                id: appItemCol
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                // App icon with letter fallback
                Item {
                    width: 32; height: 32
                    Layout.alignment: Qt.AlignHCenter

                    Image {
                        id: tileIcon
                        anchors.fill: parent
                        visible: status === Image.Ready
                        source: {
                            var icon = modelData.appIcon || ""
                            if (icon.length === 0) return ""
                            if (icon.startsWith("/")) {
                                var base = sessionManager.currentUrl.replace(/\/+$/, "")
                                return base + icon
                            }
                            return icon
                        }
                        sourceSize: Qt.size(32, 32)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        cache: true
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: CoderTheme.radiusSm
                        color: CoderTheme.activeSurface
                        visible: tileIcon.status !== Image.Ready

                        Label {
                            anchors.centerIn: parent
                            text: (modelData.appName || "?").charAt(0).toUpperCase()
                            font.pixelSize: 16
                            font.bold: true
                            color: CoderTheme.primary
                        }
                    }
                }

                Label {
                    text: modelData.appName || ""
                    font.pixelSize: 12
                    color: CoderTheme.textPrimary
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
            }

            // Overflow menu
            Button {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 2
                flat: true
                text: "⋮"
                font.pixelSize: 14
                z: 2
                visible: settingsManager.externalBrowserAllowed
                onClicked: tileOverflow.open()

                contentItem: Text {
                    text: "⋮"
                    font.pixelSize: 14
                    color: CoderTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    implicitWidth: 28; implicitHeight: 28
                    radius: CoderTheme.radiusSm
                    color: parent.hovered ? CoderTheme.hoverBg : "transparent"
                }

                Menu {
                    id: tileOverflow
                    MenuItem {
                        text: qsTr("Open in Browser")
                        onTriggered: appsGrid.openInBrowser(modelData)
                    }
                }
            }
        }
    }
}
