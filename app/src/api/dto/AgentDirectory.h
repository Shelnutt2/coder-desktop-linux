#ifndef CODER_DTO_AGENTDIRECTORY_H
#define CODER_DTO_AGENTDIRECTORY_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

// ---------------------------------------------------------------------------
// DTOs for the workspace agent's list-directory API (port 4)
// ---------------------------------------------------------------------------

/// A single entry (file or directory) returned by the agent.
struct DirectoryEntry {
    Q_GADGET
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString absolutePathString MEMBER absolutePathString)
    Q_PROPERTY(bool isDir MEMBER isDir)

public:
    QString name;
    QString absolutePathString;
    bool isDir = false;

    [[nodiscard]] bool operator==(const DirectoryEntry& other) const {
        return name == other.name && absolutePathString == other.absolutePathString &&
               isDir == other.isDir;
    }

    [[nodiscard]] static DirectoryEntry fromJson(const QJsonObject& json) {
        return DirectoryEntry{
            .name = json.value(QLatin1String("name")).toString(),
            .absolutePathString = json.value(QLatin1String("absolute_path_string")).toString(),
            .isDir = json.value(QLatin1String("is_dir")).toBool(false),
        };
    }
};

/// The full response from POST /api/v0/list-directory.
struct DirectoryListing {
    Q_GADGET
    Q_PROPERTY(QStringList absolutePath MEMBER absolutePath)
    Q_PROPERTY(QString absolutePathString MEMBER absolutePathString)
    Q_PROPERTY(QList<DirectoryEntry> contents MEMBER contents)

public:
    QStringList absolutePath;
    QString absolutePathString;
    QList<DirectoryEntry> contents;

    [[nodiscard]] static DirectoryListing fromJson(const QJsonObject& json) {
        DirectoryListing listing;

        const QJsonArray pathArray = json.value(QLatin1String("absolute_path")).toArray();
        listing.absolutePath.reserve(static_cast<int>(pathArray.size()));
        for (const auto& segment : pathArray) {
            listing.absolutePath.append(segment.toString());
        }

        listing.absolutePathString = json.value(QLatin1String("absolute_path_string")).toString();

        const QJsonArray contentsArray = json.value(QLatin1String("contents")).toArray();
        listing.contents.reserve(static_cast<int>(contentsArray.size()));
        for (const auto& entry : contentsArray) {
            listing.contents.append(DirectoryEntry::fromJson(entry.toObject()));
        }

        return listing;
    }
};

Q_DECLARE_METATYPE(DirectoryEntry)
Q_DECLARE_METATYPE(DirectoryListing)

#endif  // CODER_DTO_AGENTDIRECTORY_H
