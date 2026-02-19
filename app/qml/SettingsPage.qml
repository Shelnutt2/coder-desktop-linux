import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

// Settings page with three-layer settings (MDM → User → Default).
// Android-style sectioned list with uppercase headers and CoderTheme.
Item {
    id: settingsPage

    // Helper: detect Wayland session
    readonly property bool isWayland: {
        var session = typeof Qt.application !== "undefined"
            ? (typeof Qt.application.platformName !== "undefined"
               ? Qt.application.platformName : "")
            : ""
        return session.toLowerCase().indexOf("wayland") >= 0
    }

    signal navigateToConnect()

    // Whether to show the deployment subpage
    property bool showDeployments: false

    // ---- Sign Out confirmation dialog ----
    Dialog {
        id: signOutConfirmDialog
        title: "Sign out?"
        anchors.centerIn: Overlay.overlay
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Material.accent: CoderTheme.primary

        Label {
            text: "Are you sure you want to sign out? Your session credentials will be removed."
            wrapMode: Text.WordWrap
            width: parent.width
        }

        onAccepted: sessionManager.logout()
    }

    // When showDeployments is true, load DeploymentPage on top
    Loader {
        id: deploymentLoader
        anchors.fill: parent
        active: settingsPage.showDeployments
        sourceComponent: Component {
            DeploymentPage {
                onBackRequested: settingsPage.showDeployments = false
            }
        }
        z: 1
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        visible: !settingsPage.showDeployments

        ColumnLayout {
            width: parent.width
            spacing: 0

            // ================================================================
            // MDM STATUS BANNER
            // ================================================================
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 16
                Layout.bottomMargin: 8
                height: mdmBannerLabel.implicitHeight + 16
                radius: CoderTheme.radiusSm
                color: "#e8f0fe"
                visible: settingsManager.mdmEnabled

                Label {
                    id: mdmBannerLabel
                    anchors.fill: parent
                    anchors.margins: 8
                    text: "ℹ Some settings are managed by your organization."
                    font.pixelSize: 13
                    color: "#1a56db"
                    wrapMode: Text.WordWrap
                }
            }

            // ================================================================
            // ACCOUNT
            // ================================================================
            Label {
                text: "ACCOUNT"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: CoderTheme.textSecondary
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.topMargin: 24
                Layout.bottomMargin: 8
            }

            // Deployment URL (read-only) with copy button
            Rectangle {
                Layout.fillWidth: true
                color: "transparent"
                implicitHeight: deployUrlRow.implicitHeight + 24

                RowLayout {
                    id: deployUrlRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: "Deployment URL"
                            font.pixelSize: 14
                            color: CoderTheme.textPrimary
                        }
                        Label {
                            id: deployUrlLabel
                            text: sessionManager.currentUrl || "Not connected"
                            font.pixelSize: 13
                            color: CoderTheme.textSecondary
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }

                    Button {
                        flat: true
                        visible: (sessionManager.currentUrl || "").length > 0
                        contentItem: Label {
                            text: copyUrlTimer.running ? "✓" : "📋"
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            implicitWidth: 36; implicitHeight: 36
                            radius: CoderTheme.radius
                            color: parent.hovered ? CoderTheme.hoverBg : "transparent"
                        }
                        onClicked: {
                            clipboardHelper.text = sessionManager.currentUrl
                            clipboardHelper.selectAll()
                            clipboardHelper.copy()
                            copyUrlTimer.restart()
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: "Copy URL to clipboard"
                    }

                    // Hidden TextEdit for clipboard access
                    TextEdit {
                        id: clipboardHelper
                        visible: false
                    }

                    Timer {
                        id: copyUrlTimer
                        interval: 1500
                    }
                }
            }

            // Username (read-only)
            Rectangle {
                Layout.fillWidth: true
                color: "transparent"
                implicitHeight: usernameCol.implicitHeight + 24

                ColumnLayout {
                    id: usernameCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Label {
                        text: "Username"
                        font.pixelSize: 14
                        color: CoderTheme.textPrimary
                    }
                    Label {
                        text: sessionManager.username || "—"
                        font.pixelSize: 13
                        color: CoderTheme.textSecondary
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }

            // Manage Deployments →
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 48
                color: manageDeployHover.hovered ? CoderTheme.hoverBg : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16

                    Label {
                        text: "Manage Deployments"
                        font.pixelSize: 14
                        color: CoderTheme.primary
                        Layout.fillWidth: true
                    }
                    Label {
                        text: "→"
                        font.pixelSize: 14
                        color: CoderTheme.primary
                    }
                }

                HoverHandler { id: manageDeployHover }
                TapHandler { onTapped: settingsPage.showDeployments = true }
            }

            // Sign Out
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 48
                color: "transparent"
                Layout.leftMargin: 16
                Layout.bottomMargin: 8

                CoderButton {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Sign Out"
                    variant: "destructive"
                    onClicked: signOutConfirmDialog.open()
                }
            }

            // ---- section divider ----
            Rectangle { Layout.fillWidth: true; height: 1; color: CoderTheme.divider }

            // ================================================================
            // CONNECTION
            // ================================================================
            Label {
                text: "CONNECTION"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: CoderTheme.textSecondary
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.topMargin: 24
                Layout.bottomMargin: 8
            }

            // Coder Connect (VPN) → link
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 48
                color: vpnLinkHover.hovered ? CoderTheme.hoverBg : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16

                    Label {
                        text: "Coder Connect (VPN)"
                        font.pixelSize: 14
                        color: CoderTheme.primary
                        Layout.fillWidth: true
                    }
                    Label {
                        text: "→"
                        font.pixelSize: 14
                        color: CoderTheme.primary
                    }
                }

                HoverHandler { id: vpnLinkHover }
                // Navigation to Connect tab handled by page-level signal
                TapHandler { onTapped: settingsPage.navigateToConnect() }
            }

            // Auto-connect on launch toggle
            SettingToggle {
                label: "Auto-connect on launch"
                settingKey: "autoConnectVpn"
                checked: settingsManager.autoConnectVpn
                locked: settingsManager.autoConnectVpnLocked
            }

            // Require VPN toggle
            SettingToggle {
                label: "Require VPN connection"
                settingKey: "requireVpn"
                checked: settingsManager.requireVpn
                locked: settingsManager.requireVpnLocked
            }

            // ---- section divider ----
            Rectangle { Layout.fillWidth: true; height: 1; color: CoderTheme.divider }

            // ================================================================
            // DATA LOSS PREVENTION
            // ================================================================
            Label {
                text: "DATA LOSS PREVENTION"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: CoderTheme.textSecondary
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.topMargin: 24
                Layout.bottomMargin: 8
            }

            // Wayland-required info banner
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 8
                height: dlpInfoLabel.implicitHeight + 16
                radius: CoderTheme.radiusSm
                color: CoderTheme.warningSurface
                visible: !settingsPage.isWayland

                Label {
                    id: dlpInfoLabel
                    anchors.fill: parent
                    anchors.margins: 8
                    text: "ℹ DLP requires a Wayland session"
                    font.pixelSize: 13
                    color: CoderTheme.warning
                    wrapMode: Text.WordWrap
                }
            }

            SettingToggle {
                label: "Enable DLP"
                settingKey: "dlpEnabled"
                checked: settingsManager.dlpEnabled
                locked: settingsManager.dlpEnabledLocked
            }

            SettingToggle {
                label: "Block clipboard access"
                settingKey: "dlpClipboardBlock"
                checked: settingsManager.dlpClipboardBlock
                locked: settingsManager.dlpClipboardBlockLocked
            }

            SettingToggle {
                label: "Block screenshots"
                settingKey: "dlpScreenshotBlock"
                checked: settingsManager.dlpScreenshotBlock
                locked: settingsManager.dlpScreenshotBlockLocked
            }

            SettingToggle {
                label: "File sandbox"
                settingKey: "dlpFileSandbox"
                checked: settingsManager.dlpFileSandbox
                locked: settingsManager.dlpFileSandboxLocked
            }

            SettingToggle {
                label: "Network sandbox"
                settingKey: "dlpNetworkSandbox"
                checked: settingsManager.dlpNetworkSandbox
                locked: settingsManager.dlpNetworkSandboxLocked
            }

            SettingToggle {
                label: "Force in-app browser"
                settingKey: "dlpForceInAppBrowser"
                checked: settingsManager.dlpForceInAppBrowser
                locked: settingsManager.dlpForceInAppBrowserLocked
            }

            SettingToggle {
                label: "Disable external browser"
                settingKey: "dlpDisableExternalBrowser"
                checked: settingsManager.dlpDisableExternalBrowser
                locked: settingsManager.dlpDisableExternalBrowserLocked
            }

            SettingToggle {
                label: "Block file uploads"
                settingKey: "disableFileUpload"
                checked: settingsManager.disableFileUpload
                locked: settingsManager.disableFileUploadLocked
            }

            SettingToggle {
                label: "Block file downloads"
                settingKey: "disableFileDownload"
                checked: settingsManager.disableFileDownload
                locked: settingsManager.disableFileDownloadLocked
            }

            // ---- section divider ----
            Rectangle { Layout.fillWidth: true; height: 1; color: CoderTheme.divider }

            // ================================================================
            // APPEARANCE
            // ================================================================
            Label {
                text: "APPEARANCE"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: CoderTheme.textSecondary
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.topMargin: 24
                Layout.bottomMargin: 8
            }

            // Theme selector
            Rectangle {
                Layout.fillWidth: true
                color: "transparent"
                implicitHeight: 56

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            spacing: 6
                            Label {
                                text: "Theme"
                                font.pixelSize: 14
                                color: CoderTheme.textPrimary
                            }
                            Label {
                                text: "🔒"
                                font.pixelSize: 12
                                color: CoderTheme.textSecondary
                                visible: settingsManager.themeLocked
                            }
                        }
                        Label {
                            text: "Managed by your organization"
                            font.pixelSize: 11
                            color: CoderTheme.textSecondary
                            visible: settingsManager.themeLocked
                        }
                    }

                    ComboBox {
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        model: ["System", "Light", "Dark"]
                        currentIndex: {
                            var t = settingsManager.theme
                            if (t === "Light") return 1
                            if (t === "Dark") return 2
                            return 0
                        }
                        enabled: !settingsManager.themeLocked
                        onActivated: function(index) {
                            var values = ["System", "Light", "Dark"]
                            settingsManager.setUserPreference("theme", values[index])
                        }
                    }
                }
            }

            // Notifications toggle
            SettingToggle {
                label: "Desktop notifications"
                settingKey: "notificationsEnabled"
                checked: settingsManager.notificationsEnabled
                locked: settingsManager.notificationsEnabledLocked
            }

            // ---- section divider ----
            Rectangle { Layout.fillWidth: true; height: 1; color: CoderTheme.divider }

            // ================================================================
            // DATA & REFRESH
            // ================================================================
            Label {
                text: "DATA & REFRESH"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: CoderTheme.textSecondary
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.topMargin: 24
                Layout.bottomMargin: 8
            }

            // Refresh interval
            Rectangle {
                Layout.fillWidth: true
                color: "transparent"
                implicitHeight: 56

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            spacing: 6
                            Label {
                                text: "Auto-refresh interval"
                                font.pixelSize: 14
                                color: CoderTheme.textPrimary
                            }
                            Label {
                                text: "🔒"
                                font.pixelSize: 12
                                color: CoderTheme.textSecondary
                                visible: settingsManager.refreshIntervalSecLocked
                            }
                        }
                        Label {
                            text: settingsManager.refreshIntervalSecLocked
                                ? "Managed by your organization"
                                : refreshSlider.value + " seconds"
                            font.pixelSize: 11
                            color: CoderTheme.textSecondary
                        }
                    }

                    Slider {
                        id: refreshSlider
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        Layout.preferredWidth: 140
                        from: 5
                        to: 60
                        stepSize: 5
                        value: settingsManager.refreshIntervalSec
                        enabled: !settingsManager.refreshIntervalSecLocked
                        Material.accent: CoderTheme.primary
                        onMoved: settingsManager.setUserPreference("refreshIntervalSec", value)
                    }
                }
            }

            // Cache toggle (inverted: toggle ON = caching enabled = disableDataCache is false)
            Rectangle {
                Layout.fillWidth: true
                color: "transparent"
                implicitHeight: 56

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            spacing: 6
                            Label {
                                text: "Cache workspace & task data"
                                font.pixelSize: 14
                                color: CoderTheme.textPrimary
                            }
                            Label {
                                text: "🔒"
                                font.pixelSize: 12
                                color: CoderTheme.textSecondary
                                visible: settingsManager.disableDataCacheLocked
                            }
                        }
                        Label {
                            text: settingsManager.disableDataCacheLocked
                                ? "Managed by your organization"
                                : "Store last-known data for faster startup"
                            font.pixelSize: 11
                            color: CoderTheme.textSecondary
                        }
                    }

                    Switch {
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        checked: !settingsManager.disableDataCache
                        enabled: !settingsManager.disableDataCacheLocked
                        Material.accent: CoderTheme.primary
                        onToggled: settingsManager.setUserPreference("disableDataCache", !checked)
                    }
                }
            }

            // Log level selector
            Rectangle {
                Layout.fillWidth: true
                color: "transparent"
                implicitHeight: 56

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            spacing: 6
                            Label {
                                text: "Log level"
                                font.pixelSize: 14
                                color: CoderTheme.textPrimary
                            }
                            Label {
                                text: "🔒"
                                font.pixelSize: 12
                                color: CoderTheme.textSecondary
                                visible: settingsManager.logLevelLocked
                            }
                        }
                        Label {
                            text: settingsManager.logLevelLocked
                                ? "Managed by your organization"
                                : "Set application logging verbosity"
                            font.pixelSize: 11
                            color: CoderTheme.textSecondary
                        }
                    }

                    ComboBox {
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        model: ["Debug", "Info", "Warning", "Error"]
                        currentIndex: {
                            var l = settingsManager.logLevel.toLowerCase()
                            if (l === "debug") return 0
                            if (l === "warning") return 2
                            if (l === "error") return 3
                            return 1  // default: Info
                        }
                        enabled: !settingsManager.logLevelLocked
                        onActivated: function(index) {
                            var values = ["debug", "info", "warning", "error"]
                            settingsManager.setUserPreference("logLevel", values[index])
                        }
                    }
                }
            }

            // ---- section divider ----
            Rectangle { Layout.fillWidth: true; height: 1; color: CoderTheme.divider }

            // ================================================================
            // ABOUT
            // ================================================================
            Label {
                text: "ABOUT"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: CoderTheme.textSecondary
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.topMargin: 24
                Layout.bottomMargin: 8
            }

            Rectangle {
                Layout.fillWidth: true
                color: "transparent"
                implicitHeight: aboutCol.implicitHeight + 32

                ColumnLayout {
                    id: aboutCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Image {
                        source: "qrc:/CoderDesktop/assets/coder-icon.svg"
                        sourceSize.width: 48
                        sourceSize.height: 48
                        Layout.alignment: Qt.AlignLeft
                    }

                    Label {
                        text: "Coder Desktop for Linux"
                        font.pixelSize: 16
                        font.bold: true
                        color: CoderTheme.textPrimary
                    }
                    Label {
                        text: "Version " + Qt.application.version
                        font.pixelSize: 12
                        color: CoderTheme.textSecondary
                    }
                    Label {
                        text: "Qt " + qtRuntimeVersion
                        font.pixelSize: 12
                        color: CoderTheme.textSecondary
                    }

                    // View on GitHub →
                    Label {
                        text: "View on GitHub →"
                        font.pixelSize: 13
                        color: CoderTheme.primary
                        Layout.topMargin: 4

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally("https://github.com/coder/coder-desktop-linux")
                        }
                    }
                }
            }

            // Check for updates toggle
            SettingToggle {
                label: "Check for updates"
                settingKey: "checkForUpdates"
                checked: settingsManager.checkForUpdates
                locked: settingsManager.checkForUpdatesLocked
            }

            // ---- section divider ----
            Rectangle { Layout.fillWidth: true; height: 1; color: CoderTheme.divider }

            // ================================================================
            // RESET
            // ================================================================
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.bottomMargin: 8
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                implicitHeight: 48
                color: "transparent"

                CoderButton {
                    anchors.centerIn: parent
                    text: "Reset to Defaults"
                    variant: "destructive"
                    onClicked: resetConfirmDialog.open()
                }
            }

            Dialog {
                id: resetConfirmDialog
                title: "Reset to Defaults"
                modal: true
                anchors.centerIn: parent
                standardButtons: Dialog.Ok | Dialog.Cancel

                Label {
                    text: "Reset all non-managed settings to their defaults?\nThis cannot be undone."
                    wrapMode: Text.WordWrap
                }

                onAccepted: settingsManager.resetToDefaults()
            }

            // Bottom spacer
            Item { Layout.preferredHeight: 32 }
        }
    }

    // ---- Reusable toggle row component ----
    component SettingToggle: Rectangle {
        id: togglePane
        property string label: ""
        property string settingKey: ""
        property bool checked: false
        property bool locked: false

        Layout.fillWidth: true
        implicitHeight: 56
        color: toggleHover.hovered ? CoderTheme.hoverBg : "transparent"

        HoverHandler { id: toggleHover }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                RowLayout {
                    spacing: 6
                    Label {
                        text: togglePane.label
                        font.pixelSize: 14
                        color: CoderTheme.textPrimary
                    }
                    Label {
                        text: "🔒"
                        font.pixelSize: 12
                        color: CoderTheme.textSecondary
                        visible: togglePane.locked
                    }
                }

                Label {
                    text: "Managed by your organization"
                    font.pixelSize: 11
                    color: CoderTheme.textSecondary
                    visible: togglePane.locked
                }
            }

            Switch {
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                checked: togglePane.checked
                enabled: !togglePane.locked
                Material.accent: CoderTheme.primary
                onToggled: settingsManager.setUserPreference(togglePane.settingKey, checked)
            }
        }
    }
}
