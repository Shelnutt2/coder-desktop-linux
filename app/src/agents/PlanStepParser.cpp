#include "agents/PlanStepParser.h"

#include <QRegularExpression>
#include <QVariantMap>

namespace PlanStepParser {

namespace {

constexpr int kMaxSteps = 100;

const QRegularExpression& headingRegex() {
    static const QRegularExpression re(
        QStringLiteral(
            R"(^[ \t]*#{2,4}[ \t]+(?:step[ \t]*[.\-:]?[ \t]*)?(\d{1,3})[ \t]*[:.\-)]?[ \t]*(.*)$)"),
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    return re;
}

const QRegularExpression& orderedListRegex() {
    static const QRegularExpression re(QStringLiteral(R"(^[ \t]{0,3}(\d{1,3})[.)][ \t]+(.+)$)"),
                                       QRegularExpression::MultilineOption);
    return re;
}

const QRegularExpression& taskListRegex() {
    static const QRegularExpression re(QStringLiteral(R"(^[ \t]{0,3}-[ \t]+\[( |x|X)][ \t]+(.+)$)"),
                                       QRegularExpression::MultilineOption);
    return re;
}

struct Match {
    qsizetype start = 0;  // offset of the first char of the match
    qsizetype end = 0;    // offset one past the last char of the match
    QString title;
    bool checked = false;
};

QList<PlanStep> buildSteps(const QString& text, const QList<Match>& matches) {
    QList<PlanStep> out;
    out.reserve(qMin<qsizetype>(matches.size(), kMaxSteps));
    for (qsizetype i = 0; i < matches.size(); ++i) {
        if (out.size() >= kMaxSteps) break;
        const Match& m = matches.at(i);
        if (m.title.isEmpty()) continue;  // defensive: heading-only trailing match
        const qsizetype nextStart = (i + 1 < matches.size()) ? matches.at(i + 1).start
                                                             : static_cast<qsizetype>(text.size());
        const qsizetype rangeEnd = qMax(nextStart, m.end);
        // Body = text between the end of the title line and the next match.
        const qsizetype bodyStart = qMin(m.end, static_cast<qsizetype>(text.size()));
        PlanStep step;
        step.index = static_cast<int>(out.size()) + 1;
        step.title = m.title;
        step.body = text.mid(bodyStart, rangeEnd - bodyStart).trimmed();
        step.checked = m.checked;
        out.append(step);
    }
    return out;
}

QList<Match> scan(const QString& text, const QRegularExpression& re, int titleGroup,
                  int checkedGroup) {
    QList<Match> matches;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        Match out;
        out.start = m.capturedStart(0);
        out.end = m.capturedEnd(0);
        out.title = m.captured(titleGroup).trimmed();
        if (checkedGroup > 0)
            out.checked =
                m.captured(checkedGroup).compare(QLatin1String("x"), Qt::CaseInsensitive) == 0;
        matches.append(out);
    }
    return matches;
}

}  // namespace

QList<PlanStep> parse(const QString& markdownText) {
    if (markdownText.trimmed().isEmpty()) return {};

    // Try patterns in priority order; the first that matches wins.
    for (const auto& [re, titleGroup, checkedGroup] :
         {std::tuple{&headingRegex(), 2, 0}, std::tuple{&orderedListRegex(), 2, 0},
          std::tuple{&taskListRegex(), 2, 1}}) {
        const QList<Match> matches = scan(markdownText, *re, titleGroup, checkedGroup);
        if (matches.isEmpty()) continue;
        QList<PlanStep> steps = buildSteps(markdownText, matches);
        if (!steps.isEmpty()) return steps;
    }
    return {};
}

QVariantList toVariantList(const QList<PlanStep>& steps) {
    QVariantList list;
    list.reserve(steps.size());
    for (const PlanStep& s : steps) {
        QVariantMap m;
        m.insert(QStringLiteral("index"), s.index);
        m.insert(QStringLiteral("title"), s.title);
        m.insert(QStringLiteral("body"), s.body);
        m.insert(QStringLiteral("checked"), s.checked);
        list.append(m);
    }
    return list;
}

}  // namespace PlanStepParser
