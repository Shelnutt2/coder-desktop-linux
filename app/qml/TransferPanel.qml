import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

// Collapsible panel showing file transfer progress. Sits at the bottom of
// FileBrowserPage and is driven by the fileTransferManager model.
Rectangle {
    id: panel

    property var transferModel: null
    property bool expanded: true

    signal cancelRequested(string transferId)
    signal clearCompletedRequested()

    visible: transferModel ? transferModel.count > 0 : false
    color: CoderTheme.surface
    border.color: CoderTheme.border
    border.width: 1
    radius: CoderTheme.radius
    implicitHeight: visible ? panelCol.implicitHeight + 24 : 0

    ColumnLayout {
        id: panelCol
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // -- Header with collapse toggle ------------------------------------
        MouseArea {
            Layout.fillWidth: true
            implicitHeight: headerRow.implicitHeight
            cursorShape: Qt.PointingHandCursor
            onClicked: panel.expanded = !panel.expanded

            RowLayout {
                id: headerRow
                anchors.fill: parent

                Label {
                    text: "▼"
                    rotation: panel.expanded ? 0 : -90
                    font.pixelSize: 11
                    color: CoderTheme.textSecondary

                    Behavior on rotation { NumberAnimation { duration: 150 } }
                }

                Label {
                    text: "Transfers" + (transferModel ? " (" + transferModel.count + ")" : "")
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: CoderTheme.textSecondary
                }

                Item { Layout.fillWidth: true }

                // Clear completed button
                Label {
                    text: "Clear"
                    font.pixelSize: 11
                    color: clearCompletedMouse.containsMouse ? CoderTheme.primary : CoderTheme.textSecondary
                    visible: transferModel ? transferModel.count > 0 : false

                    MouseArea {
                        id: clearCompletedMouse
                        anchors.fill: parent
                        anchors.margins: -4
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: panel.clearCompletedRequested()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: CoderTheme.divider
            visible: panel.expanded
        }

        // -- Transfer list --------------------------------------------------
        ListView {
            id: transferList
            Layout.fillWidth: true
            Layout.preferredHeight: contentHeight
            Layout.maximumHeight: 200
            visible: panel.expanded
            clip: true
            interactive: true
            model: panel.transferModel
            spacing: 4

            delegate: Rectangle {
                width: transferList.width
                height: transferRow.implicitHeight + 16
                radius: CoderTheme.radiusSm
                color: transferDelegateMouse.containsMouse ? CoderTheme.hoverBg : "transparent"

                Behavior on color { ColorAnimation { duration: 120 } }

                MouseArea {
                    id: transferDelegateMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }

                ColumnLayout {
                    id: transferRow
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        // Direction icon
                        Label {
                            text: model.isUpload ? "↑" : "↓"
                            font.pixelSize: 14
                            font.weight: Font.Bold
                            color: model.isUpload ? CoderTheme.warning : CoderTheme.info

                            ToolTip.visible: dirIconMouse.containsMouse
                            ToolTip.text: model.isUpload ? "Uploading" : "Downloading"
                            ToolTip.delay: 500

                            MouseArea {
                                id: dirIconMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }
                        }

                        // Filename (basename)
                        Label {
                            text: {
                                var p = model.isUpload ? model.localPath : model.remotePath
                                var idx = p.lastIndexOf("/")
                                return idx >= 0 ? p.substring(idx + 1) : p
                            }
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            color: CoderTheme.textPrimary
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }

                        // Percentage / status text
                        Label {
                            text: {
                                if (model.state === "completed") return "✓"
                                if (model.state === "failed") return "✗"
                                return Math.round(model.progress * 100) + "%"
                            }
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            color: {
                                if (model.state === "completed") return CoderTheme.success
                                if (model.state === "failed") return CoderTheme.error
                                return CoderTheme.textSecondary
                            }
                        }

                        // Cancel button for running transfers
                        Label {
                            text: "✕"
                            font.pixelSize: 14
                            color: cancelMouse.containsMouse ? CoderTheme.error : CoderTheme.textSecondary
                            visible: model.state === "running" || model.state === "pending"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            MouseArea {
                                id: cancelMouse
                                anchors.fill: parent
                                anchors.margins: -4  // extend hit target
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: panel.cancelRequested(model.transferId)
                            }
                        }
                    }

                    // Progress bar
                    Rectangle {
                        Layout.fillWidth: true
                        height: 4
                        radius: 2
                        color: CoderTheme.surfaceSecondary

                        Rectangle {
                            width: parent.width * Math.min(model.progress, 1.0)
                            height: parent.height
                            radius: 2
                            color: {
                                if (model.state === "completed") return CoderTheme.success
                                if (model.state === "failed") return CoderTheme.error
                                return CoderTheme.primary
                            }

                            Behavior on width { NumberAnimation { duration: 200 } }
                        }
                    }

                    // Error message (if failed)
                    Label {
                        text: model.errorMessage || ""
                        visible: model.state === "failed" && (model.errorMessage || "") !== ""
                        font.pixelSize: 11
                        color: CoderTheme.error
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // Empty state (shouldn't normally appear since panel hides itself)
        Label {
            text: "No transfers"
            color: CoderTheme.textDisabled
            font.pixelSize: 13
            visible: panel.expanded && (transferModel ? transferModel.count === 0 : true)
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
