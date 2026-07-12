#ifndef DIFFPARSER_H
#define DIFFPARSER_H

#include <QList>
#include <QString>
#include <QVariantList>

/// Splits a server-supplied unified diff into per-file slices for the diff
/// viewer. Semantics are ported from the Android client's DiffParser.kt:
/// a new file begins on every "diff --git " line; a diff without such
/// headers is returned whole as one anonymous "(diff)" file; anything
/// unrecognized rolls forward into the current file's body so no context is
/// lost. "+++"/"---" file headers are excluded from the add/del counts.
namespace DiffParser {

struct DiffFile {
    QString path;     // post-image path; pre-image path on deletions
    QString oldPath;  // pre-image path only when it differs (renames)
    int addCount = 0;
    int delCount = 0;
    QStringList lines;  // full body for this file, including its header
};

[[nodiscard]] QList<DiffFile> parse(const QString& diff);

/// QML-friendly projection: one map per file with keys path, oldPath,
/// addCount, delCount, and hunks. hunks is a list of line maps
/// {text, kind} where kind is one of "add", "del", "meta", "hunk",
/// "context" for per-line coloring.
[[nodiscard]] QVariantList toVariantList(const QList<DiffFile>& files);

}  // namespace DiffParser

#endif  // DIFFPARSER_H
