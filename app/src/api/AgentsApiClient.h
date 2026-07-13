#ifndef AGENTSAPICLIENT_H
#define AGENTSAPICLIENT_H

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "api/dto/Chat.h"
#include "api/dto/ChatMessage.h"
#include "api/dto/ChatModels.h"

/// Asynchronous REST client for the experimental Coder Agents chat API
/// (/api/experimental/chats, /api/experimental/mcp).
///
/// Follows the CoderApiClient conventions: authentication uses the
/// `Coder-Session-Token` header, methods issue requests and emit typed
/// result signals, and requestFailed() fires for any non-2xx reply.
class AgentsApiClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)

public:
    explicit AgentsApiClient(QObject* parent = nullptr);

    [[nodiscard]] QString baseUrl() const { return m_baseUrl; }
    void setBaseUrl(const QString& url);
    void setSessionToken(const QString& token);
    [[nodiscard]] QString sessionToken() const { return m_sessionToken; }

    // -- Chats ---------------------------------------------------------------
    /// GET /api/experimental/chats with optional search query, label filters
    /// (key -> value), and pagination. Emits chatsReceived().
    Q_INVOKABLE void listChats(const QString& query = QString(),
                               const QVariantMap& labels = QVariantMap(), int limit = 0,
                               int offset = 0);
    /// POST /api/experimental/chats. The body is a codersdk.CreateChatRequest
    /// JSON object. Emits chatCreated().
    Q_INVOKABLE void createChat(const QJsonObject& request);
    /// GET /api/experimental/chats/{id}. Emits chatReceived().
    Q_INVOKABLE void getChat(const QString& chatId);
    /// PATCH /api/experimental/chats/{id} with a sparse
    /// codersdk.UpdateChatRequest object (only the provided keys are sent).
    /// Emits chatUpdated().
    Q_INVOKABLE void updateChat(const QString& chatId, const QJsonObject& patch);

    // -- Messages ------------------------------------------------------------
    /// GET /api/experimental/chats/{id}/messages?before_id&after_id&limit.
    /// Pass 0 to omit a parameter. Emits messagesReceived().
    Q_INVOKABLE void listMessages(const QString& chatId, qint64 beforeId = 0, qint64 afterId = 0,
                                  int limit = 0);
    /// POST /api/experimental/chats/{id}/messages. content is an array of
    /// codersdk.ChatInputPart objects. An HTTP 409 response carries a
    /// usage-limit payload and is emitted as usageLimitExceeded() instead of
    /// requestFailed(). Emits messageSent() on success.
    Q_INVOKABLE void sendMessage(const QString& chatId, const QJsonArray& content,
                                 const QString& busyBehavior = QStringLiteral("queue"),
                                 const QJsonObject& extra = QJsonObject());
    /// PATCH /api/experimental/chats/{id}/messages/{messageId} (edit and
    /// truncate history after the message). Emits messageEdited().
    Q_INVOKABLE void editMessage(const QString& chatId, qint64 messageId,
                                 const QJsonArray& content);

    // -- Run control ---------------------------------------------------------
    /// POST /api/experimental/chats/{id}/interrupt. Emits chatReceived().
    Q_INVOKABLE void interrupt(const QString& chatId);
    /// DELETE /api/experimental/chats/{id}/queue/{queuedId}.
    Q_INVOKABLE void deleteQueued(const QString& chatId, qint64 queuedId);
    /// POST /api/experimental/chats/{id}/queue/{queuedId}/promote.
    Q_INVOKABLE void promoteQueued(const QString& chatId, qint64 queuedId);
    /// POST /api/experimental/chats/{id}/tool-results. results is an array of
    /// codersdk.ToolResult objects.
    Q_INVOKABLE void sendToolResults(const QString& chatId, const QJsonArray& results);

    // -- Titles / diff / prompts ----------------------------------------------
    /// POST /api/experimental/chats/{id}/title/regenerate. Emits chatReceived().
    Q_INVOKABLE void regenerateTitle(const QString& chatId);
    /// POST /api/experimental/chats/{id}/title/propose. Emits titleProposed().
    Q_INVOKABLE void proposeTitle(const QString& chatId);
    /// GET /api/experimental/chats/{id}/diff. Emits diffReceived().
    Q_INVOKABLE void getDiff(const QString& chatId);
    /// GET /api/experimental/chats/{id}/prompts (composer history, newest
    /// first). Emits promptsReceived().
    Q_INVOKABLE void getPrompts(const QString& chatId, int limit = 0);

    // -- Catalog / config ------------------------------------------------------
    /// GET /api/experimental/chats/models. Emits modelsReceived().
    Q_INVOKABLE void listModels();
    /// GET /api/experimental/chats/model-configs. Emits modelConfigsReceived().
    Q_INVOKABLE void listModelConfigs();
    /// GET /api/experimental/mcp/servers. Emits mcpServersReceived().
    Q_INVOKABLE void listMcpServers();
    /// GET /api/experimental/chats/usage-limits/status.
    /// Emits usageLimitStatusReceived().
    Q_INVOKABLE void usageLimitStatus();
    /// GET /api/experimental/chats/by-workspace?workspace_ids=a,b,c: the
    /// latest non-archived chat ID per workspace, RBAC-filtered. The server
    /// caps the request at 25 IDs; callers must chunk larger lists. Emits
    /// chatsByWorkspaceReceived().
    Q_INVOKABLE void getChatsByWorkspace(const QStringList& workspaceIds);

    // -- Files ----------------------------------------------------------------
    /// POST /api/experimental/chats/files?organization={orgId}.
    ///
    /// The server handler (coderd/exp_chats.go postChatFile) reads the raw
    /// request body directly via io.ReadAll(r.Body), takes the media type
    /// from Content-Type, and extracts the filename from the
    /// Content-Disposition header. It is NOT a multipart form upload, so
    /// this sends the file bytes as the raw body with those two headers set
    /// (same wire format as codersdk ExperimentalClient.UploadChatFile).
    /// Emits fileUploaded().
    Q_INVOKABLE void uploadFile(const QString& organizationId, const QString& filename,
                                const QString& contentType, const QByteArray& data);
    /// GET /api/experimental/chats/files/{fileId}. Emits fileDownloaded().
    Q_INVOKABLE void downloadFile(const QString& fileId);

    // -- Availability probing --------------------------------------------------
    /// POST /api/v2/authcheck asking for chat create permission.
    /// Emits createChatPermissionReceived().
    Q_INVOKABLE void checkCreateChatPermission();

    /// GET /api/v2/organizations. Emits organizationsReceived() with the raw
    /// JSON array; the caller picks the default organization for chat and
    /// file-upload requests.
    Q_INVOKABLE void listOrganizations();

