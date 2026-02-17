#ifndef APPBROWSERWIDGET_H
#define APPBROWSERWIDGET_H

#include <QObject>
#include <QString>
#include <QUrl>

/// C++ backend for the workspace app browser.
///
/// Constructs the correct URL based on app type, VPN mode, and proxy mode,
/// and provides helpers for authentication cookie injection.
///
/// URL construction follows the same logic as the Android app (AppLauncher.kt):
///   1. External apps: return appUrl directly
///   2. VPN mode + appUrl present: rewrite appUrl hostname to {agent}.{workspace}.me.coder
///   3. Non-VPN: {deploymentUrl}/@{ownerName}/{workspaceName}/apps/{appSlug}
class AppBrowserWidget : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentUrl READ currentUrl NOTIFY urlChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)

public:
    explicit AppBrowserWidget(QObject* parent = nullptr);
    ~AppBrowserWidget() override = default;

    AppBrowserWidget(const AppBrowserWidget&) = delete;
    AppBrowserWidget& operator=(const AppBrowserWidget&) = delete;

    /// Build URL for a workspace app.
    ///
    /// URL construction follows the same logic as the Android app:
    ///   1. External apps: return appUrl directly
    ///   2. VPN mode + appUrl present: rewrite appUrl hostname to {agent}.{workspace}.me.coder
    ///   3. Non-VPN: {deploymentUrl}/@{ownerName}/{workspaceName}/apps/{appSlug}
    [[nodiscard]] Q_INVOKABLE QString buildAppUrl(const QString& deploymentUrl,
                                                  const QString& appUrl, const QString& appSlug,
                                                  const QString& workspaceName,
                                                  const QString& ownerName,
                                                  const QString& agentName, bool vpnActive,
                                                  bool isExternal) const;

    /// Build a session cookie value for authenticating WebEngine requests.
    ///
    /// @param token  API session token
    /// @return Cookie value string suitable for the "coder_session_token" cookie
    [[nodiscard]] Q_INVOKABLE QString buildSessionCookie(const QString& token) const;

    /// Inject the Coder session token as a cookie into the default WebEngine profile.
    /// Must be called before or right after the WebEngineView loads.
    /// @param deploymentUrl  The Coder deployment base URL (used as cookie domain)
    /// @param token  The session token value
    Q_INVOKABLE void injectSessionCookie(const QString& deploymentUrl, const QString& token);

    [[nodiscard]] QString currentUrl() const;
    [[nodiscard]] bool isLoading() const;

public slots:
    void setCurrentUrl(const QString& url);
    void setLoading(bool loading);

signals:
    void urlChanged();
    void loadingChanged();
    void titleChanged(const QString& title);
    void navigationRequested(const QUrl& url);
    void cookieReady();  // Emitted after session cookie is confirmed set

private:
    QString m_currentUrl;
    bool m_loading = false;
};

#endif  // APPBROWSERWIDGET_H
