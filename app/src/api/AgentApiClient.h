#ifndef AGENTAPICLIENT_H
#define AGENTAPICLIENT_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QStringList>

#include "api/dto/AgentDirectory.h"

/// Asynchronous HTTP client for the Coder workspace agent's local API.
///
/// Each Coder workspace agent exposes a lightweight HTTP API on port 4,
/// accessible over the VPN tunnel.  This client talks to that API to
/// browse the remote filesystem (list-directory).
///
/// Usage:
///   1. Call listDirectory() with the agent's VPN hostname and a path.
///   2. Connect to directoryListed() for success or listDirectoryError()
///      for failures.
class AgentApiClient : public QObject {
    Q_OBJECT

public:
    explicit AgentApiClient(QObject* parent = nullptr);

    /// Construct with an externally-owned QNetworkAccessManager.
    /// @param nam  Non-owning; the caller must keep @p nam alive.
    explicit AgentApiClient(QNetworkAccessManager* nam, QObject* parent = nullptr);

    /// List the contents of a directory on the remote agent.
    ///
    /// @param agentHostname  The agent's VPN hostname (e.g. "myworkspace.coder").
    /// @param path           Path segments (e.g. {"home", "coder", "repos"}).
    /// @param relativity     Either "root" (relative to /) or "home" (relative to ~/).
    ///
    /// On success, emits directoryListed().  On failure, emits listDirectoryError().
    Q_INVOKABLE void listDirectory(const QString& agentHostname, const QStringList& path,
                                   const QString& relativity = QStringLiteral("home"));

signals:
    /// Emitted when a list-directory request completes successfully.
    void directoryListed(const QString& agentHostname, const DirectoryListing& listing);

    /// Emitted when a list-directory request fails (network error, non-200, bad JSON).
    void listDirectoryError(const QString& agentHostname, const QString& errorMessage);

private:
    static constexpr int kAgentApiPort = 4;
    static constexpr auto kListDirectoryPath = "/api/v0/list-directory";

    QNetworkAccessManager* m_nam = nullptr;  // owned (via Qt parent) or externally injected
    bool m_ownsNam = false;

    [[nodiscard]] static QUrl buildAgentUrl(const QString& agentHostname, const QString& path);
};

#endif  // AGENTAPICLIENT_H
