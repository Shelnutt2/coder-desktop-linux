#include "auth/LoginFlowController.h"
#include "data/SessionManager.h"

#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QUrl>

#ifdef HAS_WEBENGINE
#include <QNetworkCookie>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#endif

// Cookie name used by Coder deployments for session authentication.
static const QByteArray kSessionCookieName = "coder_session_token";

// Unauthenticated endpoint used to probe whether a Coder deployment is
// reachable at a given scheme.  A 200 response (or even a non-network error
// HTTP status) tells us the server is there.
static const QString kProbeEndpoint = QStringLiteral("/api/v2/buildinfo");

// Probe timeout in milliseconds.
static constexpr int kProbeTimeoutMs = 10000;

LoginFlowController::LoginFlowController(SessionManager& sessionManager, QObject* parent)
    : QObject(parent), m_sessionManager(sessionManager), m_nam(new QNetworkAccessManager(this)) {
#ifdef HAS_WEBENGINE
    // Off-the-record profile — cookies are not persisted to disk.
    m_profile = new QWebEngineProfile(this);
    m_cookieStore = m_profile->cookieStore();
    setupCookieMonitoring();
#endif
}

LoginFlowController::~LoginFlowController() = default;

bool LoginFlowController::isWebEngineAvailable() const {
#ifdef HAS_WEBENGINE
    return true;
#else
    return false;
#endif
}

bool LoginFlowController::isFlowActive() const {
    return m_flowActive;
}

bool LoginFlowController::isProbing() const {
    return m_probing;
}

QString LoginFlowController::loginUrl() const {
    return m_loginUrl;
}

void LoginFlowController::startFlow(const QString& deploymentUrl) {
    // Normalise: strip trailing slashes.
    QString base = deploymentUrl;
    while (base.endsWith(QLatin1Char('/'))) base.chop(1);

    if (base.isEmpty()) return;

    // If the URL already has a scheme, proceed directly.
    if (base.startsWith(QLatin1String("http://")) || base.startsWith(QLatin1String("https://"))) {
        continueStartFlow(base);
        return;
    }

    // No scheme — probe https first, then fall back to http.
    probeScheme(base);
}

void LoginFlowController::probeScheme(const QString& hostWithoutScheme) {
    m_pendingHost = hostWithoutScheme;
    m_probing = true;
    emit probingChanged();

    const QUrl httpsUrl(QStringLiteral("https://") + hostWithoutScheme + kProbeEndpoint);
    QNetworkRequest request(httpsUrl);
    request.setTransferTimeout(kProbeTimeoutMs);

    QNetworkReply* reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        // If we're no longer probing (e.g. flow was cancelled), ignore.
        if (!m_probing) return;

        if (reply->error() == QNetworkReply::NoError ||
            reply->error() == QNetworkReply::AuthenticationRequiredError ||
            reply->error() == QNetworkReply::ContentAccessDenied ||
            reply->error() == QNetworkReply::ContentNotFoundError ||
            reply->error() == QNetworkReply::InternalServerError) {
            // Got an HTTP-level response — https works.
            qInfo() << "LoginFlowController: https probe succeeded for" << m_pendingHost;
            m_probing = false;
            emit probingChanged();
            continueStartFlow(QStringLiteral("https://") + m_pendingHost);
            return;
        }

        // HTTPS failed (connection refused, SSL error, timeout, etc.) — try HTTP.
        qInfo() << "LoginFlowController: https probe failed (" << reply->errorString()
                << "), trying http for" << m_pendingHost;

        const QUrl httpUrl(QStringLiteral("http://") + m_pendingHost + kProbeEndpoint);
        QNetworkRequest httpReq(httpUrl);
        httpReq.setTransferTimeout(kProbeTimeoutMs);

        QNetworkReply* httpReply = m_nam->get(httpReq);
        connect(httpReply, &QNetworkReply::finished, this, [this, httpReply]() {
            httpReply->deleteLater();

            if (!m_probing) return;

            m_probing = false;
            emit probingChanged();

            if (httpReply->error() == QNetworkReply::NoError ||
                httpReply->error() == QNetworkReply::AuthenticationRequiredError ||
                httpReply->error() == QNetworkReply::ContentAccessDenied ||
                httpReply->error() == QNetworkReply::ContentNotFoundError ||
                httpReply->error() == QNetworkReply::InternalServerError) {
                // HTTP works.
                qInfo() << "LoginFlowController: http probe succeeded for" << m_pendingHost;
                continueStartFlow(QStringLiteral("http://") + m_pendingHost);
            } else {
                // Neither scheme works — default to https and let the user
                // see the actual connection error during login.
                qWarning() << "LoginFlowController: both probes failed for" << m_pendingHost
                           << "— defaulting to https";
                emit probeFailed(
                    QStringLiteral("Could not reach server. Check the URL and try again."));
            }
        });
    });
}

