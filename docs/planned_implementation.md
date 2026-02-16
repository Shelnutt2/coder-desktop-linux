# Coder Desktop for Linux — Architecture Overview

This document captures the full architecture plan including amendments.
For the detailed plan, see the plan file. This document covers three
specific refinements requested during review.

---

## 1. DNS Configuration — systemd and non-systemd Support

The VPN shim layer must work on Linux systems **with and without systemd**.
We follow the same strategy as `wg-quick`, which delegates DNS to the
`resolvconf` command — a universal abstraction that works across init systems.

### Detection Cascade

```
DnsManager::configure(servers, searchDomains)

  1. Is `resolvconf` available in PATH?
     YES → pipe nameserver/search lines to:
       printf 'nameserver %s\n' <servers> | resolvconf -a "tun.coder0" -m 0 -x
     Teardown: resolvconf -d "tun.coder0" -f

  2. Is `resolvectl` available?
     YES → call directly:
       resolvectl dns coder0 <servers>
       resolvectl domain coder0 <domains>
     Teardown: resolvectl revert coder0

  3. Fallback: direct /etc/resolv.conf edit
     • Backup original to /etc/resolv.conf.coder-backup
     • Prepend our nameserver + search lines
     Teardown: restore from backup
```

### Why `resolvconf` First

This is exactly what wg-quick does. The `resolvconf` binary is the universal
DNS abstraction on Linux:

- **systemd systems**: `resolvconf` is often a symlink to `resolvectl`. Calling
  it automatically does the right thing via systemd-resolved.
- **openresolv** (non-systemd): Runs subscriber scripts that write
  `/etc/resolv.conf` directly. Works on Void Linux, Alpine, Gentoo, Arch, etc.
- **Direct file fallback**: For minimal systems with neither `resolvconf` nor
  `resolvectl`. We backup, prepend, and restore — fragile but functional.

### Implementation

```cpp
// app/src/vpn/DnsManager.h
class DnsManager {
public:
    enum class Backend { Resolvconf, Resolvectl, DirectFile };

    DnsManager();  // auto-detects backend at construction
    bool configure(const QStringList &nameservers, const QStringList &searchDomains);
    void teardown();

private:
    Backend m_backend;
    QString m_interface{"coder0"};
    QString m_backupPath;  // for DirectFile fallback

    static Backend detectBackend();  // which, resolvconf, resolvectl
};
```

### wg-quick Reference

wg-quick's `set_dns()` (from `/usr/bin/wg-quick`):
```bash
set_dns() {
    [[ ${#DNS[@]} -gt 0 ]] || return 0
    { printf 'nameserver %s\n' "${DNS[@]}"
      [[ ${#DNS_SEARCH[@]} -eq 0 ]] || printf 'search %s\n' "${DNS_SEARCH[*]}"
    } | cmd resolvconf -a "$(resolvconf_iface_prefix)$INTERFACE" -m 0 -x
    HAVE_SET_DNS=1
}
unset_dns() {
    [[ ${#DNS[@]} -gt 0 ]] || return 0
    cmd resolvconf -d "$(resolvconf_iface_prefix)$INTERFACE" -f
}
```

Key flags:
- `-a <iface>` — add/update DNS for this interface
- `-m 0` — highest priority (metric 0)
- `-x` — exclusive (prefer these nameservers over others)
- `-d <iface> -f` — delete, force (ignore errors)

---

## 2. Settings & Preferences Architecture

### Layered Configuration Model

Settings come from three layers, with higher layers overriding lower:

```
┌─────────────────────────────────────────────┐
│  Layer 3: MDM Policy (highest priority)      │
│  /etc/coder-desktop/policy.json              │
│  or dconf: /org/coder/desktop/               │
│  Read-only when present. Locks settings.     │
├─────────────────────────────────────────────┤
│  Layer 2: User Preferences                   │
│  ~/.config/coder-desktop/settings.json       │
│  (QSettings with JSON format)                │
│  Editable via Settings UI.                   │
├─────────────────────────────────────────────┤
│  Layer 1: Defaults (lowest priority)         │
│  Compiled into the application.              │
└─────────────────────────────────────────────┘
```

### MDM Settings in the App UI

