#include "AppDiscovery.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFutureWatcher>
#include <QIcon>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QtConcurrent>

namespace {

struct KnownBinary {
    QString binary;
    QString displayName;
    QString category;
};

// clang-format off
const QVector<KnownBinary> kKnownBinaries = {
    // IDE
    {QStringLiteral("code"),           QStringLiteral("Visual Studio Code"), QStringLiteral("IDE")},
    {QStringLiteral("code-insiders"),   QStringLiteral("VS Code Insiders"),  QStringLiteral("IDE")},
    {QStringLiteral("cursor"),          QStringLiteral("Cursor"),            QStringLiteral("IDE")},
    {QStringLiteral("codium"),          QStringLiteral("VSCodium"),          QStringLiteral("IDE")},
    {QStringLiteral("idea"),            QStringLiteral("IntelliJ IDEA"),     QStringLiteral("IDE")},
    {QStringLiteral("goland"),          QStringLiteral("GoLand"),            QStringLiteral("IDE")},
    {QStringLiteral("pycharm"),         QStringLiteral("PyCharm"),           QStringLiteral("IDE")},
    {QStringLiteral("webstorm"),        QStringLiteral("WebStorm"),          QStringLiteral("IDE")},
    {QStringLiteral("clion"),           QStringLiteral("CLion"),             QStringLiteral("IDE")},
    {QStringLiteral("phpstorm"),        QStringLiteral("PhpStorm"),          QStringLiteral("IDE")},
    {QStringLiteral("rider"),           QStringLiteral("Rider"),             QStringLiteral("IDE")},
    {QStringLiteral("rubymine"),        QStringLiteral("RubyMine"),          QStringLiteral("IDE")},
    {QStringLiteral("datagrip"),        QStringLiteral("DataGrip"),          QStringLiteral("IDE")},
    {QStringLiteral("fleet"),           QStringLiteral("Fleet"),             QStringLiteral("IDE")},
    {QStringLiteral("zed"),             QStringLiteral("Zed"),               QStringLiteral("IDE")},
    {QStringLiteral("lapce"),           QStringLiteral("Lapce"),             QStringLiteral("IDE")},
    {QStringLiteral("antigravity"),     QStringLiteral("Antigravity"),       QStringLiteral("IDE")},
    // Browser
    {QStringLiteral("firefox"),             QStringLiteral("Firefox"),        QStringLiteral("Browser")},
    {QStringLiteral("google-chrome"),       QStringLiteral("Google Chrome"),  QStringLiteral("Browser")},
    {QStringLiteral("google-chrome-stable"),QStringLiteral("Google Chrome"),  QStringLiteral("Browser")},
    {QStringLiteral("chromium"),            QStringLiteral("Chromium"),       QStringLiteral("Browser")},
    {QStringLiteral("chromium-browser"),    QStringLiteral("Chromium"),       QStringLiteral("Browser")},
    {QStringLiteral("brave-browser"),       QStringLiteral("Brave"),          QStringLiteral("Browser")},
    {QStringLiteral("vivaldi"),             QStringLiteral("Vivaldi"),        QStringLiteral("Browser")},
    {QStringLiteral("zen-browser"),         QStringLiteral("Zen Browser"),    QStringLiteral("Browser")},
    {QStringLiteral("microsoft-edge"),      QStringLiteral("Microsoft Edge"), QStringLiteral("Browser")},
    // Editor
    {QStringLiteral("gedit"),        QStringLiteral("gedit"),        QStringLiteral("Editor")},
    {QStringLiteral("kate"),         QStringLiteral("Kate"),         QStringLiteral("Editor")},
    {QStringLiteral("kwrite"),       QStringLiteral("KWrite"),       QStringLiteral("Editor")},
    {QStringLiteral("mousepad"),     QStringLiteral("Mousepad"),     QStringLiteral("Editor")},
    {QStringLiteral("xed"),          QStringLiteral("Xed"),          QStringLiteral("Editor")},
    {QStringLiteral("pluma"),        QStringLiteral("Pluma"),        QStringLiteral("Editor")},
    {QStringLiteral("sublime_text"), QStringLiteral("Sublime Text"), QStringLiteral("Editor")},
    {QStringLiteral("subl"),         QStringLiteral("Sublime Text"), QStringLiteral("Editor")},
    {QStringLiteral("emacs"),        QStringLiteral("Emacs"),        QStringLiteral("Editor")},
    {QStringLiteral("gvim"),         QStringLiteral("GVim"),         QStringLiteral("Editor")},
    {QStringLiteral("neovide"),      QStringLiteral("Neovide"),      QStringLiteral("Editor")},
    // AI Tool
    {QStringLiteral("claude"),    QStringLiteral("Claude"),   QStringLiteral("AI Tool")},
    {QStringLiteral("aider"),     QStringLiteral("Aider"),    QStringLiteral("AI Tool")},
    {QStringLiteral("continue"),  QStringLiteral("Continue"), QStringLiteral("AI Tool")},
    // Terminal
    {QStringLiteral("alacritty"),      QStringLiteral("Alacritty"),      QStringLiteral("Terminal")},
    {QStringLiteral("kitty"),          QStringLiteral("Kitty"),          QStringLiteral("Terminal")},
    {QStringLiteral("wezterm"),        QStringLiteral("WezTerm"),        QStringLiteral("Terminal")},
    {QStringLiteral("foot"),           QStringLiteral("Foot"),           QStringLiteral("Terminal")},
    {QStringLiteral("gnome-terminal"), QStringLiteral("GNOME Terminal"), QStringLiteral("Terminal")},
    {QStringLiteral("konsole"),        QStringLiteral("Konsole"),        QStringLiteral("Terminal")},
    {QStringLiteral("xfce4-terminal"), QStringLiteral("Xfce Terminal"), QStringLiteral("Terminal")},
    {QStringLiteral("tilix"),          QStringLiteral("Tilix"),          QStringLiteral("Terminal")},
};
// clang-format on

/// Desktop file search directories (standard XDG + Flatpak + Snap locations).
QStringList desktopFileDirs() {
    QStringList dirs = {
        QStringLiteral("/usr/share/applications"),
        QStringLiteral("/usr/local/share/applications"),
        QDir::homePath() + QStringLiteral("/.local/share/applications"),
        QStringLiteral("/var/lib/flatpak/exports/share/applications"),
        QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/applications"),
    };

    // Snap directories: /snap/*/current/meta/gui/
    QDir snapDir(QStringLiteral("/snap"));
    if (snapDir.exists()) {
        const auto snapEntries = snapDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto& snap : snapEntries) {
            QString guiDir = QStringLiteral("/snap/") + snap + QStringLiteral("/current/meta/gui");
            if (QDir(guiDir).exists()) {
                dirs.append(guiDir);
            }
        }
    }

