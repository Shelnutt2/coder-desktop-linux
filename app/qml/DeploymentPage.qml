import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

// Settings subpage for managing multiple Coder deployments.
// Shows saved deployments with switch/remove actions and an inline form
// for adding new deployments via sessionManager.login().
// Can be loaded via Loader from SettingsPage ("Manage Deployments →").
Item {
    id: deploymentsPage

    // Signal emitted when user taps the back button
    signal backRequested()

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
        function onAuthStateChanged() { refreshDeployments() }
    }

    Rectangle {
        anchors.fill: parent
        color: CoderTheme.background
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // ---- Header with back button ----
        RowLayout {
            spacing: 8

            Label {
                text: "←"
                font.pixelSize: 20
                color: CoderTheme.primary

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: backRequested()
                }
            }

            Label {
                text: "Deployments"
                font.pixelSize: 20
                font.bold: true
                color: CoderTheme.textPrimary
            }
        }

        Label {
            text: "Manage your Coder deployment connections."
            font.pixelSize: 13
            color: CoderTheme.textSecondary
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
            model: deploymentList

            delegate: Rectangle {
                width: deploymentListView.width
                height: delegateContent.implicitHeight + 24
                radius: CoderTheme.radius
                color: CoderTheme.surface
                border.color: modelData.isActive
                    ? CoderTheme.primary : CoderTheme.border
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
                            ? CoderTheme.primary
                            : CoderTheme.textDisabled
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
                            color: CoderTheme.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Label {
                            text: modelData.url
                            font.pixelSize: 12
                            color: CoderTheme.textSecondary
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 8
                            Label {
                                text: modelData.username || "—"
                                font.pixelSize: 12
                                color: CoderTheme.textDisabled
                            }
                            Label {
                                visible: modelData.isActive
                                text: "● Active"
                                font.pixelSize: 11
                                color: CoderTheme.success
                            }
                        }
                    }

                    // Action buttons
                    RowLayout {
                        spacing: 4
                        Layout.alignment: Qt.AlignVCenter

                        CoderButton {
                            text: "Switch"
                            variant: "outline"
                            enabled: !modelData.isActive
                            onClicked: {
                                sessionManager.switchDeployment(modelData.url)
                                refreshDeployments()
                            }
                        }

                        CoderButton {
                            text: "Remove"
                            variant: "destructive"
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
                visible: deploymentList.length === 0
                text: "No deployments configured"
                font.pixelSize: 16
                color: CoderTheme.textDisabled
            }
        }

        // ---- Separator ----
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: CoderTheme.divider
        }

        // ---- Add New Deployment section ----
        Label {
            text: "ADD NEW DEPLOYMENT"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: CoderTheme.textSecondary
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: newUrlField
                placeholderText: "Deployment URL (e.g. https://coder.example.com)"
                Layout.fillWidth: true
                selectByMouse: true
                color: CoderTheme.textPrimary
                background: Rectangle {
                    radius: CoderTheme.radiusSm
                    color: CoderTheme.surface
                    border.color: newUrlField.activeFocus ? CoderTheme.primary : CoderTheme.border
                    border.width: 1
                }
            }

            TextField {
                id: newTokenField
                placeholderText: "API Token"
                Layout.fillWidth: true
                echoMode: TextInput.Password
                selectByMouse: true
                color: CoderTheme.textPrimary
                background: Rectangle {
                    radius: CoderTheme.radiusSm
                    color: CoderTheme.surface
                    border.color: newTokenField.activeFocus ? CoderTheme.primary : CoderTheme.border
                    border.width: 1
                }
            }

            RowLayout {
                spacing: 8

                CoderButton {
                    text: "Add"
                    variant: "default"
                    enabled: newUrlField.text.length > 0
                             && newTokenField.text.length > 0
                    onClicked: {
                        sessionManager.login(
                            newUrlField.text.trim(),
                            newTokenField.text.trim())
                        newUrlField.text = ""
                        newTokenField.text = ""
                        // List refreshes via onAuthStateChanged
                    }
                }

                CoderButton {
                    text: "Cancel"
                    variant: "subtle"
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
            color: CoderTheme.textPrimary
        }

        onAccepted: {
            sessionManager.removeDeployment(removeDialog.deploymentUrl)
            refreshDeployments()
        }
    }
}
