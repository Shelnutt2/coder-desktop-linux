#include "agents/AskUserQuestionParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace AskUserQuestionParser {

QVariantList parse(const QString& argsJson) {
    const QJsonDocument doc = QJsonDocument::fromJson(argsJson.toUtf8());
    if (!doc.isObject()) return {};
    const QJsonValue questionsValue = doc.object().value(QLatin1String("questions"));
    if (!questionsValue.isArray()) return {};
    const QJsonArray questions = questionsValue.toArray();
    if (questions.isEmpty()) return {};

    QVariantList result;
    result.reserve(questions.size());
    for (const QJsonValue& qv : questions) {
        if (!qv.isObject()) return {};
        const QJsonObject qObj = qv.toObject();

        QVariantList options;
        const QJsonArray rawOptions = qObj.value(QLatin1String("options")).toArray();
        for (const QJsonValue& ov : rawOptions) {
            const QJsonObject oObj = ov.toObject();
            const QString label = oObj.value(QLatin1String("label")).toString();
            // The UI appends its own free-text "Other" option; drop any
            // model-provided duplicate, matching the web UI.
            if (label.trimmed().compare(QLatin1String("other"), Qt::CaseInsensitive) == 0) continue;
            QVariantMap option;
            option.insert(QStringLiteral("label"), label);
            option.insert(QStringLiteral("description"),
                          oObj.value(QLatin1String("description")).toString());
            options.append(option);
        }

        QVariantMap question;
        question.insert(QStringLiteral("header"), qObj.value(QLatin1String("header")).toString());
        question.insert(QStringLiteral("question"),
                        qObj.value(QLatin1String("question")).toString());
        question.insert(QStringLiteral("options"), options);
        result.append(question);
    }
    return result;
}

QString formatAnswer(const QVariantMap& answer) {
    if (answer.value(QStringLiteral("kind")).toString() == QLatin1String("other"))
        return QStringLiteral("Other: %1")
            .arg(answer.value(QStringLiteral("text")).toString().trimmed());
    return answer.value(QStringLiteral("label")).toString();
}

QString formatAnswers(const QVariantList& questions, const QVariantList& answers) {
    if (questions.size() == 1) return formatAnswer(answers.value(0).toMap());

    QStringList lines;
    lines.reserve(questions.size());
    for (int i = 0; i < questions.size(); ++i) {
        QString header = questions.at(i).toMap().value(QStringLiteral("header")).toString();
        if (header.isEmpty()) header = QStringLiteral("Question %1").arg(i + 1);
        lines.append(QStringLiteral("%1. %2: %3")
                         .arg(i + 1)
                         .arg(header, formatAnswer(answers.value(i).toMap())));
    }
    return lines.join(QLatin1Char('\n'));
}

}  // namespace AskUserQuestionParser
