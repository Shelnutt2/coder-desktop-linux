#include "agents/DiffParser.h"

#include <QRegularExpression>
#include <QVariantMap>

namespace DiffParser {

namespace {

const QRegularExpression& headerRegex() {
    static const QRegularExpression re(QStringLiteral(R"(^diff --git a/(.+?) b/(.+?)$)"));
    return re;
}

void countAddRemove(const QStringList& slice, int* added, int* removed) {
    *added = 0;
    *removed = 0;
    for (const QString& l : slice) {
        if (l.startsWith(QLatin1String("+++")) || l.startsWith(QLatin1String("---"))) continue;
        if (l.startsWith(QLatin1Char('+')))
            ++*added;
        else if (l.startsWith(QLatin1Char('-')))
            ++*removed;
    }
}

DiffFile makeFile(const QStringList& slice) {
    DiffFile f;
    f.lines = slice;
    countAddRemove(slice, &f.addCount, &f.delCount);

    const QRegularExpressionMatch m = headerRegex().match(slice.value(0));
    QString oldPath = QStringLiteral("?");
    QString newPath = QStringLiteral("?");
    if (m.hasMatch()) {
        oldPath = m.captured(1);
        newPath = m.captured(2);
    }
    f.path = (newPath != QLatin1String("/dev/null")) ? newPath : oldPath;
    if (oldPath != newPath && oldPath != QLatin1String("/dev/null")) f.oldPath = oldPath;
    return f;
}

QString lineKind(const QString& l) {
    if (l.startsWith(QLatin1String("diff ")) || l.startsWith(QLatin1String("index ")) ||
        l.startsWith(QLatin1String("+++")) || l.startsWith(QLatin1String("---")) ||
        l.startsWith(QLatin1String("new file")) || l.startsWith(QLatin1String("deleted file")) ||
        l.startsWith(QLatin1String("rename ")) || l.startsWith(QLatin1String("similarity ")))
        return QStringLiteral("meta");
    if (l.startsWith(QLatin1String("@@"))) return QStringLiteral("hunk");
    if (l.startsWith(QLatin1Char('+'))) return QStringLiteral("add");
    if (l.startsWith(QLatin1Char('-'))) return QStringLiteral("del");
    return QStringLiteral("context");
}

}  // namespace

QList<DiffFile> parse(const QString& diff) {
    if (diff.trimmed().isEmpty()) return {};
    const QStringList lines = diff.split(QLatin1Char('\n'));

    QList<qsizetype> headerIndices;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (lines.at(i).startsWith(QLatin1String("diff --git "))) headerIndices.append(i);
    }

    if (headerIndices.isEmpty()) {
        // Hand-crafted patch without git headers: one anonymous file so the
        // viewer still renders something.
        DiffFile f;
        f.path = QStringLiteral("(diff)");
        f.lines = lines;
        countAddRemove(lines, &f.addCount, &f.delCount);
        return {f};
    }

    QList<DiffFile> out;
    out.reserve(headerIndices.size());
    headerIndices.append(lines.size());
    for (qsizetype i = 0; i + 1 < headerIndices.size(); ++i) {
        out.append(makeFile(
            lines.mid(headerIndices.at(i), headerIndices.at(i + 1) - headerIndices.at(i))));
    }
    return out;
}

QVariantList toVariantList(const QList<DiffFile>& files) {
    QVariantList list;
    list.reserve(files.size());
    for (const DiffFile& f : files) {
        QVariantList hunks;
        hunks.reserve(f.lines.size());
        for (const QString& l : f.lines) {
            QVariantMap line;
            line.insert(QStringLiteral("text"), l);
            line.insert(QStringLiteral("kind"), lineKind(l));
            hunks.append(line);
        }
        QVariantMap m;
        m.insert(QStringLiteral("path"), f.path);
        m.insert(QStringLiteral("oldPath"), f.oldPath);
        m.insert(QStringLiteral("addCount"), f.addCount);
        m.insert(QStringLiteral("delCount"), f.delCount);
        m.insert(QStringLiteral("hunks"), hunks);
        list.append(m);
    }
    return list;
}

}  // namespace DiffParser
