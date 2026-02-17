#include "updater/AutoUpdater.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

#include "settings/SettingsManager.h"

// GitHub Releases API endpoint for this project.
static constexpr auto kReleasesUrl =
    "https://api.github.com/repos/coder/coder-desktop-linux/releases/latest";

// Default periodic check interval (24 hours).
static constexpr auto kDefaultInterval = std::chrono::hours{24};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AutoUpdater::AutoUpdater(const QString& currentVersion, SettingsManager* settingsManager,
                         QObject* parent)
    : QObject(parent),
      m_currentVersion(currentVersion),
      m_settings(settingsManager),
      m_nam(std::make_unique<QNetworkAccessManager>()) {
    connect(m_nam.get(), &QNetworkAccessManager::finished, this, &AutoUpdater::onReplyFinished);

    // Periodic timer (default: 24 h).
    m_timer.setInterval(kDefaultInterval);
    connect(&m_timer, &QTimer::timeout, this, &AutoUpdater::checkNow);

    // Fire the first check shortly after event-loop start.
    QTimer::singleShot(std::chrono::seconds{5}, this, &AutoUpdater::checkNow);
}

// ---------------------------------------------------------------------------
// Property getters
// ---------------------------------------------------------------------------

QString AutoUpdater::latestVersion() const {
    return m_latestVersion;
}
QString AutoUpdater::downloadUrl() const {
    return m_downloadUrl;
}
bool AutoUpdater::updateReady() const {
    return m_updateReady;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void AutoUpdater::checkNow() {
    // Respect the user / MDM setting.
    if (m_settings && !m_settings->checkForUpdates()) {
        qDebug() << "AutoUpdater: checkForUpdates is disabled — skipping.";
        emit checkFinished();
        return;
    }

    QNetworkRequest request{QUrl{QString::fromLatin1(kReleasesUrl)}};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("CoderDesktop/%1").arg(m_currentVersion));
    // Accept the GitHub v3 JSON media type.
    request.setRawHeader("Accept", "application/vnd.github.v3+json");

    m_nam->get(request);
}

void AutoUpdater::setCheckInterval(std::chrono::milliseconds interval) {
    if (interval.count() <= 0) {
        m_timer.stop();
    } else {
        m_timer.setInterval(interval);
        if (!m_timer.isActive()) m_timer.start();
    }
}

// ---------------------------------------------------------------------------
// Network reply handler
// ---------------------------------------------------------------------------

void AutoUpdater::onReplyFinished(QNetworkReply* reply) {
    // QNetworkReply is owned by QNetworkAccessManager; ensure cleanup.
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "AutoUpdater: network error:" << reply->errorString();
        emit checkFinished();
        return;
    }

    const QByteArray body = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        qWarning() << "AutoUpdater: unexpected JSON response.";
        emit checkFinished();
        return;
    }

    const QJsonObject root = doc.object();
    const QString tagName = root.value(QLatin1String("tag_name")).toString();

    if (tagName.isEmpty()) {
        qWarning() << "AutoUpdater: no tag_name in response.";
        emit checkFinished();
        return;
    }

    // Extract the first asset download URL (if any).
    QString assetUrl;
    const QJsonArray assets = root.value(QLatin1String("assets")).toArray();
    if (!assets.isEmpty()) {
        const QJsonObject firstAsset = assets.first().toObject();
        assetUrl = firstAsset.value(QLatin1String("browser_download_url")).toString();
    }

    // Fall back to the release HTML page if no asset is available.
    if (assetUrl.isEmpty()) assetUrl = root.value(QLatin1String("html_url")).toString();

    if (isNewerVersion(m_currentVersion, tagName)) {
        m_latestVersion = tagName;
        m_downloadUrl = assetUrl;
        m_updateReady = true;
        qInfo() << "AutoUpdater: new version available:" << tagName
                << "(current:" << m_currentVersion << ")";
        emit updateAvailable(m_latestVersion, m_downloadUrl);
    } else {
        qDebug() << "AutoUpdater: up to date (remote:" << tagName << ", local:" << m_currentVersion
                 << ").";
        emit checkFinished();
    }
}

// ---------------------------------------------------------------------------
// Semver comparison
// ---------------------------------------------------------------------------

bool AutoUpdater::parseSemver(const QString& tag, int& major, int& minor, int& patch) {
    // Accept "v1.2.3" or "1.2.3".
    static const QRegularExpression re(QStringLiteral(R"(^v?(\d+)\.(\d+)\.(\d+))"));
    const auto match = re.match(tag);
    if (!match.hasMatch()) return false;

    major = match.captured(1).toInt();
    minor = match.captured(2).toInt();
    patch = match.captured(3).toInt();
    return true;
}

bool AutoUpdater::isNewerVersion(const QString& local, const QString& remote) {
    int lMaj = 0, lMin = 0, lPat = 0;
    int rMaj = 0, rMin = 0, rPat = 0;

    if (!parseSemver(local, lMaj, lMin, lPat) || !parseSemver(remote, rMaj, rMin, rPat)) {
        // If we can't parse either version, fall back to string comparison.
        return remote > local;
    }

    if (rMaj != lMaj) return rMaj > lMaj;
    if (rMin != lMin) return rMin > lMin;
    return rPat > lPat;
}
