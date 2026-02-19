import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import CoderDesktop

// File browser page — lists remote workspace files via Agent API and allows
// downloading/uploading individual files through FileTransferManager.
Item {
    id: fileBrowserPage
    focus: true

    property string agentHostname: ""
    property string workspaceName: ""

    signal backClicked()

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Backspace) {
            if (cachedSegments.length > 1)
                navigateTo(absolutePathUpTo(cachedSegments, cachedSegments.length - 2))
            else if (cachedSegments.length === 1)
                navigateTo("/")
            event.accepted = true;
        }
    }

    // -- Internal state ------------------------------------------------------
    property string currentPath: ""          // absolute path string from API
    property var    currentContents: []       // array of {name, absolutePathString, isDir}
    property bool   isLoading: false
    property string errorMessage: ""
    property int    selectedIndex: -1
    property var    cachedSegments: pathSegments(currentPath)
    onCurrentPathChanged: cachedSegments = pathSegments(currentPath)

    // -- Helpers -------------------------------------------------------------
    function formatFileSize(bytes) {
        if (bytes === undefined || bytes === null || bytes < 0) return ""
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MB"
        return (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB"
    }

    function pathSegments(pathString) {
        if (!pathString) return []
        var parts = pathString.split("/").filter(function(s) { return s.length > 0 })
        return parts
    }

    function absolutePathUpTo(segments, index) {
        return "/" + segments.slice(0, index + 1).join("/")
    }

    function sortedContents(contents) {
        if (!contents) return []
        var arr = []
        for (var i = 0; i < contents.length; i++) arr.push(contents[i])
        arr.sort(function(a, b) {
            // Directories first
            if (a.isDir && !b.isDir) return -1
            if (!a.isDir && b.isDir) return 1
            return a.name.localeCompare(b.name)
        })
        return arr
    }

    function navigateTo(pathString) {
        selectedIndex = -1
        isLoading = true
        errorMessage = ""
        // Build path segments array from absolute path
        var segments = pathSegments(pathString)
        agentApiClient.listDirectory(agentHostname, segments, "absolute")
    }

    function navigateToRoot() {
        navigateTo("/")
    }

    // -- Initial load --------------------------------------------------------
    // Navigate when agentHostname is set (not on Component.onCompleted, because
    // the Loader's onLoaded binding may not have fired yet at that point).
    onAgentHostnameChanged: {
        if (agentHostname.length > 0) navigateToRoot()
    }

    // -- API signal handler --------------------------------------------------
    Connections {
        target: agentApiClient

        function onDirectoryListed(hostname, listing) {
            if (hostname !== agentHostname) return
            isLoading = false
            errorMessage = ""
            currentPath = listing.absolutePathString || ""
            currentContents = sortedContents(listing.contents || [])
        }

        function onListDirectoryError(hostname, error) {
            if (hostname !== agentHostname) return
            isLoading = false
            errorMessage = error || "Failed to list directory"
        }
    }

    // -- File dialogs --------------------------------------------------------
    FileDialog {
        id: uploadFileDialog
        title: "Select file to upload"
        onAccepted: {
            var dest = currentPath
            if (dest && !dest.endsWith("/")) dest += "/"
            // If a directory is selected, upload into it
            if (selectedIndex >= 0 && currentContents[selectedIndex] && currentContents[selectedIndex].isDir) {
                dest = currentContents[selectedIndex].absolutePathString
                if (!dest.endsWith("/")) dest += "/"
            }
            var filePath = selectedFile.toString();
            if (filePath.startsWith("file:///")) filePath = filePath.substring(7);
            else if (filePath.startsWith("file://")) filePath = filePath.substring(6);
            fileTransferManager.upload(agentHostname, filePath, dest + filePath.split("/").pop())
        }
    }

    FolderDialog {
        id: downloadFolderDialog
        title: "Save file to..."
        property string remoteFilePath: ""
        property string remoteFileName: ""
        onAccepted: {
            var localDir = selectedFolder.toString();
            if (localDir.startsWith("file:///")) localDir = localDir.substring(7);
            else if (localDir.startsWith("file://")) localDir = localDir.substring(6);
            if (!localDir.endsWith("/")) localDir += "/"
            fileTransferManager.download(agentHostname, remoteFilePath, localDir + remoteFileName)
        }
    }

    // -- Layout --------------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // == Header bar =====================================================
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: headerRow.implicitHeight + 24
            color: CoderTheme.surface
            border.color: CoderTheme.border
            border.width: 1
            radius: CoderTheme.radius

            RowLayout {
                id: headerRow
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                // Back button
                Label {
                    text: "←"
                    font.pixelSize: 18
                    color: backMouse.containsMouse ? CoderTheme.primary : CoderTheme.textPrimary

                    MouseArea {
                        id: backMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: fileBrowserPage.backClicked()
                    }
                }

                Label {
                    text: workspaceName + " / Files"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    color: CoderTheme.textPrimary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                CoderButton {
                    text: "Upload"
                    variant: "default"
                    visible: !settingsManager.disableFileUpload
                    onClicked: uploadFileDialog.open()
                }
            }
        }

        // == Breadcrumb bar =================================================
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: breadcrumbRow.implicitHeight + 16
            color: CoderTheme.background

            Flow {
                id: breadcrumbRow
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                // Up directory button
                Label {
                    text: "⬆"
                    font.pixelSize: 14
                    color: upDirMouse.containsMouse ? CoderTheme.primaryLight : CoderTheme.textSecondary
                    visible: cachedSegments.length > 0

                    MouseArea {
                        id: upDirMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (cachedSegments.length > 1)
                                navigateTo(absolutePathUpTo(cachedSegments, cachedSegments.length - 2))
                            else
                                navigateTo("/")
                        }
                    }
                }

                // Root /
                Label {
                    text: "/"
                    font.pixelSize: 13
                    color: breadcrumbRootMouse.containsMouse ? CoderTheme.primaryLight : CoderTheme.primary

                    MouseArea {
                        id: breadcrumbRootMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: navigateTo("/")
                    }
                }

                Repeater {
                    model: cachedSegments

                    delegate: Row {
                        spacing: 4
                        property bool isLast: index === cachedSegments.length - 1

                        Label {
                            text: modelData
                            font.pixelSize: 13
                            font.weight: isLast ? Font.DemiBold : Font.Normal
                            color: {
                                if (isLast) return CoderTheme.textPrimary
                                return segmentMouse.containsMouse ? CoderTheme.primaryLight : CoderTheme.primary
                            }

                            MouseArea {
                                id: segmentMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: isLast ? Qt.ArrowCursor : Qt.PointingHandCursor
                                onClicked: {
                                    if (!isLast) {
                                        navigateTo(absolutePathUpTo(cachedSegments, index))
                                    }
                                }
                            }
                        }

                        Label {
                            text: "/"
                            font.pixelSize: 13
                            color: CoderTheme.textSecondary
                            visible: !isLast
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: CoderTheme.divider
        }

        // == File list ======================================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Loading spinner
            BusyIndicator {
                anchors.centerIn: parent
                running: isLoading
                visible: isLoading
            }

            // Error state
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                visible: !isLoading && errorMessage !== ""

                Label {
                    text: errorMessage
                    font.pixelSize: 14
                    color: CoderTheme.error
                    horizontalAlignment: Text.AlignHCenter
                    Layout.alignment: Qt.AlignHCenter
                }

                CoderButton {
                    text: "Retry"
                    variant: "outline"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: navigateTo(currentPath || "/")
                }
            }

            // Empty state
            Label {
                anchors.centerIn: parent
                text: "This directory is empty"
                font.pixelSize: 14
                color: CoderTheme.textDisabled
                visible: !isLoading && errorMessage === "" && currentContents.length === 0
            }

            // File ListView
            ListView {
                id: fileListView
                anchors.fill: parent
                visible: !isLoading && errorMessage === "" && currentContents.length > 0
                clip: true
                model: currentContents
                currentIndex: selectedIndex
                boundsBehavior: Flickable.StopAtBounds

                delegate: Rectangle {
                    id: fileDelegate
                    width: fileListView.width
                    height: fileRow.implicitHeight + 16
                    color: {
                        if (index === selectedIndex) return CoderTheme.activeSurface
                        if (fileDelegateMouse.containsMouse) return CoderTheme.hoverBg
                        return "transparent"
                    }

                    Behavior on color { ColorAnimation { duration: 120 } }

                    property var entry: currentContents[index]

                    ToolTip.visible: fileDelegateMouse.containsMouse && entry && entry.absolutePathString
                    ToolTip.text: entry ? entry.absolutePathString : ""
                    ToolTip.delay: 800

                    MouseArea {
                        id: fileDelegateMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.LeftButton | Qt.RightButton

                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton) {
                                selectedIndex = index
                                contextMenu.popup()
                            } else {
                                selectedIndex = index
                            }
                        }

                        onDoubleClicked: {
                            if (entry.isDir) {
                                navigateTo(entry.absolutePathString)
                            } else if (!settingsManager.disableFileDownload) {
                                downloadFolderDialog.remoteFilePath = entry.absolutePathString
                                downloadFolderDialog.remoteFileName = entry.name
                                downloadFolderDialog.open()
                            }
                        }
                    }

                    RowLayout {
                        id: fileRow
                        anchors.fill: parent
                        anchors.margins: 8
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 12

                        // Icon
                        Label {
                            text: entry.isDir ? "📁" : "📄"
                            font.pixelSize: 16
                        }

                        // Name
                        Label {
                            text: entry.name
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            color: CoderTheme.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        // Size (files only)
                        Label {
                            text: !entry.isDir && entry.size !== undefined ? formatFileSize(entry.size) : ""
                            font.pixelSize: 12
                            color: CoderTheme.textSecondary
                            visible: !entry.isDir
                        }

                        // Modified time
                        Label {
                            text: entry.lastModified || ""
                            font.pixelSize: 12
                            color: CoderTheme.textSecondary
                        }
                    }

                    // Bottom border
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        height: 1
                        color: CoderTheme.divider
                    }
                }

                // Context menu
                Menu {
                    id: contextMenu
                    Material.background: CoderTheme.surface

                    MenuItem {
                        text: "Download"
                        visible: selectedIndex >= 0 && currentContents[selectedIndex] && !currentContents[selectedIndex].isDir && !settingsManager.disableFileDownload
                        height: visible ? implicitHeight : 0
                        onTriggered: {
                            var entry = currentContents[selectedIndex]
                            downloadFolderDialog.remoteFilePath = entry.absolutePathString
                            downloadFolderDialog.remoteFileName = entry.name
                            downloadFolderDialog.open()
                        }
                    }

                    MenuItem {
                        text: "Upload here"
                        visible: selectedIndex >= 0 && currentContents[selectedIndex] && currentContents[selectedIndex].isDir && !settingsManager.disableFileUpload
                        height: visible ? implicitHeight : 0
                        onTriggered: uploadFileDialog.open()
                    }

                    MenuItem {
                        text: "Open folder"
                        visible: selectedIndex >= 0 && currentContents[selectedIndex] && currentContents[selectedIndex].isDir
                        height: visible ? implicitHeight : 0
                        onTriggered: navigateTo(currentContents[selectedIndex].absolutePathString)
                    }
                }
            }
        }

        // == Action bar (shown when file selected) ==========================
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: actionRow.implicitHeight + 16
            color: CoderTheme.surface
            border.color: CoderTheme.border
            border.width: 1
            radius: CoderTheme.radiusSm
            visible: selectedIndex >= 0 && currentContents[selectedIndex] && !currentContents[selectedIndex].isDir

            RowLayout {
                id: actionRow
                anchors.fill: parent
                anchors.margins: 8
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                Label {
                    text: selectedIndex >= 0 && currentContents[selectedIndex] ? currentContents[selectedIndex].name : ""
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: CoderTheme.textPrimary
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Label {
                    text: selectedIndex >= 0 && currentContents[selectedIndex] && currentContents[selectedIndex].size !== undefined
                        ? formatFileSize(currentContents[selectedIndex].size) : ""
                    font.pixelSize: 12
                    color: CoderTheme.textSecondary
                }

                CoderButton {
                    text: "Download"
                    variant: "outline"
                    visible: !settingsManager.disableFileDownload
                    onClicked: {
                        var entry = currentContents[selectedIndex]
                        downloadFolderDialog.remoteFilePath = entry.absolutePathString
                        downloadFolderDialog.remoteFileName = entry.name
                        downloadFolderDialog.open()
                    }
                }
            }
        }

        // == Transfer panel =================================================
        TransferPanel {
            Layout.fillWidth: true
            Layout.margins: 0
            transferModel: fileTransferManager
            onCancelRequested: function(transferId) {
                fileTransferManager.cancelTransfer(transferId)
            }
        }
    }
}
