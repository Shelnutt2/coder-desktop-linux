#ifndef CODERAPICLIENT_H
#define CODERAPICLIENT_H

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>

/// Asynchronous REST client for the Coder deployment API.
///
/// All request methods return a QNetworkReply* that the caller can connect
/// to for fine-grained control.  The client also emits requestFailed() for
/// any reply that finishes with a non-2xx status.
///
/// Authentication uses the `Coder-Session-Token` header (NOT Bearer).
class CoderApiClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(bool authenticated READ isAuthenticated NOTIFY authStateChanged)

public:
    explicit CoderApiClient(QObject *parent = nullptr);

    // -- Properties ----------------------------------------------------------
    [[nodiscard]] QString baseUrl() const { return m_baseUrl; }
    void setBaseUrl(const QString &url);

    void setSessionToken(const QString &token);
    [[nodiscard]] bool isAuthenticated() const { return !m_sessionToken.isEmpty(); }

    // -- Auth ----------------------------------------------------------------
    [[nodiscard]] QNetworkReply *getAuthenticatedUser();           // GET /api/v2/users/me

    // -- Workspaces ----------------------------------------------------------
    [[nodiscard]] QNetworkReply *listWorkspaces(const QString &query = QString());
    [[nodiscard]] QNetworkReply *getWorkspace(const QString &id);
    [[nodiscard]] QNetworkReply *createWorkspaceBuild(
        const QString &workspaceId,
        const QString &transition,
        const QString &templateVersionId = QString());
    [[nodiscard]] QNetworkReply *deleteWorkspace(const QString &id);
    [[nodiscard]] QNetworkReply *favoriteWorkspace(const QString &id);
    [[nodiscard]] QNetworkReply *unfavoriteWorkspace(const QString &id);

    // -- Templates -----------------------------------------------------------
    [[nodiscard]] QNetworkReply *listTemplates();                  // GET /api/v2/templates

    // -- Tasks ---------------------------------------------------------------
    [[nodiscard]] QNetworkReply *listTasks();                      // GET /api/v2/tasks

    // -- Build info ----------------------------------------------------------
    [[nodiscard]] QNetworkReply *getBuildInfo();                    // GET /api/v2/buildinfo

    // -- High-level fetch (QML-callable) ------------------------------------
    /// Fetch workspaces and emit workspacesReceived() with the JSON array.
    Q_INVOKABLE void fetchWorkspaces();
    /// Fetch tasks and emit tasksReceived() with the JSON array.
    Q_INVOKABLE void fetchTasks();

signals:
    void baseUrlChanged();
    void authStateChanged();
    void requestFailed(const QString &endpoint, int statusCode,
                       const QString &errorMessage);

    /// Emitted when fetchWorkspaces() completes successfully.
    void workspacesReceived(const QJsonArray &workspaces);
    /// Emitted when fetchTasks() completes successfully.
    void tasksReceived(const QJsonArray &tasks);

private:
    QNetworkAccessManager *m_nam = nullptr;  // Qt parent-owned (this)
    QString m_baseUrl;
    QString m_sessionToken;

    QNetworkRequest buildRequest(const QString &path) const;
    QNetworkReply *get(const QString &path);
    QNetworkReply *post(const QString &path,
                        const QJsonObject &body = QJsonObject());
    QNetworkReply *put(const QString &path,
                       const QJsonObject &body = QJsonObject());
    QNetworkReply *del(const QString &path);

    /// Connect the reply's finished signal to our error-emitting handler.
    void connectErrorHandler(QNetworkReply *reply, const QString &endpoint);
};

#endif // CODERAPICLIENT_H