void LoginFlowController::continueStartFlow(const QString& resolvedBaseUrl) {
    m_deploymentUrl = resolvedBaseUrl;

#ifdef HAS_WEBENGINE
    // Clear any previous session cookies so the user gets a fresh login.
    clearCookies();

    // Point the embedded browser at the login page.
    m_loginUrl = resolvedBaseUrl + QStringLiteral("/login");
    m_flowActive = true;
    emit loginUrlChanged();
    emit flowActiveChanged();
#else
    // No WebEngine — open external browser to /cli-auth.
    openExternalCliAuth(resolvedBaseUrl);
#endif
}

void LoginFlowController::cancelFlow() {
    m_flowActive = false;
    m_loginUrl.clear();
    m_deploymentUrl.clear();

    // Cancel any in-progress scheme probe.
    if (m_probing) {
        m_probing = false;
        m_pendingHost.clear();
        emit probingChanged();
    }

#ifdef HAS_WEBENGINE
    clearCookies();
#endif

    emit loginUrlChanged();
    emit flowActiveChanged();
}

void LoginFlowController::openExternalCliAuth(const QString& deploymentUrl) {
    QString base = deploymentUrl;
    while (base.endsWith(QLatin1Char('/'))) base.chop(1);

    if (!base.startsWith(QLatin1String("http://")) && !base.startsWith(QLatin1String("https://"))) {
        base.prepend(QLatin1String("https://"));
    }

    const QString cliAuthUrl = base + QStringLiteral("/cli-auth");
    QDesktopServices::openUrl(QUrl(cliAuthUrl));
    emit externalBrowserOpened(cliAuthUrl);
}

void LoginFlowController::handleJsTokenResult(const QString& token) {
    if (!m_flowActive) return;
    if (token.isEmpty()) return;

    qInfo() << "LoginFlowController: captured session token via JavaScript API key generation";

    m_sessionManager.login(m_deploymentUrl, token);

    m_flowActive = false;
    m_loginUrl.clear();
    emit loginUrlChanged();
    emit flowActiveChanged();
    emit tokenObtained(m_deploymentUrl, token);

#ifdef HAS_WEBENGINE
    clearCookies();
#endif
}

#ifdef HAS_WEBENGINE
void LoginFlowController::setupCookieMonitoring() {
    connect(m_cookieStore, &QWebEngineCookieStore::cookieAdded, this,
            [this](const QNetworkCookie& cookie) {
                if (!m_flowActive) return;

                if (cookie.name() != kSessionCookieName) return;

                const QString token = QString::fromUtf8(cookie.value());
                if (token.isEmpty()) return;

                qInfo() << "LoginFlowController: captured session token from browser cookie";

                // Auto-login with the captured token.
                m_sessionManager.login(m_deploymentUrl, token);

                // End the flow.
                m_flowActive = false;
                m_loginUrl.clear();
                emit loginUrlChanged();
                emit flowActiveChanged();
                emit tokenObtained(m_deploymentUrl, token);

                // Clear cookies now that we have the token.
                clearCookies();
            });
}

void LoginFlowController::clearCookies() {
    if (m_cookieStore) m_cookieStore->deleteAllCookies();
}
#endif
