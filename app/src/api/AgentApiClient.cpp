#include "api/AgentApiClient.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AgentApiClient::AgentApiClient(QObject* parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)), m_ownsNam(true) {}

AgentApiClient::AgentApiClient(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent), m_nam(nam), m_ownsNam(false) {}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void AgentApiClient::listDirectory(const QString& agentHostname, const QStringList& path,
                                   const QString& relativity) {
    // Build the JSON request body.
    QJsonObject body;
    body.insert(QLatin1String("relativity"), relativity);

    QJsonArray pathArray;
    for (const auto& segment : path) {
        pathArray.append(segment);
    }
    body.insert(QLatin1String("path"), pathArray);

    // Build the HTTP request.
    const QUrl url = buildAgentUrl(agentHostname, QLatin1String(kListDirectoryPath));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply* reply = m_nam->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    // Capture agentHostname by value for the lambda.
    connect(reply, &QNetworkReply::finished, this, [this, reply, agentHostname]() {
        reply->deleteLater();

        // --- Network-level error ---
        if (reply->error() != QNetworkReply::NoError) {
            const QString msg = QStringLiteral("Network error listing directory on %1: %2")
                                    .arg(agentHostname, reply->errorString());
            qWarning() << "AgentApiClient:" << msg;
            emit listDirectoryError(agentHostname, msg);
            return;
        }

        // --- HTTP status check ---
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status < 200 || status >= 300) {
            const QString body = QString::fromUtf8(reply->readAll());
            const QString msg = QStringLiteral("Agent API on %1 returned HTTP %2: %3")
                                    .arg(agentHostname)
                                    .arg(status)
                                    .arg(body.left(512));
            qWarning() << "AgentApiClient:" << msg;
            emit listDirectoryError(agentHostname, msg);
            return;
        }

        // --- Parse JSON response ---
        const QByteArray data = reply->readAll();
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            const QString msg =
                QStringLiteral("Failed to parse list-directory response from %1: %2")
                    .arg(agentHostname, parseError.errorString());
            qWarning() << "AgentApiClient:" << msg;
            emit listDirectoryError(agentHostname, msg);
            return;
        }

        if (!doc.isObject()) {
            const QString msg =
                QStringLiteral("Unexpected JSON type in list-directory response from %1")
                    .arg(agentHostname);
            qWarning() << "AgentApiClient:" << msg;
            emit listDirectoryError(agentHostname, msg);
            return;
        }

        const DirectoryListing listing = DirectoryListing::fromJson(doc.object());
        emit directoryListed(agentHostname, listing);
    });
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

QUrl AgentApiClient::buildAgentUrl(const QString& agentHostname, const QString& path) {
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(agentHostname);
    url.setPort(kAgentApiPort);
    url.setPath(path);
    return url;
}
