#ifndef ASKUSERQUESTIONPARSER_H
#define ASKUSERQUESTIONPARSER_H

#include <QString>
#include <QVariantList>
#include <QVariantMap>

/// Parses ask_user_question tool-call arguments and formats the outgoing
/// answer message, matching the web UI's AskUserQuestionTool semantics:
///
/// - Args JSON: {"questions": [{"header", "question",
///   "options": [{"label", "description"}]}]}. Options whose label is
///   "other" (case-insensitive) are dropped; the UI always appends its own
///   free-text "Other" option.
/// - Outgoing message: a single question sends just the answer text
///   ("Other: <text>" for free-form answers, otherwise the option label);
///   multiple questions send numbered lines "N. <header>: <answer>".
///
/// parse() returns an empty list for anything that is not yet a valid
/// questions object (including truncated streaming JSON), so callers can
/// fall back to the generic tool-call card until the args are complete.
namespace AskUserQuestionParser {

/// Returns a list of maps {header (QString), question (QString),
/// options (QVariantList of {label, description})}. Empty when argsJson is
/// not a JSON object with a non-empty "questions" array of objects.
[[nodiscard]] QVariantList parse(const QString& argsJson);

/// Formats one answer map {kind: "option"|"other", label, text}.
[[nodiscard]] QString formatAnswer(const QVariantMap& answer);

/// Formats the outgoing chat message for the given questions and answers
/// (parallel lists). Empty header falls back to "Question N".
[[nodiscard]] QString formatAnswers(const QVariantList& questions, const QVariantList& answers);

}  // namespace AskUserQuestionParser

#endif  // ASKUSERQUESTIONPARSER_H
