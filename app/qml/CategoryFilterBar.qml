import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

RowLayout {
    id: filterBar

    property string selectedCategory: ""
    signal categorySelected(string category)

    spacing: 6

    Repeater {
        model: [
            { label: "All",      value: "" },
            { label: "IDE",      value: "IDE" },
            { label: "Browser",  value: "Browser" },
            { label: "Editor",   value: "Editor" },
            { label: "AI Tool",  value: "AI Tool" },
            { label: "Terminal", value: "Terminal" }
        ]

        delegate: Rectangle {
            required property var modelData
            required property int index

            property bool isActive: filterBar.selectedCategory === modelData.value

            width: chipLabel.implicitWidth + 20
            height: 28
            radius: 14
            color: isActive ? CoderTheme.primary : chipMouse.containsMouse ? CoderTheme.hoverBg : CoderTheme.surfaceSecondary
            border.color: isActive ? CoderTheme.primary : CoderTheme.border
            border.width: isActive ? 0 : 1

            Behavior on color { ColorAnimation { duration: 120 } }

            Label {
                id: chipLabel
                anchors.centerIn: parent
                text: modelData.label
                font.pixelSize: 12
                font.weight: isActive ? Font.DemiBold : Font.Normal
                color: isActive ? CoderTheme.textInvert : CoderTheme.textPrimary
            }

            MouseArea {
                id: chipMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    filterBar.selectedCategory = modelData.value;
                    filterBar.categorySelected(modelData.value);
                }
            }
        }
    }
}
