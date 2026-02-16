import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Settings page with three-layer settings (MDM → User → Default).
// Locked settings show a lock icon and "Managed by your organization".
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

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 0

            // ================================================================
            // Deployment
            // ================================================================
            Label {
                text: "Deployment"
                font.pixelSize: 13
                font.bold: true
                opacity: 0.6
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.bottomMargin: 4
            }

            // Current URL (read-only)
            Pane {
                Layout.fillWidth: true
                padding: 16

                ColumnLayout {
                    width: parent.width - 32
                    spacing: 4

                    Label {
                        text: "Current Deployment"
                        font.pixelSize: 14
                    }
                    Label {
                        text: sessionManager.currentUrl || "Not connected"
                        font.pixelSize: 13
                        opacity: 0.6
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }
            }

            // Sign Out button
            Pane {
                Layout.fillWidth: true
                padding: 16

                Button {
                    text: "Sign Out"
                    flat: true
                    Material.foreground: Material.Red
                    onClicked: sessionManager.logout()
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Material.dividerColor }

            // ================================================================
            // VPN
            // ================================================================
            Label {
                text: "VPN"
                font.pixelSize: 13
                font.bold: true
                opacity: 0.6
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.bottomMargin: 4
            }

            // Auto-connect VPN toggle
            SettingToggle {
                label: "Auto-connect VPN on launch"
                settingKey: "autoConnectVpn"
                checked: settingsManager.autoConnectVpn
                locked: settingsManager.autoConnectVpnLocked
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Material.dividerColor }

            // ================================================================
            // Data Loss Prevention
            // ================================================================
            Label {
                text: "Data Loss Prevention"
                font.pixelSize: 13
                font.bold: true
                opacity: 0.6
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.bottomMargin: 4
            }

            // Wayland-required info banner
            Rectangle {
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.bottomMargin: 8
                height: dlpInfoLabel.implicitHeight + 16
                radius: 4
                color: Material.color(Material.Blue, Material.Shade50)
                visible: !settingsPage.isWayland

                Label {
                    id: dlpInfoLabel
                    anchors.fill: parent
                    anchors.margins: 8
                    text: "ℹ DLP requires a Wayland session"
                    font.pixelSize: 13
                    color: Material.color(Material.Blue, Material.Shade900)
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

            Rectangle { Layout.fillWidth: true; height: 1; color: Material.dividerColor }

            // ================================================================
            // Data & Refresh
            // ================================================================
            Label {
                text: "Data & Refresh"
                font.pixelSize: 13
                font.bold: true
                opacity: 0.6
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.bottomMargin: 4
            }

            // Refresh interval
            Pane {
                Layout.fillWidth: true
                padding: 16

                RowLayout {
                    width: parent.width - 32
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            spacing: 6
                            Label {
                                text: "Auto-refresh interval"
                                font.pixelSize: 14
                            }
                            Label {
                                text: "🔒"
                                font.pixelSize: 12
                                visible: settingsManager.refreshIntervalSecLocked
                            }
                        }
                        Label {
                            text: settingsManager.refreshIntervalSecLocked
                                ? "Managed by your organization"
                                : refreshSlider.value + " seconds"
                            font.pixelSize: 11
                            opacity: 0.5
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
                        onMoved: settingsManager.setUserPreference("refreshIntervalSec", value)
                    }
                }
            }

            // Cache toggle (inverted: toggle ON = caching enabled = disableDataCache is false)
            Pane {
                Layout.fillWidth: true
                padding: 16

                RowLayout {
                    width: parent.width - 32
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            spacing: 6
                            Label {
                                text: "Cache workspace & task data"
                                font.pixelSize: 14
                            }
                            Label {
                                text: "🔒"
                                font.pixelSize: 12
                                visible: settingsManager.disableDataCacheLocked
                            }
                        }
                        Label {
                            text: settingsManager.disableDataCacheLocked
                                ? "Managed by your organization"
                                : "Store last-known data for faster startup"
                            font.pixelSize: 11
                            opacity: 0.5
                        }
                    }

                    Switch {
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        checked: !settingsManager.disableDataCache
                        enabled: !settingsManager.disableDataCacheLocked
                        onToggled: settingsManager.setUserPreference("disableDataCache", !checked)
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Material.dividerColor }

            // ================================================================
            // Appearance
            // ================================================================
            Label {
                text: "Appearance"
                font.pixelSize: 13
                font.bold: true
                opacity: 0.6
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.bottomMargin: 4
            }

            // Theme selector
            Pane {
                Layout.fillWidth: true
                padding: 16

                RowLayout {
                    width: parent.width - 32
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            spacing: 6
                            Label {
                                text: "Theme"
                                font.pixelSize: 14
                            }
                            Label {
                                text: "🔒"
                                font.pixelSize: 12
                                visible: settingsManager.themeLocked
                            }
                        }
                        Label {
                            text: settingsManager.themeLocked
                                ? "Managed by your organization"
                                : ""
                            font.pixelSize: 11
                            opacity: 0.5
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

            Rectangle { Layout.fillWidth: true; height: 1; color: Material.dividerColor }

            // ================================================================
            // About
            // ================================================================
            Label {
                text: "About"
                font.pixelSize: 13
                font.bold: true
                opacity: 0.6
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.bottomMargin: 4
            }

            Pane {
                Layout.fillWidth: true
                padding: 16

                ColumnLayout {
                    spacing: 4

                    Label {
                        text: "Coder Desktop for Linux"
                        font.pixelSize: 14
                        font.bold: true
                    }
                    Label {
                        text: "App version: " + Qt.application.version
                        font.pixelSize: 12
                        opacity: 0.6
                    }
                    Label {
                        text: "Qt version: " + qtVersion
                        font.pixelSize: 12
                        opacity: 0.6
                        // qtVersion is available as a QML global in Qt 6
                        property string qtVersion: "6.x"
                    }
                }
            }

            // Bottom spacer
            Item { Layout.preferredHeight: 32 }
        }
    }

    // ---- Reusable toggle row component ----
    component SettingToggle: Pane {
        id: togglePane
        property string label: ""
        property string settingKey: ""
        property bool checked: false
        property bool locked: false

        Layout.fillWidth: true
        padding: 16

        RowLayout {
            width: parent.width - 32
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                RowLayout {
                    spacing: 6
                    Label {
                        text: togglePane.label
                        font.pixelSize: 14
                    }
                    Label {
                        text: "🔒"
                        font.pixelSize: 12
                        visible: togglePane.locked
                    }
                }

                Label {
                    text: "Managed by your organization"
                    font.pixelSize: 11
                    opacity: 0.5
                    visible: togglePane.locked
                }
            }

            Switch {
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                checked: togglePane.checked
                enabled: !togglePane.locked
                onToggled: settingsManager.setUserPreference(togglePane.settingKey, checked)
            }
        }
    }
}
