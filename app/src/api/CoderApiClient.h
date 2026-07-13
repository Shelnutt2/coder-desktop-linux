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
    explicit CoderApiClient(QObject* parent = nullptr);

    // -- Properties ----------------------------------------------------------
    [[nodiscard]] QString baseUrl() const { return m_baseUrl; }
    void setBaseUrl(const QString& url);

    void setSessionToken(const QString& token);
    [[nodiscard]] bool isAuthenticated() const { return !m_sessionToken.isEmpty(); }

    // -- Auth ----------------------------------------------------------------
    [[nodiscard]] QNetworkReply* getAuthenticatedUser();  // GET /api/v2/users/me

    // -- Workspaces ----------------------------------------------------------
    [[nodiscard]] QNetworkReply* listWorkspaces(const QString& query = QString());
    [[nodiscard]] QNetworkReply* getWorkspace(const QString& id);
    [[nodiscard]] QNetworkReply* createWorkspaceBuild(const QString& workspaceId,
                                                      const QString& transition,
                                                      const QString& templateVersionId = QString());
    [[nodiscard]] QNetworkReply* deleteWorkspace(const QString& id);
    [[nodiscard]] QNetworkReply* favoriteWorkspace(const QString& id);
    [[nodiscard]] QNetworkReply* unfavoriteWorkspace(const QString& id);

    // -- Templates -----------------------------------------------------------
    [[nodiscard]] QNetworkReply* listTemplates();  // GET /api/v2/templates

    // -- Tasks ---------------------------------------------------------------
    [[nodiscard]] QNetworkReply* listTasks();  // GET /api/v2/tasks

    // -- Build info ----------------------------------------------------------
    [[nodiscard]] QNetworkReply* getBuildInfo();  // GET /api/v2/buildinfo

    // -- High-level fetch (QML-callable) ------------------------------------
    /// Fetch workspaces and emit workspacesReceived() with the JSON array.
    /// @param query  Optional server-side filter (e.g. "owner:me"), sent as `q=`.
    Q_INVOKABLE void fetchWorkspaces(const QString& query = QString());
    /// Fetch tasks and emit tasksReceived() with the JSON array.
    Q_INVOKABLE void fetchTasks();
    /// Fetch a single workspace and emit workspaceDetailReceived() with the JSON object.
    Q_INVOKABLE void fetchWorkspaceDetail(const QString& id);
    /// Start a workspace and emit workspaceActionCompleted() on success.
    Q_INVOKABLE void startWorkspace(const QString& id);
    /// Stop a workspace and emit workspaceActionCompleted() on success.
    Q_INVOKABLE void stopWorkspace(const QString& id);
    /// Update a workspace (currently an alias for start). TODO: pass templateActiveVersionId.
    Q_INVOKABLE void updateWorkspace(const QString& id);

signals:
    void baseUrlChanged();
    void authStateChanged();
    void requestFailed(const QString& endpoint, int statusCode, const QString& errorMessage);

    /// Emitted when fetchWorkspaces() completes successfully.
    void workspacesReceived(const QJsonArray& workspaces);
    /// Emitted when fetchTasks() completes successfully.
    void tasksReceived(const QJsonArray& tasks);
    /// Emitted when fetchWorkspaceDetail() completes successfully.
    void workspaceDetailReceived(const QJsonObject& workspace);
    /// Emitted when a workspace start/stop/update action completes successfully.
    void workspaceActionCompleted();

private:
    QNetworkAccessManager* m_nam = nullptr;  // Qt parent-owned (this)
    QString m_baseUrl;
    QString m_sessionToken;

    QNetworkRequest buildRequest(const QString& path) const;
    QNetworkReply* get(const QString& path);
    QNetworkReply* post(const QString& path, const QJsonObject& body = QJsonObject());
    QNetworkReply* put(const QString& path, const QJsonObject& body = QJsonObject());
    QNetworkReply* del(const QString& path);

    /// Connect the reply's finished signal to our error-emitting handler.
    void connectErrorHandler(QNetworkReply* reply, const QString& endpoint);
};

#endif  // CODERAPICLIENT_H