    return dirs;
}

/// Parse a single .desktop file and return an AppEntry (or nullopt on skip).
std::optional<AppEntry> parseDesktopFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }

    QTextStream stream(&file);

    bool inDesktopEntry = false;
    QString name;
    QString exec;
    QString icon;
    QString categories;
    QString type;
    bool noDisplay = false;
    bool hidden = false;

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            if (line == QStringLiteral("[Desktop Entry]")) {
                inDesktopEntry = true;
            } else {
                // We've left [Desktop Entry]; stop parsing.
                if (inDesktopEntry) {
                    break;
                }
            }
            continue;
        }

        if (!inDesktopEntry) {
            continue;
        }

        int eqIdx = line.indexOf(QLatin1Char('='));
        if (eqIdx < 1) {
            continue;
        }
        QString key = line.left(eqIdx).trimmed();
        QString value = line.mid(eqIdx + 1).trimmed();

        if (key == QStringLiteral("Name")) {
            name = value;
        } else if (key == QStringLiteral("Exec")) {
            exec = value;
        } else if (key == QStringLiteral("Icon")) {
            icon = value;
        } else if (key == QStringLiteral("Categories")) {
            categories = value;
        } else if (key == QStringLiteral("Type")) {
            type = value;
        } else if (key == QStringLiteral("NoDisplay")) {
            noDisplay = (value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
        } else if (key == QStringLiteral("Hidden")) {
            hidden = (value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
        }
    }

    // Filter: must be Type=Application, not hidden/nodisplay, and have Name+Exec.
    if (type != QStringLiteral("Application") || noDisplay || hidden) {
        return std::nullopt;
    }
    if (name.isEmpty() || exec.isEmpty()) {
        return std::nullopt;
    }

    AppEntry entry;
    entry.name = name;
    entry.exec = AppDiscovery::cleanExecLine(exec);
    entry.id = AppDiscovery::binaryNameFromExec(entry.exec);
    entry.category = AppDiscovery::categorizeFromDesktopCategories(categories);
    entry.desktopFile = filePath;
    entry.fromDesktopFile = true;

    if (!icon.isEmpty()) {
        if (icon.startsWith(QLatin1Char('/'))) {
            entry.iconPath = icon;
            entry.iconName = icon;
        } else if (QIcon::hasThemeIcon(icon)) {
            entry.iconName = icon;
        }
        // else: icon not found in theme, leave iconName empty → fallback emoji
    }

    return entry;
}

}  // namespace

