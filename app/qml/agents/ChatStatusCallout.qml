import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

// Status callouts between the timeline and the composer: stream errors
// (kind-aware copy, Retry when retryable), server retry countdown,
// requires-action notice, and the usage-limit banner from HTTP 409.
ColumnLayout {
    id: callout
    // The page's ChatController context object.
    property var chat: null
    spacing: 6

    function errorHeadline(kind) {
        if (kind === "overloaded") return "The model provider is overloaded."
        if (kind === "rate_limit") return "Rate limited by the model provider."
        if (kind === "timeout") return "The model request timed out."
        if (kind === "auth") return "Model provider authentication failed."
        if (kind === "config") return "The model configuration is invalid."
        if (kind === "usage_limit") return "Usage limit reached."
        return "The agent hit an error."
    }

    function formatDollarsFromMicros(micros) {
        return "$" + (micros / 1000000).toFixed(2)
    }

    // ---- Error callout ----
    Rectangle {
        visible: callout.chat && callout.chat.hasError && !callout.chat.hasRetry
        Layout.fillWidth: true
        implicitHeight: errorCol.implicitHeight + 16
        radius: CoderTheme.radiusSm
        color: CoderTheme.errorSurface
        border.color: CoderTheme.error
        border.width: 1

        RowLayout {
            id: errorCol
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                Label {
                    text: callout.chat ? callout.errorHeadline(callout.chat.errorKind) : ""
                    color: CoderTheme.error
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Label {
                    visible: callout.chat && callout.chat.errorMessage.length > 0
                    text: callout.chat ? callout.chat.errorMessage : ""
                    color: CoderTheme.error
                    opacity: 0.85
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
            CoderButton {
                visible: callout.chat && callout.chat.errorRetryable
                text: "Retry"
                variant: "outline"
                // Reconcile: refetch the newest history page and restart the
                // stream so the retried run streams in cleanly.
                onClicked: {
                    callout.chat.refreshMessages()
                    callout.chat.reconnect()
                }
            }
        }
    }

    // ---- Server retry countdown ----
    Rectangle {
        visible: callout.chat && callout.chat.hasRetry
        Layout.fillWidth: true
        implicitHeight: retryRow.implicitHeight + 12
        radius: CoderTheme.radiusSm
        color: CoderTheme.warningSurface
        border.color: CoderTheme.warning
        border.width: 1

        RowLayout {
            id: retryRow
            anchors.fill: parent
            anchors.margins: 6
            spacing: 8

            BusyIndicator { implicitWidth: 16; implicitHeight: 16; running: parent.visible }
            Label {
                text: callout.chat
                    ? "Retrying (attempt " + callout.chat.retryAttempt + ") in "
                      + Math.ceil(callout.chat.retryDelayMs / 1000) + "s\u2026"
                    : ""
                color: CoderTheme.warning
                font.pixelSize: 12
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
        }
    }

    // ---- Requires-action banner ----
    Rectangle {
        visible: callout.chat && callout.chat.hasActionRequired
        Layout.fillWidth: true
        implicitHeight: actionRow.implicitHeight + 12
        radius: CoderTheme.radiusSm
        color: CoderTheme.warningSurface
        border.color: CoderTheme.warning
        border.width: 1

        RowLayout {
            id: actionRow
            anchors.fill: parent
            anchors.margins: 6
            spacing: 8
            Label { text: "!"; color: CoderTheme.warning; font.bold: true; font.pixelSize: 13 }
            Label {
                text: "The agent is waiting for your input below."
                color: CoderTheme.warning
                font.pixelSize: 12
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }
        }
    }

    // ---- Usage-limit banner (HTTP 409) ----
    Rectangle {
        visible: callout.chat && callout.chat.hasUsageLimit
        Layout.fillWidth: true
        implicitHeight: usageRow.implicitHeight + 12
        radius: CoderTheme.radiusSm
        color: CoderTheme.warningSurface
        border.color: CoderTheme.warning
        border.width: 1

        RowLayout {
            id: usageRow
            anchors.fill: parent
            anchors.margins: 6
            spacing: 8
            Label {
                text: callout.chat
                    ? "Usage limit reached: "
                      + callout.formatDollarsFromMicros(callout.chat.usageSpentMicros)
                      + " of "
                      + callout.formatDollarsFromMicros(callout.chat.usageLimitMicros)
                      + " spent. Resets "
                      + Qt.formatDateTime(callout.chat.usageResetsAt, "MMM d, hh:mm") + "."
                    : ""
                color: CoderTheme.warning
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label {
                text: "\u00d7"
                color: CoderTheme.warning
                font.pixelSize: 14
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    onClicked: callout.chat.clearUsageLimit()
                }
            }
        }
    }
}
