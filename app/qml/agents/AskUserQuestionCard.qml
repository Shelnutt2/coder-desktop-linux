import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CoderDesktop

// Interactive card for ask_user_question tool calls, modeled on the web
// UI's AskUserQuestionTool: each question renders its header, question
// text, selectable options (label bold, description muted), and a
// free-text "Other" option. Submit is enabled only once every question has
// a valid answer and sends a normal chat message formatted exactly like
// the web UI (single question: just the answer; multiple: numbered
// "N. <header>: <answer>" lines).
//
// The card becomes read-only once the answer was submitted here or a newer
// user message exists in the chat (answered elsewhere).
Rectangle {
    id: card
    // The page's ChatController context object.
    property var chat: null
    // Parsed questions from ChatController.parseAskUserQuestions:
    // [{header, question, options: [{label, description}]}].
    property var questions: []
    property bool isError: false
    // True when a newer user message exists in the timeline.
    property bool answered: false

    // Per-question answers: {kind: "option"|"other", label, optionIndex,
    // text} or null when unanswered. The first option is preselected,
    // matching the web UI.
    property var answers: []
    property bool submitted: false

    readonly property bool interactive: !answered && !submitted && chat !== null && !isError
    readonly property bool canSubmit: {
        if (!interactive) return false
        for (var i = 0; i < questions.length; ++i) {
            var a = answers[i]
            if (!a) return false
            if (a.kind === "other" && a.text.trim().length === 0) return false
            if (a.kind === "option" && a.label.trim().length === 0) return false
        }
        return questions.length > 0
    }

    function resetAnswers() {
        var list = []
        for (var i = 0; i < questions.length; ++i) {
            var opts = questions[i].options
            list.push(opts.length > 0
                ? { kind: "option",
                    label: opts[0].label.length > 0 ? opts[0].label : "Option 1",
                    optionIndex: 0, text: "" }
                : null)
        }
        answers = list
    }
    onQuestionsChanged: resetAnswers()
    Component.onCompleted: resetAnswers()

    function setAnswer(questionIndex, answer) {
        var list = answers.slice()
        list[questionIndex] = answer
        answers = list
    }

    function submit() {
        if (!canSubmit || !chat) return
        var message = chat.formatAskUserAnswers(questions, answers)
        if (message.trim().length === 0) return
        chat.sendMessage(message)
        submitted = true
    }

    implicitHeight: col.implicitHeight + 24
    radius: CoderTheme.radius
    color: CoderTheme.surface
    border.color: card.interactive ? CoderTheme.info : CoderTheme.border
    border.width: 1

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        spacing: 10

        RowLayout {
            spacing: 6
            Image {
                source: "qrc:/CoderDesktop/assets/icons/question.svg"
                sourceSize.width: 14
                sourceSize.height: 14
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 2
            }
            Label {
                text: card.interactive
                    ? "The agent has a question for you"
                    : "Question answered"
                color: card.interactive ? CoderTheme.info : CoderTheme.textSecondary
                font.pixelSize: 12
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
        }

        Repeater {
            model: card.questions

            ColumnLayout {
                id: questionBlock
                required property var modelData
                required property int index
                readonly property var answer: card.answers[index] || null
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: questionBlock.modelData.header.length > 0
                        ? questionBlock.modelData.header
                        : "Question " + (questionBlock.index + 1)
                    visible: card.questions.length > 1
                             || questionBlock.modelData.header.length > 0
                    color: CoderTheme.textSecondary
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.capitalization: Font.AllUppercase
                }
                Label {
                    text: questionBlock.modelData.question.length > 0
                        ? questionBlock.modelData.question
                        : "No question provided."
                    color: CoderTheme.textPrimary
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                // Options box: model options plus the synthetic "Other".
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: optionsCol.implicitHeight + 12
                    radius: CoderTheme.radiusSm
                    color: "transparent"
                    border.color: CoderTheme.border
                    border.width: 1

                    ColumnLayout {
                        id: optionsCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 6
                        spacing: 2

                        Repeater {
                            model: questionBlock.modelData.options

                            Rectangle {
                                id: optionRow
                                required property var modelData
                                required property int index
                                readonly property bool selected:
                                    questionBlock.answer !== null
                                    && questionBlock.answer.kind === "option"
                                    && questionBlock.answer.optionIndex === index
                                Layout.fillWidth: true
                                implicitHeight: optionContent.implicitHeight + 10
                                radius: CoderTheme.radiusSm
                                color: selected ? CoderTheme.activeSurface
                                     : optionHover.hovered && card.interactive
                                       ? CoderTheme.hoverBg : "transparent"

                                RowLayout {
                                    id: optionContent
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 6
                                    anchors.rightMargin: 6
                                    spacing: 8

                                    // Radio indicator.
                                    Rectangle {
                                        width: 14; height: 14; radius: 7
                                        Layout.alignment: Qt.AlignTop
                                        Layout.topMargin: 2
                                        color: "transparent"
                                        border.color: optionRow.selected
                                            ? CoderTheme.primary : CoderTheme.border
                                        border.width: optionRow.selected ? 4 : 1
                                    }
                                    ColumnLayout {
                                        spacing: 0
                                        Layout.fillWidth: true
                                        Label {
                                            text: optionRow.modelData.label.length > 0
                                                ? optionRow.modelData.label
                                                : "Option " + (optionRow.index + 1)
                                            color: CoderTheme.textPrimary
                                            font.pixelSize: 12
                                            font.weight: Font.Medium
                                            wrapMode: Text.Wrap
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            visible: optionRow.modelData.description.length > 0
                                            text: optionRow.modelData.description
                                            color: CoderTheme.textSecondary
                                            font.pixelSize: 11
                                            wrapMode: Text.Wrap
                                            Layout.fillWidth: true
                                        }
                                    }
                                }

                                HoverHandler {
                                    id: optionHover
                                    enabled: card.interactive
                                    cursorShape: Qt.PointingHandCursor
                                }
                                TapHandler {
                                    enabled: card.interactive
                                    gesturePolicy: TapHandler.DragThreshold
                                    onTapped: card.setAnswer(questionBlock.index, {
                                        kind: "option",
                                        label: optionRow.modelData.label.length > 0
                                            ? optionRow.modelData.label
                                            : "Option " + (optionRow.index + 1),
                                        optionIndex: optionRow.index,
                                        text: ""
                                    })
                                }
                            }
                        }

                        // Synthetic free-text "Other" option.
                        Rectangle {
                            id: otherRow
                            readonly property bool selected:
                                questionBlock.answer !== null
                                && questionBlock.answer.kind === "other"
                            Layout.fillWidth: true
                            implicitHeight: otherContent.implicitHeight + 10
                            radius: CoderTheme.radiusSm
                            color: selected ? CoderTheme.activeSurface
                                 : otherHover.hovered && card.interactive
                                   ? CoderTheme.hoverBg : "transparent"

                            ColumnLayout {
                                id: otherContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                spacing: 4

                                RowLayout {
                                    spacing: 8
                                    Layout.fillWidth: true
                                    Rectangle {
                                        width: 14; height: 14; radius: 7
                                        color: "transparent"
                                        border.color: otherRow.selected
                                            ? CoderTheme.primary : CoderTheme.border
                                        border.width: otherRow.selected ? 4 : 1
                                    }
                                    ColumnLayout {
                                        spacing: 0
                                        Layout.fillWidth: true
                                        Label {
                                            text: "Other"
                                            color: CoderTheme.textPrimary
                                            font.pixelSize: 12
                                            font.weight: Font.Medium
                                        }
                                        Label {
                                            text: "Share a different answer."
                                            color: CoderTheme.textSecondary
                                            font.pixelSize: 11
                                        }
                                    }
                                }
                                TextField {
                                    id: otherField
                                    visible: otherRow.selected
                                    enabled: card.interactive
                                    placeholderText: "Describe another answer"
                                    font.pixelSize: 12
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 22
                                    Layout.bottomMargin: 2
                                    text: otherRow.selected && questionBlock.answer
                                        ? questionBlock.answer.text : ""
                                    onTextEdited: card.setAnswer(questionBlock.index, {
                                        kind: "other", label: "", optionIndex: -1, text: text
                                    })
                                }
                            }

                            HoverHandler {
                                id: otherHover
                                enabled: card.interactive
                                cursorShape: Qt.PointingHandCursor
                            }
                            TapHandler {
                                enabled: card.interactive && !otherRow.selected
                                gesturePolicy: TapHandler.DragThreshold
                                onTapped: {
                                    // Reuse any text already typed in the
                                    // field so re-selecting Other keeps it.
                                    card.setAnswer(questionBlock.index, {
                                        kind: "other", label: "", optionIndex: -1,
                                        text: otherField.text
                                    })
                                    otherField.forceActiveFocus()
                                }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            visible: card.interactive
            spacing: 8
            CoderButton {
                text: card.submitted ? "Submitted" : "Submit"
                enabled: card.canSubmit
                onClicked: card.submit()
            }
        }

        Label {
            visible: card.submitted
            text: "Answer sent."
            color: CoderTheme.textSecondary
            font.pixelSize: 11
        }
    }
}
