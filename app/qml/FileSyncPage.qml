import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

Item {
    id: fileSyncPage

    property int selectedIndex: -1
    property string selectedSessionId: ""
    property bool selectedPaused: false

    function statusDotColor(category) {
        switch (category) {
        case "ok":
            return CoderTheme.success;
        case "working":
            return CoderTheme.warning;
        case "error":
            return CoderTheme.error;
        case "paused":
            return CoderTheme.textDisabled;
        default:
            return CoderTheme.textDisabled;
        }
    }

    Rectangle {
        anchors.fill: parent
        color: CoderTheme.background
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // ---- Header: title + new session button ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Label {
                text: "File Sync"
                font.pixelSize: 20
                font.weight: Font.Bold
                color: CoderTheme.textPrimary
                Layout.fillWidth: true
            }

            CoderButton {
                text: "New Session"
                variant: "default"
                enabled: fileSyncManager.available && vpnBridge.state === "connected"
                onClicked: newSessionDialog.open()
            }
        }

        // ---- Prerequisites banner: VPN not connected ----
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: prereqRow.implicitHeight + 16
            radius: CoderTheme.radius
            color: CoderTheme.warningSurface
            border.color: CoderTheme.warning
            border.width: 1
            visible: vpnBridge.state !== "connected"

            RowLayout {
                id: prereqRow
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Label {
                    text: "⚠"
                    font.pixelSize: 16
                    color: CoderTheme.warning
                }

                Label {
                    text: "Connect to Coder VPN to enable file sync."
                    color: CoderTheme.textPrimary
                    font.pixelSize: 13
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ---- Policy disabled banner ----
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: policyRow.implicitHeight + 16
            radius: CoderTheme.radius
            color: CoderTheme.errorSurface
            border.color: CoderTheme.error
            border.width: 1
            visible: !fileSyncManager.available

            RowLayout {
                id: policyRow
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Label {
                    text: "File transfer is disabled by policy."
                    color: CoderTheme.textPrimary
                    font.pixelSize: 13
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ---- Divider ----
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: CoderTheme.divider
        }

        // ---- Column header ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: fileSyncManager.count > 0

            Label {
                text: "Local Path"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                font.letterSpacing: 1.0
                color: CoderTheme.textSecondary
                Layout.fillWidth: true
                Layout.preferredWidth: 3
            }

            Label {
                text: "Workspace"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                font.letterSpacing: 1.0
                color: CoderTheme.textSecondary
                Layout.fillWidth: true
                Layout.preferredWidth: 2
            }

            Label {
                text: "Remote Path"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                font.letterSpacing: 1.0
                color: CoderTheme.textSecondary
                Layout.fillWidth: true
                Layout.preferredWidth: 3
            }

            // Status dot column header spacer
            Item {
                implicitWidth: 24
            }
        }

        // ---- Session list ----
        ListView {
            id: sessionList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: fileSyncPage.visible ? fileSyncManager : null
            clip: true
            spacing: 4
            currentIndex: fileSyncPage.selectedIndex

            delegate: Rectangle {
                id: delegateCard
                width: sessionList.width
                height: delegateRow.implicitHeight + 16
                radius: CoderTheme.radiusSm
                color: {
                    if (fileSyncPage.selectedIndex === index) return CoderTheme.activeSurface;
                    if (delegateMouseArea.containsMouse) return CoderTheme.hoverBg;
                    return "transparent";
                }
                border.color: fileSyncPage.selectedIndex === index ? CoderTheme.primary : "transparent"
                border.width: fileSyncPage.selectedIndex === index ? 1 : 0

                Behavior on color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                MouseArea {
                    id: delegateMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onClicked: function (mouse) {
                        if (mouse.button === Qt.RightButton) {
                            fileSyncPage.selectedIndex = index;
                            fileSyncPage.selectedSessionId = model.sessionId;
                            fileSyncPage.selectedPaused = model.paused;
                            contextMenu.popup();
                        } else {
                            if (fileSyncPage.selectedIndex === index) {
                                fileSyncPage.selectedIndex = -1;
                                fileSyncPage.selectedSessionId = "";
                                fileSyncPage.selectedPaused = false;
                            } else {
                                fileSyncPage.selectedIndex = index;
                                fileSyncPage.selectedSessionId = model.sessionId;
                                fileSyncPage.selectedPaused = model.paused;
                            }
                        }
                    }
                }

                RowLayout {
                    id: delegateRow
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Label {
                        text: model.localPath
                        color: CoderTheme.textPrimary
                        font.pixelSize: 13
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                        Layout.preferredWidth: 3
                    }

                    Label {
                        text: model.workspace
                        color: CoderTheme.textSecondary
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Layout.preferredWidth: 2
                    }

                    Label {
                        text: model.remotePath
                        color: CoderTheme.textPrimary
                        font.pixelSize: 13
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                        Layout.preferredWidth: 3
                    }

                    // Status dot
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: fileSyncPage.statusDotColor(model.statusCategory)

                        ToolTip.visible: dotMouseArea.containsMouse
                        ToolTip.text: model.statusString + (model.conflictCount > 0 ? " (" + model.conflictCount + " conflicts)" : "")
                        ToolTip.delay: 500

                        MouseArea {
                            id: dotMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }
                }

                // Right-click context menu
                Menu {
                    id: contextMenu
                    Material.background: CoderTheme.surface

                    MenuItem {
                        text: model.paused ? "Resume" : "Pause"
                        onTriggered: {
                            if (model.paused) {
                                fileSyncManager.resumeSession(model.sessionId);
                            } else {
                                fileSyncManager.pauseSession(model.sessionId);
                            }
                        }
                    }
                    MenuItem {
                        text: "Reset"
                        onTriggered: fileSyncManager.resetSession(model.sessionId)
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: "Delete"
                        onTriggered: {
                            deleteSessionId = model.sessionId;
                            deleteConfirmDialog.open();
                        }

                        contentItem: Text {
                            text: "Delete"
                            color: CoderTheme.error
                        }
                    }
                }
            }

            // ---- Empty state ----
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                visible: fileSyncManager.count === 0 && fileSyncManager.available
                opacity: 0.6

                Label {
                    text: "📁"
                    font.pixelSize: 48
                    Layout.alignment: Qt.AlignHCenter
                }

                Label {
                    text: "No sync sessions"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    color: CoderTheme.textPrimary
                    Layout.alignment: Qt.AlignHCenter
                }

                Label {
                    text: "Create one to sync files between your\nmachine and workspaces."
                    font.pixelSize: 13
                    color: CoderTheme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    Layout.alignment: Qt.AlignHCenter
                }

                CoderButton {
                    text: "New Session"
                    variant: "outline"
                    Layout.alignment: Qt.AlignHCenter
                    enabled: vpnBridge.state === "connected"
                    onClicked: newSessionDialog.open()
                }
            }
        }

        // ---- Action bar ----
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: actionRow.implicitHeight + 16
            radius: CoderTheme.radius
            color: CoderTheme.surface
            border.color: CoderTheme.border
            border.width: 1
            visible: fileSyncPage.selectedIndex >= 0 && fileSyncPage.selectedIndex < fileSyncManager.count

            RowLayout {
                id: actionRow
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Item {
                    Layout.fillWidth: true
                }

                CoderButton {
                    text: fileSyncPage.selectedPaused ? "Resume" : "Pause"
                    variant: "outline"
                    onClicked: {
                        if (fileSyncPage.selectedPaused) {
                            fileSyncManager.resumeSession(fileSyncPage.selectedSessionId);
                        } else {
                            fileSyncManager.pauseSession(fileSyncPage.selectedSessionId);
                        }
                    }
                }

                CoderButton {
                    text: "Reset"
                    variant: "outline"
                    onClicked: fileSyncManager.resetSession(fileSyncPage.selectedSessionId)
                }

                CoderButton {
                    text: "Delete"
                    variant: "destructive"
                    onClicked: {
                        deleteSessionId = fileSyncPage.selectedSessionId;
                        deleteConfirmDialog.open();
                    }
                }
            }
        }
    }

    // ---- Delete confirmation dialog ----
    property string deleteSessionId: ""

    Dialog {
        id: deleteConfirmDialog
        title: "Delete Sync Session"
        modal: true
        anchors.centerIn: parent
        width: 360

        Material.background: CoderTheme.surface
        Material.foreground: CoderTheme.textPrimary

        contentItem: Label {
            text: "Are you sure you want to delete this sync session?\nThis action cannot be undone."
            color: CoderTheme.textPrimary
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        footer: DialogButtonBox {
            CoderButton {
                text: "Cancel"
                variant: "outline"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            CoderButton {
                text: "Delete"
                variant: "destructive"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }

        onAccepted: {
            fileSyncManager.deleteSession(fileSyncPage.deleteSessionId);
            fileSyncPage.selectedIndex = -1;
            fileSyncPage.deleteSessionId = "";
        }

        onRejected: {
            fileSyncPage.deleteSessionId = "";
        }
    }

    // ---- New session dialog ----
    FileSyncSessionDialog {
        id: newSessionDialog
    }
}
