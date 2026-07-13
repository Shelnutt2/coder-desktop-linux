import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

// Compact chip for skill parts: skill name in a subtle pill.
Rectangle {
    id: chip
    property string name: ""

    implicitWidth: row.implicitWidth + 16
    implicitHeight: row.implicitHeight + 8
    radius: height / 2
    color: CoderTheme.activeSurface
    border.color: CoderTheme.info
    border.width: 1

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 4
        Label { text: "\u2726"; color: CoderTheme.info; font.pixelSize: 10 }
        Label {
            text: chip.name
            color: CoderTheme.info
            font.pixelSize: 11
            elide: Text.ElideRight
            Layout.maximumWidth: 160
        }
    }
}
