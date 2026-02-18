#include "AppLaunchProfile.h"

#include <QHash>

namespace {

/// Build the static registry of curated launch profiles.
const QHash<QString, LaunchProfile>& profileRegistry() {
    static const QHash<QString, LaunchProfile> registry = []() {
        QHash<QString, LaunchProfile> reg;

        // Helper lambdas for common profile shapes.
        auto addBrowser = [&](const QString& id) {
            // Browsers need multiprocess, shared memory, home for profiles.
            reg.insert(id, {/*.isolatePid=*/false,
                            /*.isolateIpc=*/false,
                            /*.isolateNetwork=*/false,
                            /*.isolateFilesystem=*/false,
                            /*.bindHomeRw=*/true,
                            /*.extraBindPaths=*/{}});
        };

        auto addElectronIde = [&](const QString& id) {
            // Electron apps need IPC for extension host processes.
            reg.insert(id, {false, false, false, false, true, {}});
        };

        auto addJetBrainsIde = [&](const QString& id) {
            // Java-based, needs home for caches/config but IPC isolation is OK.
            reg.insert(id, {false, true, false, false, true, {}});
        };

        auto addIsolated = [&](const QString& id) {
            // Strict isolation: PID + IPC isolated, no special filesystem access.
            reg.insert(id, {true, true, false, false, false, {}});
        };

        // --- Browsers ---
        for (const auto& id : {QStringLiteral("firefox"), QStringLiteral("google-chrome"),
                               QStringLiteral("google-chrome-stable"), QStringLiteral("chromium"),
                               QStringLiteral("chromium-browser"), QStringLiteral("brave-browser"),
                               QStringLiteral("vivaldi"), QStringLiteral("zen-browser"),
                               QStringLiteral("microsoft-edge")}) {
            addBrowser(id);
        }

        // --- Electron IDEs ---
        for (const auto& id : {QStringLiteral("code"), QStringLiteral("code-insiders"),
                               QStringLiteral("cursor"), QStringLiteral("codium")}) {
            addElectronIde(id);
        }

        // --- JetBrains IDEs ---
        for (const auto& id :
             {QStringLiteral("idea"), QStringLiteral("goland"), QStringLiteral("pycharm"),
              QStringLiteral("webstorm"), QStringLiteral("clion"), QStringLiteral("phpstorm"),
              QStringLiteral("rider"), QStringLiteral("rubymine"), QStringLiteral("datagrip")}) {
            addJetBrainsIde(id);
        }

        // --- Other IDEs (isolated) ---
        for (const auto& id : {QStringLiteral("zed"), QStringLiteral("lapce"),
                               QStringLiteral("fleet"), QStringLiteral("antigravity")}) {
            addIsolated(id);
        }

        // --- Editors (isolated) ---
        for (const auto& id :
             {QStringLiteral("gedit"), QStringLiteral("kate"), QStringLiteral("kwrite"),
              QStringLiteral("mousepad"), QStringLiteral("xed"), QStringLiteral("pluma"),
              QStringLiteral("sublime_text"), QStringLiteral("subl"), QStringLiteral("emacs"),
              QStringLiteral("gvim"), QStringLiteral("neovide")}) {
            addIsolated(id);
        }

        // --- AI Tools (isolated) ---
        for (const auto& id :
             {QStringLiteral("claude"), QStringLiteral("aider"), QStringLiteral("continue")}) {
            addIsolated(id);
        }

        // --- Terminals (isolated) ---
        for (const auto& id :
             {QStringLiteral("alacritty"), QStringLiteral("kitty"), QStringLiteral("wezterm"),
              QStringLiteral("foot"), QStringLiteral("gnome-terminal"), QStringLiteral("konsole"),
              QStringLiteral("xfce4-terminal"), QStringLiteral("tilix")}) {
            addIsolated(id);
        }

        return reg;
    }();
    return registry;
}

}  // namespace

LaunchProfile AppLaunchProfile::profileFor(const QString& appId) {
    const auto& reg = profileRegistry();
    auto it = reg.find(appId);
    if (it != reg.end()) {
        return it.value();
    }
    // Conservative default for unknown apps.
    return {/*.isolatePid=*/true,
            /*.isolateIpc=*/true,
            /*.isolateNetwork=*/false,
            /*.isolateFilesystem=*/false,
            /*.bindHomeRw=*/false,
            /*.extraBindPaths=*/{}};
}

bool AppLaunchProfile::hasProfile(const QString& appId) {
    return profileRegistry().contains(appId);
}

QStringList AppLaunchProfile::knownAppIds() {
    return profileRegistry().keys();
}
