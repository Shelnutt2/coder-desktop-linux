import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

Item {
    id: secureDevPage

    property bool dlpSectionVisible: dlpCompositor.available && settingsManager.dlpEnabled

    // Clip content to this item's bounds so nothing bleeds into other tabs
    // when using visibility-based page switching in Main.qml.
    clip: true

    Rectangle {
        anchors.fill: parent
        color: CoderTheme.background
    }

    Label {
        id: pageHeader
        text: "Secure Development"
        font.pixelSize: 20
        font.bold: true
        color: CoderTheme.textPrimary
        padding: 16
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
    }

    ScrollView {
        anchors.top: pageHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 12

            // ---- Availability banner ----
            Rectangle {
                Layout.fillWidth: true
                Layout.margins: 16
                height: availBannerCol.implicitHeight + 24
                radius: CoderTheme.radius
                color: dlpCompositor.available ? CoderTheme.successSurface : CoderTheme.warningSurface
                visible: true

                ColumnLayout {
                    id: availBannerCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    Label {
                        text: dlpCompositor.available
                            ? "✓ Display session detected — Secure Development is available"
                            : "⚠ Secure Development requires a Wayland or X11 session"
                        font.bold: true
                        color: dlpCompositor.available ? CoderTheme.success : CoderTheme.warning
                    }
                    Label {
                        text: dlpCompositor.available
                            ? "The nested compositor can enforce clipboard, screenshot, and sandbox policies."
                            : "No display server detected. Secure Development features are disabled."
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        color: CoderTheme.textSecondary
                    }
                }
            }

            // ---- Security level indicator ----
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                height: secLevelCol.implicitHeight + 24
                radius: CoderTheme.radius
                color: dlpCompositor.securityLevel === "full" ? CoderTheme.successSurface : CoderTheme.warningSurface
                visible: dlpCompositor.available

                ColumnLayout {
                    id: secLevelCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    Label {
                        text: dlpCompositor.securityLevel === "full"
                            ? "🛡 Full DLP protection — Wayland session"
                            : "⚠ Partial DLP protection — X11 host"
                        font.bold: true
                        color: dlpCompositor.securityLevel === "full" ? CoderTheme.success : CoderTheme.warning
                    }
                    Label {
                        text: dlpCompositor.securityLevel === "full"
                            ? "All DLP protections are fully enforced on this Wayland session."
                            : "Clipboard isolation is enforced. Screenshot protection is limited on X11 hosts."
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        color: CoderTheme.textSecondary
                    }
                }
            }

            // ---- Not enabled message ----
            Rectangle {
                Layout.fillWidth: true
                Layout.margins: 16
                height: notEnabledCol.implicitHeight + 24
                radius: CoderTheme.radius
                color: CoderTheme.warningSurface
                visible: dlpCompositor.available && !settingsManager.dlpEnabled

                ColumnLayout {
                    id: notEnabledCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    Label {
                        text: "Secure Development is not enabled"
                        font.bold: true
                        color: CoderTheme.warning
                    }
                    Label {
                        text: "Enable Data Loss Prevention in Settings to use the secure compositor."
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        color: CoderTheme.textSecondary
                    }
                }
            }

            // ---- Active Policies (read-only summary) ----
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                radius: CoderTheme.radius
                color: CoderTheme.surface
                border.color: CoderTheme.border
                border.width: 1
                implicitHeight: policyCol.implicitHeight + 24
                visible: dlpSectionVisible

                ColumnLayout {
                    id: policyCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Label {
                        text: "ACTIVE POLICIES"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: CoderTheme.textSecondary
                    }

                    GridLayout {
                        columns: 2
                        columnSpacing: 16
                        rowSpacing: 6
                        Layout.fillWidth: true

                        Label { text: "Clipboard:"; color: CoderTheme.textPrimary }
                        Label {
                            text: settingsManager.dlpClipboardBlock ? "Blocked" : "Allowed"
                            font.bold: true
                            color: settingsManager.dlpClipboardBlock ? CoderTheme.success : CoderTheme.textSecondary
                        }

                        Label { text: "Screenshots:"; color: CoderTheme.textPrimary }
                        Label {
                            text: "Blocked (always)"
                            font.bold: true
                            color: CoderTheme.success
                        }

                        Label { text: "File sandbox:"; color: CoderTheme.textPrimary }
                        Label {
                            text: settingsManager.dlpFileSandbox ? "Enabled" : "Disabled"
                            font.bold: true
                            color: settingsManager.dlpFileSandbox ? CoderTheme.success : CoderTheme.textSecondary
                        }

                        Label { text: "Network sandbox:"; color: CoderTheme.textPrimary }
                        Label {
                            text: settingsManager.dlpNetworkSandbox ? "Enabled" : "Disabled"
                            font.bold: true
                            color: settingsManager.dlpNetworkSandbox ? CoderTheme.success : CoderTheme.textSecondary
                        }

                        Label { text: "Watermark:"; color: CoderTheme.textPrimary }
                        Label {
                            text: settingsManager.dlpWatermark ? "Enabled" : "Disabled"
                            font.bold: true
                            color: settingsManager.dlpWatermark ? CoderTheme.success : CoderTheme.textSecondary
                        }

                        Label { text: "D-Bus filter:"; color: CoderTheme.textPrimary }
                        Label {
                            text: settingsManager.dlpDbusFilter ? "Enabled" : "Disabled"
                            font.bold: true
                            color: settingsManager.dlpDbusFilter ? CoderTheme.success : CoderTheme.textSecondary
                        }

                        Label { text: "In-app browser:"; color: CoderTheme.textPrimary }
                        Label {
                            text: settingsManager.dlpForceInAppBrowser ? "Forced" : "Optional"
                            font.bold: true
                            color: CoderTheme.textPrimary
                        }

                        Label { text: "External browser:"; color: CoderTheme.textPrimary }
                        Label {
                            text: settingsManager.externalBrowserAllowed ? "Allowed" : "Blocked"
                            font.bold: true
                            color: CoderTheme.textPrimary
                        }
                    }

                    Label {
                        text: "Configure in Settings →"
                        font.pixelSize: 13
                        color: CoderTheme.primary
                        Layout.topMargin: 4

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            // Navigation handled by parent
                        }
                    }
                }
            }

            // ---- App Launcher section ----
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                radius: CoderTheme.radius
                color: CoderTheme.surface
                border.color: CoderTheme.border
                border.width: 1
                implicitHeight: appLauncherCol.implicitHeight + 24
                visible: dlpSectionVisible

                ColumnLayout {
                    id: appLauncherCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 10

                    Label {
                        text: "APP LAUNCHER"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: CoderTheme.textSecondary
                    }

                    // Search bar and view toggle
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        TextField {
                            id: searchField
                            placeholderText: "Search apps…"
                            Layout.fillWidth: true
                            color: CoderTheme.textPrimary
                            onTextChanged: appModel.searchQuery = text
                            background: Rectangle {
                                radius: CoderTheme.radiusSm
                                color: CoderTheme.background
                                border.color: searchField.activeFocus ? CoderTheme.primary : CoderTheme.border
                                border.width: 1
                            }
                        }

                        Rectangle {
                            width: viewToggleRow.implicitWidth + 8
                            height: 36
                            radius: CoderTheme.radiusSm
                            color: CoderTheme.surfaceSecondary

                            RowLayout {
                                id: viewToggleRow
                                anchors.centerIn: parent
                                spacing: 2

                                Rectangle {
                                    width: 28; height: 28
                                    radius: CoderTheme.radiusSm
                                    color: !listViewMode ? CoderTheme.primary : "transparent"

                                    Label {
                                        anchors.centerIn: parent
                                        text: "▦"
                                        font.pixelSize: 14
                                        color: !listViewMode ? CoderTheme.textInvert : CoderTheme.textSecondary
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: listViewMode = false
                                    }
                                }

                                Rectangle {
                                    width: 28; height: 28
                                    radius: CoderTheme.radiusSm
                                    color: listViewMode ? CoderTheme.primary : "transparent"

                                    Label {
                                        anchors.centerIn: parent
                                        text: "☰"
                                        font.pixelSize: 14
                                        color: listViewMode ? CoderTheme.textInvert : CoderTheme.textSecondary
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: listViewMode = true
                                    }
                                }
                            }
                        }

                        CoderButton {
                            text: "↻"
                            variant: "outline"
                            onClicked: appModel.refresh()
                            ToolTip.visible: hovered
                            ToolTip.text: "Refresh apps"
                        }
                    }

                    // Category filter
                    CategoryFilterBar {
                        id: categoryFilter
                        Layout.fillWidth: true
                        onCategorySelected: function(category) {
                            appModel.categoryFilter = category;
                        }
                    }

                    // Loading indicator
                    BusyIndicator {
                        Layout.alignment: Qt.AlignHCenter
                        running: appModel.loading
                        visible: appModel.loading
                        Material.accent: CoderTheme.primary
                    }

                    // Grid view (default)
                    GridView {
                        id: appGridView
                        Layout.fillWidth: true
                        Layout.preferredHeight: contentHeight
                        visible: !listViewMode && !appModel.loading
                        interactive: false
                        cellWidth: 130
                        cellHeight: 140

                        model: appModel

                        delegate: AppTile {
                            width: appGridView.cellWidth - 10
                            height: appGridView.cellHeight - 10
                            appName: model.name
                            appExec: model.exec
                            appId: model.appId
                            appCategory: model.category
                            appIconName: model.iconName
                            appIconPath: model.iconPath

                            onLaunched: {
                                launchDialog.appName = model.name;
                                launchDialog.appExec = model.exec;
                                launchDialog.appId = model.appId;
                                launchDialog.appCategory = model.category;
                                launchDialog.appIconName = model.iconName;
                                launchDialog.open();
                            }
                        }
                    }

                    // List view (alternate)
                    ListView {
                        id: appListView
                        Layout.fillWidth: true
                        Layout.preferredHeight: contentHeight
                        visible: listViewMode && !appModel.loading
                        interactive: false
                        spacing: 4

                        model: appModel

                        delegate: Rectangle {
                            width: appListView.width
                            height: 48
                            radius: CoderTheme.radiusSm
                            color: listItemMouse.containsMouse ? CoderTheme.hoverBg : "transparent"

                            MouseArea {
                                id: listItemMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    launchDialog.appName = model.name;
                                    launchDialog.appExec = model.exec;
                                    launchDialog.appId = model.appId;
                                    launchDialog.appCategory = model.category;
                                    launchDialog.appIconName = model.iconName;
                                    launchDialog.open();
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 12

                                // Icon or emoji fallback
                                Item {
                                    width: 32; height: 32

                                    Image {
                                        id: listItemIcon
                                        anchors.centerIn: parent
                                        width: 32; height: 32
                                        sourceSize: Qt.size(32, 32)
                                        source: {
                                            if (model.iconPath !== "")
                                                return "file://" + model.iconPath;
                                            if (model.iconName !== "")
                                                return "image://icon-theme/" + model.iconName;
                                            return "";
                                        }
                                        visible: status === Image.Ready
                                        fillMode: Image.PreserveAspectFit
                                    }
                                    Label {
                                        anchors.centerIn: parent
                                        font.pixelSize: 20
                                        visible: !listItemIcon.visible
                                        text: {
                                            switch (model.category) {
                                                case "IDE":      return "🖥️";
                                                case "Browser":  return "🌐";
                                                case "Editor":   return "📝";
                                                case "AI Tool":  return "🤖";
                                                case "Terminal": return "⌨️";
                                                default:         return "📦";
                                            }
                                        }
                                    }
                                }

                                Label {
                                    text: model.name
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    color: CoderTheme.textPrimary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Rectangle {
                                    width: listCatLabel.implicitWidth + 12
                                    height: listCatLabel.implicitHeight + 4
                                    radius: CoderTheme.radiusSm
                                    color: CoderTheme.surfaceSecondary
                                    visible: model.category !== ""

                                    Label {
                                        id: listCatLabel
                                        anchors.centerIn: parent
                                        text: model.category
                                        font.pixelSize: 10
                                        color: CoderTheme.textSecondary
                                    }
                                }
                            }
                        }
                    }

                    // Empty state
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 16
                        Layout.bottomMargin: 16
                        spacing: 8
                        visible: appModel.totalCount === 0 && !appModel.loading

                        Label {
                            text: "📦"
                            font.pixelSize: 40
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Label {
                            text: "No apps detected"
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            color: CoderTheme.textSecondary
                            Layout.alignment: Qt.AlignHCenter
                        }
                        Label {
                            text: "Install desktop applications or use the custom command below."
                            font.pixelSize: 12
                            color: CoderTheme.textDisabled
                            Layout.alignment: Qt.AlignHCenter
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }

            // ---- Running Apps section ----
            RunningAppsPanel {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: dlpSectionVisible && dlpCompositor.launchedAppCount > 0
                model: dlpCompositor.runningApps
                onStopRequested: function(pid) {
                    dlpCompositor.stopApp(pid);
                }
                onStopAllRequested: dlpCompositor.stopAll()
            }

            // ---- Custom Command section ----
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                radius: CoderTheme.radius
                color: CoderTheme.surface
                border.color: CoderTheme.border
                border.width: 1
                implicitHeight: customCmdCol.implicitHeight + 24
                visible: dlpSectionVisible

                ColumnLayout {
                    id: customCmdCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 8

                    Label {
                        text: "CUSTOM COMMAND"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: CoderTheme.textSecondary
                    }

                    TextField {
                        id: commandField
                        placeholderText: "Command (e.g. firefox, code, xterm)"
                        Layout.fillWidth: true
                        color: CoderTheme.textPrimary
                        background: Rectangle {
                            radius: CoderTheme.radiusSm
                            color: CoderTheme.background
                            border.color: commandField.activeFocus ? CoderTheme.primary : CoderTheme.border
                            border.width: 1
                        }
                    }

                    TextField {
                        id: workspacePathField
                        placeholderText: "Workspace path (optional)"
                        Layout.fillWidth: true
                        color: CoderTheme.textPrimary
                        background: Rectangle {
                            radius: CoderTheme.radiusSm
                            color: CoderTheme.background
                            border.color: workspacePathField.activeFocus ? CoderTheme.primary : CoderTheme.border
                            border.width: 1
                        }
                    }

                    // Collapsible sandbox options
                    Rectangle {
                        Layout.fillWidth: true
                        height: sandboxToggleLabel.implicitHeight + 16
                        radius: CoderTheme.radiusSm
                        color: sandboxToggleMouse.containsMouse ? CoderTheme.hoverBg : "transparent"

                        RowLayout {
                            id: sandboxToggleLabel
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 4

                            Label {
                                text: customSandboxExpanded ? "▾ Sandbox Options" : "▸ Sandbox Options"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                color: CoderTheme.textSecondary
                            }
                            Item { Layout.fillWidth: true }
                        }

                        MouseArea {
                            id: sandboxToggleMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: customSandboxExpanded = !customSandboxExpanded
                        }
                    }

                    RowLayout {
                        spacing: 16
                        visible: customSandboxExpanded

                        CheckBox {
                            id: customPidCheck
                            text: "Isolate PID"
                            checked: true
                            Material.accent: CoderTheme.primary
                        }
                        CheckBox {
                            id: customIpcCheck
                            text: "Isolate IPC"
                            checked: true
                            Material.accent: CoderTheme.primary
                        }
                        CheckBox {
                            id: customNetCheck
                            text: "Isolate Network"
                            checked: false
                            Material.accent: CoderTheme.primary
                        }
                        CheckBox {
                            id: customFsCheck
                            text: "Isolate Filesystem"
                            checked: false
                            Material.accent: CoderTheme.primary
                        }
                    }

                    CoderButton {
                        text: "Launch"
                        variant: "default"
                        enabled: commandField.text.length > 0
                        onClicked: {
                            dlpCompositor.launchApp(
                                commandField.text,
                                "",
                                workspacePathField.text,
                                customPidCheck.checked,
                                customIpcCheck.checked,
                                customNetCheck.checked,
                                customFsCheck.checked,
                                false,
                                [],
                                settingsManager.dlpDbusFilter
                            );
                            commandField.text = "";
                        }
                    }
                }
            }

            // ---- Spacer ----
            Item { Layout.fillHeight: true }
        }
    }

    // ---- Local state ----
    property bool listViewMode: false
    property bool customSandboxExpanded: false

    // ---- Launch Dialog ----
    LaunchDialog {
        id: launchDialog
        onLaunchRequested: function(command, appName, workspacePath, pid, ipc, net, fs, homeRw) {
            dlpCompositor.launchApp(command, appName, workspacePath, pid, ipc, net, fs, homeRw, [], settingsManager.dlpDbusFilter, settingsManager.dlpDbusAllowedNames);
        }
    }

    // ---- Error handling ----
    Connections {
        target: dlpCompositor
        function onErrorOccurred(message) {
            errorLabel.text = message;
            errorLabel.visible = true;
            errorTimer.restart();
        }
    }

    Label {
        id: errorLabel
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        visible: false
        color: CoderTheme.error
        wrapMode: Text.WordWrap
        font.bold: true
        padding: 8
        background: Rectangle { color: CoderTheme.errorSurface; radius: CoderTheme.radiusSm }
    }

    Timer {
        id: errorTimer
        interval: 5000
        onTriggered: errorLabel.visible = false
    }
}
