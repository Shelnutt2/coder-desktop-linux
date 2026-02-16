import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Page for managing multiple Coder deployments.
// Shows saved deployments with switch/remove actions and an inline form
// for adding new deployments via sessionManager.login().
Item {
    id: deploymentsPage

    // Refresh the deployment list whenever visibility changes or after
    // login/remove/switch actions.
    property var deploymentList: []

    function refreshDeployments() {
        deploymentList = sessionManager.deployments()
    }

    Component.onCompleted: refreshDeployments()

    // Re-query when the page becomes visible (e.g. tab switch).
    onVisibleChanged: if (visible) refreshDeployments()

    // Also refresh when auth state changes (login/logout/switch).
    Connections {
        target: sessionManager
        function onAuthStateChanged() { deploymentsPage.refreshDeployments() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // ---- Header ----
        Label {
            text: "Deployments"
            font.pixelSize: 20
            font.bold: true
        }

        Label {
            text: "Manage your Coder deployment connections."
            font.pixelSize: 13
            opacity: 0.6
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        // ---- Deployment list ----
        ListView {
            id: deploymentListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: deploymentsPage.deploymentList

            delegate: Rectangle {
                width: deploymentListView.width
                height: delegateContent.implicitHeight + 24
                radius: 8
                color: Material.background
                border.color: modelData.isActive
                    ? Material.accent : Material.dividerColor
                border.width: modelData.isActive ? 2 : 1

                RowLayout {
                    id: delegateContent
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    // Active indicator dot
                    Rectangle {
                        width: 12; height: 12; radius: 6
                        color: modelData.isActive
                            ? Material.color(Material.Green)
                            : Material.color(Material.Grey)
                        Layout.alignment: Qt.AlignVCenter

                        ToolTip.visible: activeMouseArea.containsMouse
                        ToolTip.text: modelData.isActive ? "Active" : "Inactive"

                        MouseArea {
                            id: activeMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }

                    // Deployment info
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: modelData.name || modelData.url
                            font.pixelSize: 15
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Label {
                            text: modelData.url
                            font.pixelSize: 12
                            opacity: 0.6
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 8
                            Label {
                                text: modelData.username || "—"
                                font.pixelSize: 12
                                opacity: 0.5
                            }
                            Label {
                                visible: modelData.isActive
                                text: "● Active"
                                font.pixelSize: 11
                                color: Material.color(Material.Green)
                            }
                        }
                    }

                    // Action buttons
                    RowLayout {
                        spacing: 4
                        Layout.alignment: Qt.AlignVCenter

                        Button {
                            flat: true
                            text: "Switch"
                            enabled: !modelData.isActive
                            onClicked: {
                                sessionManager.switchDeployment(modelData.url)
                                deploymentsPage.refreshDeployments()
                            }
                        }

                        Button {
                            flat: true
                            text: "Remove"
                            Material.foreground: Material.Red
                            onClicked: {
                                removeDialog.deploymentUrl = modelData.url
                                removeDialog.deploymentName =
                                    modelData.name || modelData.url
                                removeDialog.open()
                            }
                        }
                    }
                }
            }

            // ---- Empty state ----
            Label {
                anchors.centerIn: parent
                visible: deploymentsPage.deploymentList.length === 0
                text: "No deployments configured"
                font.pixelSize: 16
                opacity: 0.5
            }
        }

        // ---- Separator ----
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Material.dividerColor
        }

        // ---- Add New Deployment section ----
        Label {
            text: "Add New Deployment"
            font.pixelSize: 16
            font.bold: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: newUrlField
                placeholderText: "Deployment URL (e.g. https://coder.example.com)"
                Layout.fillWidth: true
                selectByMouse: true
            }

            TextField {
                id: newTokenField
                placeholderText: "API Token"
                Layout.fillWidth: true
                echoMode: TextInput.Password
                selectByMouse: true
            }

            RowLayout {
                spacing: 8

                Button {
                    text: "Add"
                    enabled: newUrlField.text.length > 0
                             && newTokenField.text.length > 0
                    highlighted: true
                    onClicked: {
                        sessionManager.login(
                            newUrlField.text.trim(),
                            newTokenField.text.trim())
                        newUrlField.text = ""
                        newTokenField.text = ""
                        // List refreshes via onAuthStateChanged
                    }
                }

                Button {
                    text: "Cancel"
                    flat: true
                    onClicked: {
                        newUrlField.text = ""
                        newTokenField.text = ""
                    }
                }
            }
        }
    }

    // ---- Remove confirmation dialog ----
    Dialog {
        id: removeDialog
        title: "Remove Deployment"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string deploymentUrl: ""
        property string deploymentName: ""

        Label {
            text: "Remove \"" + removeDialog.deploymentName
                  + "\"?\n\nThis will delete the saved credentials for this deployment."
            wrapMode: Text.WordWrap
            width: parent.width
        }

        onAccepted: {
            sessionManager.removeDeployment(removeDialog.deploymentUrl)
            deploymentsPage.refreshDeployments()
        }
    }
}
