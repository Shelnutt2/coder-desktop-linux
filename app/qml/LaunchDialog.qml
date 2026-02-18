import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

Dialog {
    id: dialog

    property string appName: ""
    property string appExec: ""
    property string appId: ""
    property string appCategory: ""
    property string appIconName: ""

    signal launchRequested(string command, string appName, string workspacePath,
                           bool pid, bool ipc, bool net, bool fs, bool homeRw)

    title: "Launch " + appName
    modal: true
    standardButtons: Dialog.NoButton
    anchors.centerIn: parent
    width: 420

    Material.background: CoderTheme.surface

    function setDefaults() {
        workspaceField.text = "";
        // Browsers and Electron IDEs: relaxed
        if (appCategory === "Browser" ||
            ["code", "code-insiders", "cursor", "codium", "vscode"].indexOf(appId) >= 0) {
            pidCheck.checked = false;
            ipcCheck.checked = false;
            netCheck.checked = false;
            fsCheck.checked = false;
            homeCheck.checked = false;
        }
        // JetBrains IDEs
        else if (["idea", "goland", "pycharm", "webstorm", "clion",
                  "phpstorm", "rider", "rubymine", "datagrip"].indexOf(appId) >= 0) {
            pidCheck.checked = false;
            ipcCheck.checked = true;
            netCheck.checked = false;
            fsCheck.checked = false;
            homeCheck.checked = true;
        }
        // Everything else: conservative defaults
        else {
            pidCheck.checked = true;
            ipcCheck.checked = true;
            netCheck.checked = false;
            fsCheck.checked = false;
            homeCheck.checked = false;
        }
    }

    onAboutToShow: {
        setDefaults();
    }

    contentItem: ColumnLayout {
        spacing: 12

        // App header
        RowLayout {
            spacing: 12

            Label {
                font.pixelSize: 32
                text: {
                    switch (dialog.appCategory) {
                        case "IDE":      return "🖥️";
                        case "Browser":  return "🌐";
                        case "Editor":   return "📝";
                        case "AI Tool":  return "🤖";
                        case "Terminal": return "⌨️";
                        default:         return "📦";
                    }
                }
            }

            ColumnLayout {
                spacing: 2

                Label {
                    text: dialog.appName
                    font.pixelSize: 16
                    font.bold: true
                    color: CoderTheme.textPrimary
                }
                Label {
                    text: dialog.appExec
                    font.pixelSize: 12
                    color: CoderTheme.textSecondary
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: CoderTheme.divider
        }

        // Sandbox toggles
        Label {
            text: "SANDBOX OPTIONS"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: CoderTheme.textSecondary
        }

        GridLayout {
            columns: 2
            columnSpacing: 16
            rowSpacing: 4
            Layout.fillWidth: true

            CheckBox {
                id: pidCheck
                text: "Isolate PID"
                Material.accent: CoderTheme.primary
            }
            CheckBox {
                id: ipcCheck
                text: "Isolate IPC"
                Material.accent: CoderTheme.primary
            }
            CheckBox {
                id: netCheck
                text: "Isolate Network"
                Material.accent: CoderTheme.primary
            }
            CheckBox {
                id: fsCheck
                text: "Isolate Filesystem"
                Material.accent: CoderTheme.primary
            }
            CheckBox {
                id: homeCheck
                text: "Bind Home (R/W)"
                Material.accent: CoderTheme.primary
            }
        }

        // Workspace path
        Label {
            text: "Workspace Path"
            font.pixelSize: 12
            color: CoderTheme.textSecondary
        }

        TextField {
            id: workspaceField
            placeholderText: "/home/user/project (optional)"
            Layout.fillWidth: true
            color: CoderTheme.textPrimary
            background: Rectangle {
                radius: CoderTheme.radiusSm
                color: CoderTheme.background
                border.color: workspaceField.activeFocus ? CoderTheme.primary : CoderTheme.border
                border.width: 1
            }
        }

        // Buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            CoderButton {
                text: "Cancel"
                variant: "outline"
                onClicked: dialog.close()
            }

            CoderButton {
                text: "Launch"
                variant: "default"
                onClicked: {
                    dialog.launchRequested(
                        dialog.appExec,
                        dialog.appName,
                        workspaceField.text,
                        pidCheck.checked,
                        ipcCheck.checked,
                        netCheck.checked,
                        fsCheck.checked,
                        homeCheck.checked
                    );
                    dialog.close();
                }
            }
        }
    }
}