**Key behavior**: When MDM is NOT enabled (no `/etc/coder-desktop/policy.json`
or it's missing fields), ALL MDM-manageable settings are exposed in the
application's Settings page as user-editable preferences. This is critical
for development, testing, and non-enterprise users.

When MDM IS enabled, each setting can be:
- **Locked**: MDM value is enforced, UI shows the value but grayed out / read-only
- **Suggested**: MDM provides a default, user can override
- **Absent**: Falls through to user preference or app default

### Settings Structure

```cpp
// app/src/settings/SettingsManager.h
class SettingsManager : public QObject {
    Q_OBJECT

    // Each setting has: effective value, source (mdm/user/default), and isLocked
    Q_PROPERTY(QString deploymentUrl READ deploymentUrl NOTIFY settingsChanged)
    Q_PROPERTY(bool deploymentUrlLocked READ deploymentUrlLocked NOTIFY settingsChanged)

public:
    // All MDM-manageable settings (exposed in UI when MDM not active)
    struct AllSettings {
        // Deployment
        QString deploymentUrl;
        QStringList allowedDeployments;
        bool disableMultiDeployment = false;

        // VPN
        bool requireVpn = false;
        bool autoConnectVpn = false;

        // Security
        bool requireBiometric = false;

        // DLP (included in MDM; shown in Settings when MDM not active)
        bool dlpEnabled = false;
        bool dlpClipboardBlock = false;
        bool dlpScreenshotBlock = false;
        bool dlpFileSandbox = false;
        bool dlpNetworkSandbox = false;

        // File restrictions
        bool disableFileUpload = false;
        bool disableFileDownload = false;
    };

    // Query: where did this value come from?
    enum class Source { Default, User, Mdm };
    Source settingSource(const QString &key) const;
    bool isLocked(const QString &key) const;

    // Write (only works for user-editable settings)
    void setUserPreference(const QString &key, const QVariant &value);

private:
    MdmConfigManager *m_mdm;    // reads /etc/coder-desktop/policy.json
    QSettings *m_userPrefs;     // ~/.config/coder-desktop/settings.json
};
```

### Settings UI Behavior

```qml
// qml/pages/SettingsPage.qml
SettingsSection {
    title: "VPN"

    SettingsToggle {
        label: "Auto-connect VPN on launch"
        checked: settingsManager.autoConnectVpn
        locked: settingsManager.isLocked("autoConnectVpn")
        // When locked: shows lock icon, toggle is disabled, tooltip shows "Managed by your organization"
        // When unlocked: user can toggle freely
        onToggled: settingsManager.setUserPreference("autoConnectVpn", checked)
    }
}

SettingsSection {
    title: "Data Loss Prevention"
    description: settingsManager.mdmEnabled
        ? "These settings are managed by your organization."
        : "Configure DLP policies for workspace applications."

    SettingsToggle {
        label: "Enable DLP compositor"
        checked: settingsManager.dlpEnabled
        locked: settingsManager.isLocked("dlpEnabled")
        onToggled: settingsManager.setUserPreference("dlpEnabled", checked)
    }

    SettingsToggle {
        label: "Block clipboard export"
        checked: settingsManager.dlpClipboardBlock
        locked: settingsManager.isLocked("dlpClipboardBlock")
        enabled: settingsManager.dlpEnabled  // only meaningful if DLP is on
        onToggled: settingsManager.setUserPreference("dlpClipboardBlock", checked)
    }
    // ... screenshot block, file sandbox, network sandbox
}
```

### MDM Policy File Format

```json
// /etc/coder-desktop/policy.json
{
  "version": 1,
  "settings": {
    "deploymentUrl": { "value": "https://coder.example.com", "locked": true },
    "autoConnectVpn": { "value": true, "locked": false },
    "dlpEnabled": { "value": true, "locked": true },
    "dlpClipboardBlock": { "value": true, "locked": true },
    "dlpScreenshotBlock": { "value": true, "locked": true },
    "dlpFileSandbox": { "value": true, "locked": true },
    "dlpNetworkSandbox": { "value": true, "locked": true },
    "disableFileDownload": { "value": true, "locked": true }
  }
}
```

### User Preferences Storage

```json
// ~/.config/coder-desktop/settings.json
{
  "autoConnectVpn": true,
  "dlpEnabled": false,
  "theme": "system",
  "notificationsEnabled": true,
  "terminalFont": "monospace",
  "terminalFontSize": 14
}
```

### Credential Storage (separate from settings)

Tokens and secrets are NOT stored in settings files. They use the
platform secret service:

- **Primary**: libsecret (GNOME Keyring / KWallet via D-Bus Secret Service API)
- **Fallback**: Encrypted file in `~/.config/coder-desktop/credentials.enc`
  (for headless / no-desktop-environment scenarios)

---

## 3. Implementation Phases (Corrected)

The original Phase 1 incorrectly referenced "Speaker protocol implementation"
which was removed when we switched to the Go shared library approach.
Corrected phases:

### Phase 1: Foundation (Weeks 1–4)
- [ ] Project scaffolding (CMake, Go module, directory structure)
- [ ] AGENTS.md + initial docs
- [ ] Go VPN shared library (`coder-vpn-linux/bridge.go`) — C API + build script
- [ ] VPN shim layer (C++ — TUN creation, route management)
- [ ] DNS manager (resolvconf → resolvectl → direct file cascade)
- [ ] Basic system tray icon

### Phase 2: Core Features (Weeks 5–10)
- [ ] Coder API client (REST + auth)
- [ ] Session/deployment management with secure credential storage
- [ ] Workspace list + start/stop/delete/rebuild
- [ ] VPN connect/disconnect with peer display
- [ ] Settings system (layered: defaults → user prefs → MDM)
- [ ] Settings UI with MDM-aware locked/unlocked controls
- [ ] Desktop notifications

### Phase 3: Rich Features (Weeks 11–16)
- [ ] Terminal (xterm.js in WebEngine)
- [ ] Web views for workspace apps (VPN + proxy modes)
- [ ] AI Tasks (create, monitor, message, logs)
- [ ] Build logs streaming
- [ ] Multi-deployment UI
- [ ] Update detection for workspaces

### Phase 4: DLP (Weeks 17–24)
- [ ] wlroots nested compositor (`coder-dlp-compositor/`)
- [ ] Clipboard mediation
- [ ] Screenshot/screencopy blocking (protocol omission)
- [ ] Bubblewrap sandbox launcher (file + network namespace)
- [ ] DLP policy engine + MDM integration
- [ ] DLP settings in app UI (user-configurable when MDM not active)
- [ ] Security context protocol support

### Phase 5: Polish & Release (Weeks 25–30)
- [ ] Packaging (deb, rpm, flatpak, AppImage)
- [ ] Auto-update mechanism
- [ ] Comprehensive testing
- [ ] Performance optimization
- [ ] Documentation finalization
- [ ] CI/CD release pipeline
