import QtQuick
import QtQuick.Layouts
import CoderDesktop

Rectangle {
    id: chip
    property string status: "Stopped"

    implicitWidth: chipRow.implicitWidth + 16
    implicitHeight: chipRow.implicitHeight + 8
    radius: height / 2
    color: CoderTheme.statusSurface(status)
    border.color: CoderTheme.statusColor(status)
    border.width: 1

    RowLayout {
        id: chipRow
        anchors.centerIn: parent
        spacing: 6

        Rectangle {
            width: 6; height: 6; radius: 3
            color: CoderTheme.statusColor(chip.status)
        }
        Text {
            text: chip.status
            color: CoderTheme.statusColor(chip.status)
            font.pixelSize: 12
            font.weight: Font.Medium
        }
    }
}
