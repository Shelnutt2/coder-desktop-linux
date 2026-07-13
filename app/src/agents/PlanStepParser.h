#ifndef PLANSTEPPARSER_H
#define PLANSTEPPARSER_H

#include <QList>
#include <QString>
#include <QVariantList>

/// Parses an assistant plan message's Markdown body into an ordered list of
/// plan steps. Semantics are ported from the Android client's
/// PlanStepParser.kt:
///
/// Three input shapes are recognized, tried in priority order; the first
/// pattern that matches at least one step wins and patterns are never mixed
/// within a single parse pass:
///  1. Markdown headings ("## Step 1: Foo", "### 2. Bar") at heading levels
///     2 to 4; the literal word "step" is matched case-insensitively.
///  2. Ordered lists ("1. Foo", "2) Bar") indented at most 3 columns.
///  3. Task lists ("- [ ] Foo", "- [x] Bar"); "[x]" marks the step checked.
///
/// The body of a step is the text between the end of its title line and the
/// start of the next match (or end of input), trimmed. The step index is a
/// monotonic 1-based counter; numbers captured from the source are ignored.
/// Results are capped at 100 steps; blank titles are skipped. Empty input
/// yields an empty list.
namespace PlanStepParser {

struct PlanStep {
    int index = 0;
    QString title;
    QString body;
    bool checked = false;  // true only for task-list "[x]" entries
};

[[nodiscard]] QList<PlanStep> parse(const QString& markdownText);

/// QML-friendly projection: list of maps with keys index, title, body,
/// checked.
[[nodiscard]] QVariantList toVariantList(const QList<PlanStep>& steps);

}  // namespace PlanStepParser

#endif  // PLANSTEPPARSER_H
