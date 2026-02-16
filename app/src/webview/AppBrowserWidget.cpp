#include "AppBrowserWidget.h"

#include <QUrl>
#include <QUrlQuery>

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
