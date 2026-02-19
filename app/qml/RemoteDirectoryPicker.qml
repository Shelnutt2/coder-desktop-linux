import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

Dialog {
    id: dirPicker

    title: "Select Remote Directory"
    modal: true
    anchors.centerIn: parent
    width: 400
    height: 500
    padding: 0

    Material.background: CoderTheme.surface
    Material.foreground: CoderTheme.textPrimary

    property string agentHostname: ""
    property string selectedPath: ""
    property bool showDotfiles: false
    property bool loading: false
    property string errorMessage: ""

    signal directorySelected(string path)

    // Internal directory tree model stored as a JS array.
    // Using a flat JS array with depth tracking rather than a nested ListModel
    // for simplicity. The tradeoff is that reassigning the array triggers a
    // full re-render, but the tree is typically small (tens of entries).
    // Each entry: { path, name, depth, expanded, loaded }
    property var dirModel: []

    // Pending request tracking for signal-based async
    property var pendingPathSegments: ""
    property string pendingRelativity: ""
    property int pendingDepth: 0
    property int pendingParentIndex: -1

    onOpened: {
        dirPicker.selectedPath = "";
        dirPicker.errorMessage = "";
        dirPicker.dirModel = [];
        loadDirectory("", "home", 0, -1);
    }

    // Signal-based API — no callback. Results arrive via Connections below.
    function loadDirectory(pathSegments, relativity, depth, parentIndex) {
        dirPicker.loading = true;
        dirPicker.errorMessage = "";
        dirPicker.pendingPathSegments = pathSegments;
        dirPicker.pendingRelativity = relativity;
        dirPicker.pendingDepth = depth;
        dirPicker.pendingParentIndex = parentIndex;
        agentApiClient.listDirectory(dirPicker.agentHostname, pathSegments, relativity);
    }

    Connections {
        target: agentApiClient

        function onDirectoryListed(hostname, listing) {
            if (hostname !== dirPicker.agentHostname) return;
            dirPicker.loading = false;

            var depth = dirPicker.pendingDepth;
            var parentIndex = dirPicker.pendingParentIndex;
            var entries = listing.contents || [];

            var newEntries = [];
            for (var i = 0; i < entries.length; i++) {
                var entry = entries[i];
                // Skip dotfiles unless enabled
                if (!dirPicker.showDotfiles && entry.name.startsWith(".")) continue;
                // Only show directories
                if (!entry.isDir) continue;

                newEntries.push({
                    path: entry.absolutePathString,
                    name: entry.name,
                    depth: depth,
                    expanded: false,
                    loaded: false
                });
            }

            // Sort alphabetically
            newEntries.sort(function (a, b) {
                return a.name.localeCompare(b.name);
            });

            if (parentIndex < 0) {
                // Root level
                dirPicker.dirModel = newEntries;
            } else {
                // Insert after parent, removing old children if re-expanding
                var model = dirPicker.dirModel.slice();
                var insertPos = parentIndex + 1;

                // Remove existing children
                while (insertPos < model.length && model[insertPos].depth > model[parentIndex].depth) {
                    model.splice(insertPos, 1);
                }

                // Insert new children
                for (var j = 0; j < newEntries.length; j++) {
                    model.splice(insertPos + j, 0, newEntries[j]);
                }

                // Mark parent as expanded and loaded
                model[parentIndex].expanded = true;
                model[parentIndex].loaded = true;

                dirPicker.dirModel = model;
            }
        }

        function onListDirectoryError(hostname, errorMessage) {
            if (hostname !== dirPicker.agentHostname) return;
            dirPicker.loading = false;
            dirPicker.errorMessage = errorMessage || "Failed to list directory.";
        }
    }

    function toggleExpand(itemIndex) {
        var model = dirPicker.dirModel.slice();
        var item = model[itemIndex];

        if (item.expanded) {
            // Collapse: remove all deeper items below this one
            item.expanded = false;
            var removePos = itemIndex + 1;
            while (removePos < model.length && model[removePos].depth > item.depth) {
                model.splice(removePos, 1);
            }
            model[itemIndex] = item;
            dirPicker.dirModel = model;
        } else if (item.loaded) {
            // Re-expand: reload from API
            loadDirectory(item.path, "", item.depth + 1, itemIndex);
        } else {
            // First expansion: load from API
            loadDirectory(item.path, "", item.depth + 1, itemIndex);
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ---- Toolbar ----
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            spacing: 8

            Label {
                text: "📁"
                font.pixelSize: 16
            }

            Label {
                text: dirPicker.agentHostname
                font.pixelSize: 13
                font.weight: Font.Medium
                color: CoderTheme.textPrimary
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            CheckBox {
                id: dotfilesToggle
                text: "Show hidden"
                font.pixelSize: 11
                checked: dirPicker.showDotfiles
                onCheckedChanged: {
                    dirPicker.showDotfiles = checked;
                    // Reload from root
                    dirPicker.dirModel = [];
                    loadDirectory("", "home", 0, -1);
                }

                Material.foreground: CoderTheme.textSecondary
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: CoderTheme.divider
        }

        // ---- Error banner ----
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: pickerErrorLabel.implicitHeight + 16
            radius: CoderTheme.radiusSm
            color: CoderTheme.errorSurface
            border.color: CoderTheme.error
            border.width: 1
            visible: dirPicker.errorMessage.length > 0

            Label {
                id: pickerErrorLabel
                anchors.fill: parent
                anchors.margins: 8
                text: dirPicker.errorMessage
                color: CoderTheme.error
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }

        // ---- Loading indicator ----
        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: dirPicker.loading && dirPicker.dirModel.length === 0
            visible: dirPicker.loading && dirPicker.dirModel.length === 0
        }

        // ---- Directory tree list ----
        ListView {
            id: dirListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: dirPicker.dirModel

            delegate: Rectangle {
                id: dirDelegate
                width: dirListView.width
                height: 32
                color: {
                    if (dirPicker.selectedPath === modelData.path) return CoderTheme.activeSurface;
                    if (dirDelegateMouseArea.containsMouse) return CoderTheme.hoverBg;
                    return "transparent";
                }

                Behavior on color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                MouseArea {
                    id: dirDelegateMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        dirPicker.selectedPath = modelData.path;
                    }

                    onDoubleClicked: {
                        dirPicker.toggleExpand(index);
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8 + modelData.depth * 20
                    anchors.rightMargin: 8
                    spacing: 4

                    // Expand/collapse arrow
                    Label {
                        text: modelData.expanded ? "▼" : "▶"
                        font.pixelSize: 10
                        color: CoderTheme.textSecondary
                        Layout.preferredWidth: 16
                        horizontalAlignment: Text.AlignHCenter

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dirPicker.toggleExpand(index)
                        }
                    }

                    // Folder icon
                    Label {
                        text: "📁"
                        font.pixelSize: 14
                    }

                    // Directory name
                    Label {
                        text: modelData.name
                        font.pixelSize: 13
                        color: CoderTheme.textPrimary
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    // Loading indicator for this specific directory
                    BusyIndicator {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        running: dirPicker.loading && modelData.expanded && !modelData.loaded
                        visible: running
                    }
                }
            }

            // ---- Empty state ----
            Label {
                anchors.centerIn: parent
                text: "No directories found"
                font.pixelSize: 13
                color: CoderTheme.textDisabled
                visible: dirPicker.dirModel.length === 0 && !dirPicker.loading
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: CoderTheme.divider
        }

        // ---- Selected path display ----
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            spacing: 8

            Label {
                text: "Path:"
                font.pixelSize: 12
                font.weight: Font.Medium
                color: CoderTheme.textSecondary
            }

            Label {
                text: dirPicker.selectedPath.length > 0 ? dirPicker.selectedPath : "No directory selected"
                font.pixelSize: 12
                color: dirPicker.selectedPath.length > 0 ? CoderTheme.textPrimary : CoderTheme.textDisabled
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
        }
    }

    footer: DialogButtonBox {
        padding: 12

        CoderButton {
            text: "Cancel"
            variant: "outline"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        CoderButton {
            text: "Select"
            variant: "default"
            enabled: dirPicker.selectedPath.length > 0
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: {
        dirPicker.directorySelected(dirPicker.selectedPath);
    }
}
