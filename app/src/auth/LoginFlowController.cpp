#include "auth/LoginFlowController.h"
#include "data/SessionManager.h"

#include <QDesktopServices>
#include <QUrl>

#ifdef HAS_WEBENGINE
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#include <QNetworkCookie>
#endif

// Cookie name used by Coder deployments for session authentication.
static const QByteArray kSessionCookieName = "coder_session_token";

LoginFlowController::LoginFlowController(SessionManager &sessionManager,
                                         QObject *parent)
    : QObject(parent)
    , m_sessionManager(sessionManager)
{
#ifdef HAS_WEBENGINE
    // Off-the-record profile — cookies are not persisted to disk.
    m_profile = new QWebEngineProfile(this);
    m_cookieStore = m_profile->cookieStore();
    setupCookieMonitoring();
#endif
}

LoginFlowController::~LoginFlowController() = default;

bool LoginFlowController::isWebEngineAvailable() const
{
#ifdef HAS_WEBENGINE
    return true;
#else
    return false;
#endif
}

bool LoginFlowController::isFlowActive() const
{
    return m_flowActive;
}

QString LoginFlowController::loginUrl() const
{
    return m_loginUrl;
}

void LoginFlowController::startFlow(const QString &deploymentUrl)
{
    // Normalise: strip trailing slashes.
    QString base = deploymentUrl;
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);

    if (base.isEmpty())
        return;

    // Ensure the URL has a scheme.
    if (!base.startsWith(QLatin1String("http://")) &&
        !base.startsWith(QLatin1String("https://"))) {
        base.prepend(QLatin1String("https://"));
    }

    m_deploymentUrl = base;

#ifdef HAS_WEBENGINE
    // Clear any previous session cookies so the user gets a fresh login.
    clearCookies();

    // Point the embedded browser at the login page.
    m_loginUrl = base + QStringLiteral("/login");
    m_flowActive = true;
    emit loginUrlChanged();
    emit flowActiveChanged();
#else
    // No WebEngine — open external browser to /cli-auth.
    openExternalCliAuth(base);
#endif
}

void LoginFlowController::cancelFlow()
{
    m_flowActive = false;
    m_loginUrl.clear();
    m_deploymentUrl.clear();

#ifdef HAS_WEBENGINE
    clearCookies();
#endif

    emit loginUrlChanged();
    emit flowActiveChanged();
}

void LoginFlowController::openExternalCliAuth(const QString &deploymentUrl)
{
    QString base = deploymentUrl;
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);

    if (!base.startsWith(QLatin1String("http://")) &&
        !base.startsWith(QLatin1String("https://"))) {
        base.prepend(QLatin1String("https://"));
    }

    const QString cliAuthUrl = base + QStringLiteral("/cli-auth");
    QDesktopServices::openUrl(QUrl(cliAuthUrl));
    emit externalBrowserOpened(cliAuthUrl);
}

#ifdef HAS_WEBENGINE
void LoginFlowController::setupCookieMonitoring()
{
    connect(m_cookieStore, &QWebEngineCookieStore::cookieAdded,
            this, [this](const QNetworkCookie &cookie) {
        if (!m_flowActive)
            return;

        if (cookie.name() != kSessionCookieName)
            return;

        const QString token = QString::fromUtf8(cookie.value());
        if (token.isEmpty())
            return;

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

void LoginFlowController::clearCookies()
{
    if (m_cookieStore)
        m_cookieStore->deleteAllCookies();
}
#endif
