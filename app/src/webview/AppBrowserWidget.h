#ifndef APPBROWSERWIDGET_H
#define APPBROWSERWIDGET_H

#include <QObject>
#include <QString>
#include <QUrl>

/// C++ backend for the workspace app browser.
///
/// Constructs the correct URL based on VPN mode vs proxy mode,
/// and provides helpers for authentication cookie injection.
///
/// VPN mode URLs:   https://{appSlug}--{agentName}--{workspaceName}.coder
/// Proxy mode URLs: {deploymentUrl}/api/v2/workspaceagents/{agentId}/proxy/
class AppBrowserWidget : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentUrl READ currentUrl NOTIFY urlChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)

public:
    explicit AppBrowserWidget(QObject *parent = nullptr);
    ~AppBrowserWidget() override = default;

    AppBrowserWidget(const AppBrowserWidget &) = delete;
    AppBrowserWidget &operator=(const AppBrowserWidget &) = delete;

    /// Build URL for a workspace app.
    ///
    /// @param deploymentUrl  Base URL of the Coder deployment (e.g. "https://coder.example.com")
    /// @param agentId        UUID of the workspace agent
    /// @param appSlug        Application slug identifier
    /// @param workspaceName  Name of the workspace
    /// @param agentName      Name of the agent within the workspace
    /// @param vpnActive      Whether VPN tunnel is currently connected
    /// @return Fully-qualified URL string for the app
    [[nodiscard]] Q_INVOKABLE QString buildAppUrl(const QString &deploymentUrl,
                                                   const QString &agentId,
                                                   const QString &appSlug,
                                                   const QString &workspaceName,
                                                   const QString &agentName,
                                                   bool vpnActive) const;

    /// Build a session cookie value for authenticating WebEngine requests.
    ///
    /// @param token  API session token
    /// @return Cookie value string suitable for the "coder_session_token" cookie
    [[nodiscard]] Q_INVOKABLE QString buildSessionCookie(const QString &token) const;

    [[nodiscard]] QString currentUrl() const;
    [[nodiscard]] bool isLoading() const;

public slots:
    void setCurrentUrl(const QString &url);
    void setLoading(bool loading);

signals:
    void urlChanged();
    void loadingChanged();
    void titleChanged(const QString &title);
    void navigationRequested(const QUrl &url);

private:
    QString m_currentUrl;
    bool m_loading = false;
};

#endif // APPBROWSERWIDGET_H
