import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

// Compact chip for file / image / file-reference parts. Optionally shows
// upload progress and a remove button when used in the composer.
Rectangle {
    id: chip
    property string name: ""
    property bool uploading: false
    property bool removable: false
    property bool failed: false
    signal removeClicked()

    implicitWidth: row.implicitWidth + 16
    implicitHeight: row.implicitHeight + 8
    radius: height / 2
    color: failed ? CoderTheme.errorSurface : CoderTheme.surfaceSecondary
    border.color: failed ? CoderTheme.error : CoderTheme.border
    border.width: 1

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 4

        BusyIndicator {
            visible: chip.uploading
            running: chip.uploading
            implicitWidth: 12
            implicitHeight: 12
        }
        Label {
            text: "\u{1F4CE}"
            visible: !chip.uploading
            font.pixelSize: 10
        }
        Label {
            text: chip.name
            color: chip.failed ? CoderTheme.error : CoderTheme.textPrimary
            font.pixelSize: 11
            elide: Text.ElideMiddle
            Layout.maximumWidth: 140
        }
        Label {
            visible: chip.removable
            text: "\u00d7"
            color: CoderTheme.textSecondary
            font.pixelSize: 12
            MouseArea {
                anchors.fill: parent
                anchors.margins: -4
                cursorShape: Qt.PointingHandCursor
                onClicked: chip.removeClicked()
            }
        }
    }
}