signals:
    void baseUrlChanged();
    /// Emitted for any non-2xx reply (except the 409 usage-limit case of
    /// sendMessage). body carries the raw response payload when available.
    void requestFailed(const QString& endpoint, int statusCode, const QString& errorMessage,
                       const QByteArray& body);

    void chatsReceived(const QList<Chat>& chats);
    /// Raw JSON array companion to chatsReceived(), used for disk caching.
    void chatsJsonReceived(const QJsonArray& raw);
    void chatCreated(const Chat& chat);
    void chatReceived(const Chat& chat);
    void chatUpdated(const QString& chatId);
    void messagesReceived(const QString& chatId, const QList<ChatMessage>& messages,
                          const QList<ChatQueuedMessage>& queued, bool hasMore, qint64 beforeId,
                          qint64 afterId);
    void messageSent(const QString& chatId, bool queued, const QJsonObject& response);
    void messageEdited(const QString& chatId, const QJsonObject& response);
    void usageLimitExceeded(const QString& chatId, const ChatUsageLimitExceeded& payload);
    void queuedDeleted(const QString& chatId, qint64 queuedId);
    void queuedPromoted(const QString& chatId, qint64 queuedId);
    void toolResultsSent(const QString& chatId);
    void titleProposed(const QString& chatId, const QString& title);
    void diffReceived(const QString& chatId, const QJsonObject& diff);
    void promptsReceived(const QString& chatId, const QStringList& prompts);
    void modelsReceived(const ChatModelsResponse& models);
    void modelConfigsReceived(const QList<ChatModelConfig>& configs);
    void mcpServersReceived(const QList<McpServerConfig>& servers);
    void usageLimitStatusReceived(const ChatUsageLimitStatus& status);
    /// Map of workspace ID -> latest chat ID for getChatsByWorkspace().
    void chatsByWorkspaceReceived(const QVariantMap& chatsByWorkspace);
    void fileUploaded(const QString& fileId);
    void fileDownloaded(const QString& fileId, const QByteArray& data, const QString& contentType);
    void createChatPermissionReceived(bool allowed);
    void organizationsReceived(const QJsonArray& organizations);

private:
    QNetworkAccessManager* m_nam = nullptr;  // Qt parent-owned (this)
    QString m_baseUrl;
    QString m_sessionToken;

    [[nodiscard]] QNetworkRequest buildRequest(const QString& path) const;
    [[nodiscard]] QNetworkReply* get(const QString& path);
    [[nodiscard]] QNetworkReply* post(const QString& path, const QJsonObject& body);
    [[nodiscard]] QNetworkReply* post(const QString& path);
    [[nodiscard]] QNetworkReply* patch(const QString& path, const QJsonObject& body);
    [[nodiscard]] QNetworkReply* del(const QString& path);
    void connectErrorHandler(QNetworkReply* reply, const QString& endpoint);
};

#endif  // AGENTSAPICLIENT_H
