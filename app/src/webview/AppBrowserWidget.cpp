#include "AppBrowserWidget.h"

#include <QDebug>
#include <QNetworkCookie>
#include <QUrl>
#include <QUrlQuery>

#ifdef HAS_WEBENGINE
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>
#endif

AppBrowserWidget::AppBrowserWidget(QObject *parent)
    : QObject(parent)
{
}

QString AppBrowserWidget::buildAppUrl(const QString &deploymentUrl,
                                       const QString &agentId,
                                       const QString &appSlug,
                                       const QString &workspaceName,
                                       const QString &agentName,
                                       bool vpnActive) const
{
    if (vpnActive) {
        // VPN mode: use the .coder hostname directly.
        // Format: https://{appSlug}--{agentName}--{workspaceName}.coder
        return QStringLiteral("https://%1--%2--%3.coder")
            .arg(appSlug, agentName, workspaceName);
    }

    // Proxy mode: route through the deployment's API proxy endpoint.
    // Format: {deploymentUrl}/api/v2/workspaceagents/{agentId}/proxy/
    QUrl base(deploymentUrl);
    if (!base.isValid() || base.scheme().isEmpty()) {
        return {};
    }

    // Ensure the path ends with a trailing slash for correct relative resolution.
    const QString path = QStringLiteral("/api/v2/workspaceagents/%1/proxy/").arg(agentId);
    base.setPath(path);

    return base.toString();
}

QString AppBrowserWidget::buildSessionCookie(const QString &token) const
{
    // The Coder API uses "coder_session_token" as the cookie name.
    // Return the cookie value directly — the caller sets the cookie name.
    return token;
}

QString AppBrowserWidget::currentUrl() const
{
    return m_currentUrl;
}

bool AppBrowserWidget::isLoading() const
{
    return m_loading;
}

void AppBrowserWidget::setCurrentUrl(const QString &url)
{
    if (m_currentUrl != url) {
        m_currentUrl = url;
        emit urlChanged();
    }
}

void AppBrowserWidget::setLoading(bool loading)
{
    if (m_loading != loading) {
        m_loading = loading;
        emit loadingChanged();
    }
}

void AppBrowserWidget::injectSessionCookie(const QString &deploymentUrl, const QString &token)
{
    qDebug() << "AppBrowserWidget::injectSessionCookie: deploymentUrl=" << deploymentUrl;
    if (token.isEmpty() || deploymentUrl.isEmpty()) {
        qWarning() << "AppBrowserWidget::injectSessionCookie: missing token or URL";
        return;
    }

#ifdef HAS_WEBENGINE
    QUrl url(deploymentUrl);
    if (!url.isValid()) {
        qWarning() << "AppBrowserWidget::injectSessionCookie: invalid URL:" << deploymentUrl;
        return;
    }

    QNetworkCookie cookie;
    cookie.setName("coder_session_token");
    cookie.setValue(token.toUtf8());
    cookie.setDomain(url.host());
    cookie.setPath("/");
    cookie.setSecure(url.scheme() == "https");
    cookie.setHttpOnly(true);

    auto *profile = QWebEngineProfile::defaultProfile();
    profile->cookieStore()->setCookie(cookie, url);
    qDebug() << "AppBrowserWidget::injectSessionCookie: cookie set for domain" << url.host();
#else
    qWarning() << "AppBrowserWidget::injectSessionCookie: WebEngine not available, skipping";
#endif
}
