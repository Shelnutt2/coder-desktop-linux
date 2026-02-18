#ifndef APPLAUNCHPROFILE_H
#define APPLAUNCHPROFILE_H

#include <QString>
#include <QStringList>

/// Default sandbox configuration for a known application.
struct LaunchProfile {
    bool isolatePid = true;
    bool isolateIpc = true;
    bool isolateNetwork = false;
    bool isolateFilesystem = false;
    bool bindHomeRw = false;
    QStringList extraBindPaths;
};

/// Registry of curated launch profiles for common applications.
class AppLaunchProfile {
public:
    /// Get the default profile for an app. Returns a conservative default if unknown.
    [[nodiscard]] static LaunchProfile profileFor(const QString& appId);

    /// Returns true if a curated profile exists for this app ID.
    [[nodiscard]] static bool hasProfile(const QString& appId);

    /// Get all known profile app IDs (for display/debug).
    [[nodiscard]] static QStringList knownAppIds();
};

#endif  // APPLAUNCHPROFILE_H
