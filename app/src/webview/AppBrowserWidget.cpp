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
                                       const QString &appUrl,
                                       const QString &appSlug,
                                       const QString &workspaceName,
                                       const QString &ownerName,
                                       const QString &agentName,
                                       bool vpnActive,
                                       bool isExternal) const
{
    qDebug() << "[AppBrowser] buildAppUrl: deploymentUrl=" << deploymentUrl
             << "appUrl=" << appUrl << "appSlug=" << appSlug
             << "workspace=" << workspaceName << "owner=" << ownerName
             << "agent=" << agentName << "vpn=" << vpnActive
             << "external=" << isExternal;

    // 1. External apps — use the API URL directly
    if (isExternal) {
        qDebug() << "[AppBrowser] External app, using URL directly:" << appUrl;
        return appUrl;
    }

    // 2. VPN mode — rewrite app URL hostname to tailnet FQDN
    if (vpnActive && !appUrl.isEmpty()) {
        QUrl parsed(appUrl);
        if (parsed.isValid()) {
            // Build tailnet hostname: {agentName}.{workspaceName}.me.coder
            const QString hostname = QStringLiteral("%1.%2.me.coder")
                .arg(agentName, workspaceName);
            parsed.setHost(hostname);
            const QString result = parsed.toString();
            qDebug() << "[AppBrowser] VPN mode, rewrote URL to:" << result;
            return result;
        }
    }

    // 3. Path-based proxy — {deploymentUrl}/@{owner}/{workspace}/apps/{slug}
    QUrl base(deploymentUrl);
    if (!base.isValid() || base.scheme().isEmpty()) {
        qWarning() << "[AppBrowser] Invalid deployment URL:" << deploymentUrl;
        return {};
    }
    const QString path = QStringLiteral("/@%1/%2/apps/%3")
        .arg(ownerName, workspaceName, appSlug);
    base.setPath(path);
    const QString result = base.toString();
    qDebug() << "[AppBrowser] Proxy mode, built URL:" << result;
    return result;
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
