#ifndef APPENTRY_H
#define APPENTRY_H

#include <QString>

/// Represents a discovered application that can be launched in secure mode.
struct AppEntry {
    QString id;           // e.g. "firefox", "code" — used as key for profile lookup
    QString name;         // Display name (from .desktop Name= or prettified binary name)
    QString exec;         // Command to execute (from .desktop Exec= or binary name)
    QString iconName;     // Icon theme name (from .desktop Icon= or empty)
    QString iconPath;     // Resolved absolute icon file path (or empty)
    QString category;     // "IDE", "Browser", "Editor", "AI Tool", "Terminal", "Other"
    QString desktopFile;  // Path to .desktop file (empty if binary-only discovery)
    bool fromDesktopFile = false;
};

#endif  // APPENTRY_H