AppDiscovery::AppDiscovery(QObject* parent) : QObject(parent) {}

QVector<AppEntry> AppDiscovery::scan() const {
    auto apps = scanDesktopFiles();
    auto pathApps = scanPathBinaries(apps);
    apps.append(pathApps);
    return apps;
}

void AppDiscovery::scanAsync() {
    auto* watcher = new QFutureWatcher<QVector<AppEntry>>(this);
    connect(watcher, &QFutureWatcher<QVector<AppEntry>>::finished, this, [this, watcher]() {
        emit scanComplete(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([this]() { return scan(); }));
}

QVector<AppEntry> AppDiscovery::scanDesktopFiles() const {
    QVector<AppEntry> results;
    QSet<QString> seenIds;

    const auto dirs = desktopFileDirs();
    for (const auto& dirPath : dirs) {
        QDirIterator it(dirPath, {QStringLiteral("*.desktop")}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            auto entry = parseDesktopFile(it.filePath());
            if (!entry.has_value()) {
                continue;
            }
            // Deduplicate by id — first discovered wins (higher-priority dirs listed first).
            if (seenIds.contains(entry->id)) {
                continue;
            }
            seenIds.insert(entry->id);
            results.append(std::move(*entry));
        }
    }

    return results;
}

QVector<AppEntry> AppDiscovery::scanPathBinaries(const QVector<AppEntry>& existing) const {
    QSet<QString> existingIds;
    for (const auto& app : existing) {
        existingIds.insert(app.id);
    }

    QVector<AppEntry> results;
    for (const auto& known : kKnownBinaries) {
        if (existingIds.contains(known.binary)) {
            continue;
        }
        QString path = QStandardPaths::findExecutable(known.binary);
        if (path.isEmpty()) {
            continue;
        }
        AppEntry entry;
        entry.id = known.binary;
        entry.name = known.displayName;
        entry.exec = known.binary;
        entry.category = known.category;
        entry.fromDesktopFile = false;
        results.append(std::move(entry));
    }

    return results;
}

QString AppDiscovery::categorizeFromDesktopCategories(const QString& categories) {
    if (categories.isEmpty()) {
        return QStringLiteral("Other");
    }
    // Split on ';' and check known category keywords.
    const auto parts = categories.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const auto& part : parts) {
        QString trimmed = part.trimmed();
        if (trimmed == QStringLiteral("IDE") || trimmed == QStringLiteral("Development")) {
            return QStringLiteral("IDE");
        }
    }
    for (const auto& part : parts) {
        QString trimmed = part.trimmed();
        if (trimmed == QStringLiteral("WebBrowser")) {
            return QStringLiteral("Browser");
        }
    }
    for (const auto& part : parts) {
        QString trimmed = part.trimmed();
        if (trimmed == QStringLiteral("TextEditor")) {
            return QStringLiteral("Editor");
        }
    }
    for (const auto& part : parts) {
        QString trimmed = part.trimmed();
        if (trimmed == QStringLiteral("TerminalEmulator")) {
            return QStringLiteral("Terminal");
        }
    }
    return QStringLiteral("Other");
}

QString AppDiscovery::cleanExecLine(const QString& exec) {
    // Remove XDG field codes: %u %U %f %F %i %c %k and their variants.
    static const QRegularExpression fieldCodeRe(QStringLiteral(R"(\s+%[uUfFickdDnNvm]\b)"));
    QString cleaned = exec;
    cleaned.remove(fieldCodeRe);
    return cleaned.trimmed();
}

QString AppDiscovery::binaryNameFromExec(const QString& exec) {
    // Extract the first token (the binary), then take just the filename portion.
    QString firstToken = exec.section(QLatin1Char(' '), 0, 0);
    // Handle "env VAR=val binary" style Exec lines.
    if (firstToken == QStringLiteral("env")) {
        // Skip env and any VAR=VAL tokens.
        const auto tokens = exec.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (int i = 1; i < tokens.size(); ++i) {
            if (!tokens[i].contains(QLatin1Char('='))) {
                firstToken = tokens[i];
                break;
            }
        }
    }
    // Return just the filename, not the full path.
    int lastSlash = firstToken.lastIndexOf(QLatin1Char('/'));
    if (lastSlash >= 0) {
        return firstToken.mid(lastSlash + 1);
    }
    return firstToken;
}
