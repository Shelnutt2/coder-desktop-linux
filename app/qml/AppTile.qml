import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

Rectangle {
    id: tile

    property string appName: ""
    property string appExec: ""
    property string appId: ""
    property string appCategory: ""
    property string appIconName: ""
    property string appIconPath: ""

    signal launched()

    width: 120
    height: 130
    radius: CoderTheme.radius
    color: mouseArea.containsMouse ? CoderTheme.hoverBg : CoderTheme.surface
    border.color: mouseArea.containsMouse ? CoderTheme.borderHover : CoderTheme.border
    border.width: 1

    Behavior on color { ColorAnimation { duration: 120 } }
    Behavior on border.color { ColorAnimation { duration: 120 } }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: tile.launched()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48

            Image {
                id: iconImage
                anchors.centerIn: parent
                width: 48
                height: 48
                sourceSize: Qt.size(48, 48)
                source: {
                    if (tile.appIconPath !== "")
                        return "file://" + tile.appIconPath;
                    if (tile.appIconName !== "")
                        return "image://icon-theme/" + tile.appIconName;
                    return "";
                }
                visible: status === Image.Ready
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Label {
                id: fallbackEmoji
                anchors.centerIn: parent
                font.pixelSize: 32
                visible: !iconImage.visible
                text: {
                    switch (tile.appCategory) {
                        case "IDE":      return "🖥️";
                        case "Browser":  return "🌐";
                        case "Editor":   return "📝";
                        case "AI Tool":  return "🤖";
                        case "Terminal": return "⌨️";
                        default:         return "📦";
                    }
                }
            }
        }

        Label {
            text: tile.appName
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            maximumLineCount: 2
            wrapMode: Text.Wrap
            font.pixelSize: 12
            font.weight: Font.Medium
            color: CoderTheme.textPrimary
        }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: categoryLabel.implicitWidth + 12
            Layout.preferredHeight: categoryLabel.implicitHeight + 4
            radius: CoderTheme.radiusSm
            color: CoderTheme.surfaceSecondary
            visible: tile.appCategory !== ""

            Label {
                id: categoryLabel
                anchors.centerIn: parent
                text: tile.appCategory
                font.pixelSize: 10
                color: CoderTheme.textSecondary
            }
        }

        Item { Layout.fillHeight: true }
    }
}
