import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

// Plan card rendered for the newest assistant plan message while the chat is
// in plan mode. Steps come from ChatController.parsePlanSteps (PlanStepParser
// semantics); "Implement" clears plan_mode and sends the canned implement
// instruction (matching Android), "Keep planning" focuses refinement.
Rectangle {
    id: card
    // The page's ChatController context object.
    property var chat: null
    property string planMarkdown: ""
    // Parsed steps are recomputed only when the plan markdown actually
    // changes (handler assignment, not a binding), so streaming deltas on
    // the same message never re-run the C++ parser.
    property var steps: []
    signal keepPlanningRequested()

    function reparse() {
        steps = (chat && planMarkdown.length > 0) ? chat.parsePlanSteps(planMarkdown) : []
    }
    onPlanMarkdownChanged: reparse()
    onChatChanged: reparse()
    Component.onCompleted: reparse()

    visible: steps.length > 0
    implicitHeight: col.implicitHeight + 24
    radius: CoderTheme.radius
    color: CoderTheme.activeSurface
    border.color: CoderTheme.info
    border.width: 1

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: 12
        spacing: 6

        RowLayout {
            spacing: 6
            Image {
                source: "qrc:/CoderDesktop/assets/icons/plan.svg"
                sourceSize.width: 14
                sourceSize.height: 14
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
            }
            Label {
                text: "Plan (" + card.steps.length + " steps)"
                color: CoderTheme.info
                font.pixelSize: 13
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
        }

        Repeater {
            model: card.steps
            RowLayout {
                required property var modelData
                spacing: 8
                Layout.fillWidth: true

                // Checked/unchecked indicator per parsed step state.
                Rectangle {
                    width: 14; height: 14; radius: 3
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: 2
                    color: modelData.checked ? CoderTheme.success : "transparent"
                    border.color: modelData.checked ? CoderTheme.success : CoderTheme.border
                    border.width: 1
                    Label {
                        anchors.centerIn: parent
                        visible: modelData.checked
                        text: "\u2713"
                        color: "#ffffff"
                        font.pixelSize: 9
                    }
                }
                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    Label {
                        text: modelData.index + ". " + modelData.title
                        color: CoderTheme.textPrimary
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    Label {
                        visible: modelData.body.length > 0
                        text: modelData.body
                        color: CoderTheme.textSecondary
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }

        RowLayout {
            spacing: 8
            Layout.topMargin: 4
            CoderButton {
                text: "Implement this plan"
                onClicked: card.chat.implementPlan()
            }
            CoderButton {
                text: "Keep planning"
                variant: "outline"
                onClicked: card.keepPlanningRequested()
            }
        }
    }
}
