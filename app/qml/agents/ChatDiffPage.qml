import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

// Diff viewer for a chat: per-file collapsible sections with monospace
// lines colored by +/-/context, add/del counts per file, and a PR link
// button when the server reports a pull_request_url.
Rectangle {
    id: page
    // The page's ChatController context object.
    property var chat: null
    signal backClicked()

    color: CoderTheme.background

    Component.onCompleted: if (chat) chat.fetchDiff()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ToolButton {
                text: "\u2190"
                font.pixelSize: 16
                onClicked: page.backClicked()
            }
            Label {
                text: "Changes"
                font.pixelSize: 16
                font.bold: true
                color: CoderTheme.textPrimary
                Layout.fillWidth: true
            }
            CoderButton {
                visible: page.chat && page.chat.pullRequestUrl.length > 0
                text: "Open PR"
                variant: "outline"
                onClicked: Qt.openUrlExternally(page.chat.pullRequestUrl)
            }
            CoderButton {
                text: "Refresh"
                variant: "outline"
                enabled: page.chat && !page.chat.diffLoading
                onClicked: page.chat.fetchDiff()
            }
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: page.chat && page.chat.diffLoading
            visible: running
        }

        Label {
            visible: page.chat && !page.chat.diffLoading && page.chat.diffFiles.length === 0
            text: "No changes yet"
            color: CoderTheme.textDisabled
            font.pixelSize: 13
            Layout.alignment: Qt.AlignHCenter
        }

        // Per-file collapsible sections.
        ListView {
            id: fileList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: page.visible && page.chat ? page.chat.diffFiles : []

            delegate: Rectangle {
                id: fileCard
                required property var modelData
                property bool expanded: true

                width: fileList.width
                height: fileHeader.implicitHeight + 16
                       + (expanded ? linesCol.implicitHeight + 8 : 0)
                radius: CoderTheme.radiusSm
                color: CoderTheme.surface
                border.color: CoderTheme.border
                border.width: 1
                clip: true

                RowLayout {
                    id: fileHeader
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 8
                    spacing: 6

                    Label {
                        text: fileCard.expanded ? "\u25be" : "\u25b8"
                        color: CoderTheme.textSecondary
                        font.pixelSize: 12
                    }
                    Label {
                        text: fileCard.modelData.oldPath.length > 0
                            ? fileCard.modelData.oldPath + " \u2192 " + fileCard.modelData.path
                            : fileCard.modelData.path
                        color: CoderTheme.textPrimary
                        font.family: "monospace"
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                    Label {
                        text: "+" + fileCard.modelData.addCount
                        color: CoderTheme.success
                        font.pixelSize: 11
                        font.family: "monospace"
                    }
                    Label {
                        text: "-" + fileCard.modelData.delCount
                        color: CoderTheme.error
                        font.pixelSize: 11
                        font.family: "monospace"
                    }
                }

                MouseArea {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: fileHeader.implicitHeight + 16
                    onClicked: fileCard.expanded = !fileCard.expanded
                }

                Column {
                    id: linesCol
                    visible: fileCard.expanded
                    anchors.top: fileHeader.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 8
                    anchors.topMargin: 6
                    spacing: 0

                    Repeater {
                        model: fileCard.expanded ? fileCard.modelData.hunks : []
                        Rectangle {
                            required property var modelData
                            width: linesCol.width
                            height: lineText.implicitHeight
                            color: {
                                if (modelData.kind === "add")
                                    return Qt.alpha(CoderTheme.success, 0.12)
                                if (modelData.kind === "del")
                                    return Qt.alpha(CoderTheme.error, 0.12)
                                return "transparent"
                            }
                            Text {
                                id: lineText
                                width: parent.width
                                text: modelData.text.length > 0 ? modelData.text : " "
                                font.family: "monospace"
                                font.pixelSize: 10
                                wrapMode: Text.WrapAnywhere
                                color: {
                                    if (modelData.kind === "add") return CoderTheme.success
                                    if (modelData.kind === "del") return CoderTheme.error
                                    if (modelData.kind === "meta" || modelData.kind === "hunk")
                                        return CoderTheme.textDisabled
                                    return CoderTheme.textSecondary
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
