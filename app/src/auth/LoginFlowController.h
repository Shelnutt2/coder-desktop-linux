#ifndef LOGINFLOWCONTROLLER_H
#define LOGINFLOWCONTROLLER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

#ifdef HAS_WEBENGINE
class QWebEngineCookieStore;
class QWebEngineProfile;
#endif

class SessionManager;

/// Manages the browser-based login flow for Coder deployments.
///
/// When Qt WebEngine is available, the controller creates an off-the-record
/// browser profile and monitors cookies.  When the Coder deployment sets
/// the `coder_session_token` cookie after successful authentication, the
/// controller captures it and emits tokenObtained().
///
/// When WebEngine is not available, the controller opens an external browser
/// to the deployment's /cli-auth page and the user must paste the token
/// manually.
class LoginFlowController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool webEngineAvailable READ isWebEngineAvailable CONSTANT)
    Q_PROPERTY(bool flowActive READ isFlowActive NOTIFY flowActiveChanged)
    Q_PROPERTY(bool probing READ isProbing NOTIFY probingChanged)
    Q_PROPERTY(QString loginUrl READ loginUrl NOTIFY loginUrlChanged)

public:
    explicit LoginFlowController(SessionManager &sessionManager,
                                 QObject *parent = nullptr);
    ~LoginFlowController() override;

    LoginFlowController(const LoginFlowController &) = delete;
    LoginFlowController &operator=(const LoginFlowController &) = delete;

    [[nodiscard]] bool isWebEngineAvailable() const;
    [[nodiscard]] bool isFlowActive() const;
    [[nodiscard]] bool isProbing() const;
    [[nodiscard]] QString loginUrl() const;

    /// Start a browser-based login flow for the given deployment URL.
    /// If WebEngine is available, emits loginUrlChanged so QML can show the
    /// embedded browser.  Otherwise opens an external browser to /cli-auth.
    Q_INVOKABLE void startFlow(const QString &deploymentUrl);

    /// Cancel an in-progress browser login flow.
    Q_INVOKABLE void cancelFlow();

    /// Open the deployment's /cli-auth page in an external browser.
    Q_INVOKABLE void openExternalCliAuth(const QString &deploymentUrl);

#ifdef HAS_WEBENGINE
    /// Returns the off-the-record WebEngine profile for QML to use.
    /// Null when WebEngine is unavailable or flow is not active.
    [[nodiscard]] QWebEngineProfile *webEngineProfile() const { return m_profile; }
#endif

signals:
    /// Emitted when a session token is captured from the browser cookie.
    void tokenObtained(const QString &deploymentUrl, const QString &token);

    /// Emitted when the login URL changes (flow started/cancelled).
    void loginUrlChanged();

    /// Emitted when the flow active state changes.
    void flowActiveChanged();

    /// Emitted when scheme probing starts or finishes.
    void probingChanged();

    /// Emitted if we can't use WebEngine and opened an external browser instead.
    void externalBrowserOpened(const QString &cliAuthUrl);

    /// Emitted when scheme probing fails for both https and http.
    void probeFailed(const QString &errorMessage);

private:
    /// Probe /api/v2/buildinfo to determine the correct scheme (https vs http).
    /// Calls continueStartFlow() when done.
    void probeScheme(const QString &hostWithoutScheme);

    /// Continue the login flow after the scheme has been resolved.
    void continueStartFlow(const QString &resolvedBaseUrl);

    SessionManager &m_sessionManager;  // non-owning reference
    QNetworkAccessManager *m_nam = nullptr;  // owned by this (parent)
    QString m_deploymentUrl;
    QString m_loginUrl;
    QString m_pendingHost;  // host being probed (no scheme)
    bool m_flowActive = false;
    bool m_probing = false;

#ifdef HAS_WEBENGINE
    QWebEngineProfile *m_profile = nullptr;   // owned by this
    QWebEngineCookieStore *m_cookieStore = nullptr;  // non-owning (owned by profile)

    void setupCookieMonitoring();
    void clearCookies();
#endif
};

#endif // LOGINFLOWCONTROLLER_H
