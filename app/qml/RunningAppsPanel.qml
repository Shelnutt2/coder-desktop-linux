import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import CoderDesktop

Rectangle {
    id: panel

    property var model: null
    signal stopRequested(int pid)
    signal stopAllRequested()

    color: CoderTheme.surface
    border.color: CoderTheme.border
    border.width: 1
    radius: CoderTheme.radius
    implicitHeight: panelCol.implicitHeight + 24

    ColumnLayout {
        id: panelCol
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "RUNNING APPS"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: CoderTheme.textSecondary
            }

            Item { Layout.fillWidth: true }

            Label {
                text: panel.model ? panel.model.count + " running" : "0 running"
                font.pixelSize: 12
                color: CoderTheme.textSecondary
            }

            CoderButton {
                text: "Stop All"
                variant: "destructive"
                visible: panel.model ? panel.model.count > 1 : false
                onClicked: panel.stopAllRequested()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: CoderTheme.divider
        }

        Repeater {
            model: panel.model

            delegate: Rectangle {
                Layout.fillWidth: true
                height: appRow.implicitHeight + 16
                radius: CoderTheme.radiusSm
                color: delegateMouse.containsMouse ? CoderTheme.hoverBg : "transparent"

                MouseArea {
                    id: delegateMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }

                RowLayout {
                    id: appRow
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 12

                    ColumnLayout {
                        spacing: 2
                        Layout.fillWidth: true

                        Label {
                            text: model.appName || model.command
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            color: CoderTheme.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: "PID " + model.pid + " — " + model.command
                            font.pixelSize: 11
                            color: CoderTheme.textSecondary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    CoderButton {
                        text: "Stop"
                        variant: "destructive"
                        onClicked: panel.stopRequested(model.pid)
                    }
                }
            }
        }

        Label {
            text: "No running apps"
            color: CoderTheme.textDisabled
            font.pixelSize: 13
            visible: panel.model ? panel.model.count === 0 : true
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
