import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

// Compact source-citation row: favicon-style bullet, title, and host.
// Clicking opens the source URL externally.
Rectangle {
    id: row
    property string title: ""
    property string url: ""

    implicitHeight: content.implicitHeight + 8
    radius: CoderTheme.radiusSm
    color: ma.containsMouse ? CoderTheme.hoverBg : "transparent"
    border.color: CoderTheme.border
    border.width: 1

    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 6

        Label { text: "\u2197"; color: CoderTheme.primary; font.pixelSize: 11 }
        Label {
            text: row.title.length > 0 ? row.title : row.url
            color: CoderTheme.textPrimary
            font.pixelSize: 11
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label {
            text: {
                try { return new URL(row.url).hostname } catch (e) { return "" }
            }
            color: CoderTheme.textDisabled
            font.pixelSize: 10
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            // Source URLs are untrusted agent output; only open http,
            // https, and mailto links externally.
            if (/^(https?|mailto):/i.test(row.url)) Qt.openUrlExternally(row.url)
        }
    }
}
