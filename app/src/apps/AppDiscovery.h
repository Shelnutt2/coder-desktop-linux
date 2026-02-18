#ifndef APPDISCOVERY_H
#define APPDISCOVERY_H

#include <QObject>
#include <QVector>

#include "AppEntry.h"

/// Discovers launchable applications via XDG .desktop files and $PATH probing.
class AppDiscovery : public QObject {
    Q_OBJECT

public:
    explicit AppDiscovery(QObject* parent = nullptr);

    /// Perform synchronous scan. Returns list of discovered apps.
    [[nodiscard]] QVector<AppEntry> scan() const;

    /// Start async scan (uses QtConcurrent). Emits scanComplete when done.
    void scanAsync();

signals:
    void scanComplete(const QVector<AppEntry>& apps);

private:
    [[nodiscard]] QVector<AppEntry> scanDesktopFiles() const;
    [[nodiscard]] QVector<AppEntry> scanPathBinaries(const QVector<AppEntry>& existing) const;
    [[nodiscard]] static QString categorizeFromDesktopCategories(const QString& categories);
    [[nodiscard]] static QString cleanExecLine(const QString& exec);
    [[nodiscard]] static QString binaryNameFromExec(const QString& exec);
};

#endif  // APPDISCOVERY_H
