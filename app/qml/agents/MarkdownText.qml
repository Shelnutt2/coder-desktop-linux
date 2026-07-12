import QtQuick
import CoderDesktop

// Markdown-rendering text block for assistant content. Links open in the
// external browser; text is selectable with the mouse.
TextEdit {
    id: control
    property alias markdown: control.text

    // Allowlist of link schemes that may be handed to the OS. Markdown from
    // the agent is untrusted; anything outside http/https/mailto is ignored.
    function isSafeLink(link) {
        return /^(https?|mailto):/i.test(link)
    }

    textFormat: TextEdit.MarkdownText
    wrapMode: Text.Wrap
    readOnly: true
    selectByMouse: true
    color: CoderTheme.textPrimary
    selectionColor: CoderTheme.primary
    selectedTextColor: CoderTheme.textInvert
    font.pixelSize: 13

    onLinkActivated: function(link) {
        if (control.isSafeLink(link)) Qt.openUrlExternally(link)
    }

    // Show a pointing hand over links without stealing selection drags.
    HoverHandler {
        enabled: control.hoveredLink.length > 0
        cursorShape: Qt.PointingHandCursor
    }
}
