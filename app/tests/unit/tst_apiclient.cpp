#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "api/CoderApiClient.h"
#include "api/dto/BuildInfo.h"
#include "api/dto/Template.h"
#include "api/dto/User.h"
#include "api/dto/Workspace.h"
#include "data/SecureStorage.h"

/// Unit tests for the Coder REST API client, DTO parsing, and SecureStorage.
class TestApiClient : public QObject {
    Q_OBJECT

private slots:

    // -----------------------------------------------------------------------
    // DTO: User
    // -----------------------------------------------------------------------

    void testUserFromJson()
    {
        const QJsonObject obj{
            {QLatin1String("id"),         QStringLiteral("abc-123")},
            {QLatin1String("username"),   QStringLiteral("testuser")},
            {QLatin1String("email"),      QStringLiteral("test@coder.com")},
            {QLatin1String("avatar_url"), QStringLiteral("https://example.com/avatar.png")},
        };
        const User u = User::fromJson(obj);
        QCOMPARE(u.id,        QStringLiteral("abc-123"));
        QCOMPARE(u.username,  QStringLiteral("testuser"));
        QCOMPARE(u.email,     QStringLiteral("test@coder.com"));
        QCOMPARE(u.avatarUrl, QStringLiteral("https://example.com/avatar.png"));
    }

    void testUserFromJsonMissingFields()
    {
        const User u = User::fromJson(QJsonObject{});
        QVERIFY(u.id.isEmpty());
        QVERIFY(u.username.isEmpty());
    }

    // -----------------------------------------------------------------------
    // DTO: Template
    // -----------------------------------------------------------------------

    void testTemplateFromJson()
    {
        const QJsonObject obj{
            {QLatin1String("id"),                QStringLiteral("tmpl-1")},
            {QLatin1String("name"),              QStringLiteral("docker")},
            {QLatin1String("display_name"),      QStringLiteral("Docker")},
            {QLatin1String("description"),       QStringLiteral("A docker template")},
            {QLatin1String("icon"),              QStringLiteral("/icon/docker.svg")},
            {QLatin1String("active_version_id"), QStringLiteral("v-99")},
            {QLatin1String("organization_id"),   QStringLiteral("org-1")},
        };
        const Template t = Template::fromJson(obj);
        QCOMPARE(t.id,              QStringLiteral("tmpl-1"));
        QCOMPARE(t.name,            QStringLiteral("docker"));
        QCOMPARE(t.displayName,     QStringLiteral("Docker"));
        QCOMPARE(t.activeVersionId, QStringLiteral("v-99"));
    }

    void testTemplateListFromJson()
    {
        QJsonArray arr;
        arr.append(QJsonObject{{QLatin1String("id"), QStringLiteral("t1")}});
        arr.append(QJsonObject{{QLatin1String("id"), QStringLiteral("t2")}});
        const auto list = Template::listFromJson(arr);
        QCOMPARE(list.size(), 2);
        QCOMPARE(list[0].id, QStringLiteral("t1"));
        QCOMPARE(list[1].id, QStringLiteral("t2"));
    }

    // -----------------------------------------------------------------------
    // DTO: BuildInfo
    // -----------------------------------------------------------------------

    void testBuildInfoFromJson()
    {
        const QJsonObject obj{
            {QLatin1String("version"),               QStringLiteral("v2.5.0")},
            {QLatin1String("dashboard_url"),          QStringLiteral("https://coder.example.com")},
            {QLatin1String("external_auth_enabled"),  true},
        };
        const BuildInfo b = BuildInfo::fromJson(obj);
        QCOMPARE(b.version,      QStringLiteral("v2.5.0"));
        QCOMPARE(b.dashboardUrl, QStringLiteral("https://coder.example.com"));
        QCOMPARE(b.externalAuthEnabled, true);
    }

    // -----------------------------------------------------------------------
    // DTO: Workspace (with agents, apps, enums)
    // -----------------------------------------------------------------------

    void testWorkspaceFromJson()
    {
        // Build a realistic Workspace JSON with nested agents.
        QJsonObject appObj{
            {QLatin1String("display_name"), QStringLiteral("VS Code")},
            {QLatin1String("slug"),         QStringLiteral("vscode")},
            {QLatin1String("icon"),         QStringLiteral("/icon/code.svg")},
            {QLatin1String("url"),          QStringLiteral("https://code.example.com")},
            {QLatin1String("subdomain"),    true},
            {QLatin1String("external"),     false},
        };
        QJsonObject agentObj{
            {QLatin1String("id"),     QStringLiteral("agent-1")},
            {QLatin1String("name"),   QStringLiteral("main")},
            {QLatin1String("status"), QStringLiteral("connected")},
            {QLatin1String("apps"),   QJsonArray{appObj}},
        };
        QJsonObject resource{
            {QLatin1String("agents"), QJsonArray{agentObj}},
        };
        QJsonObject latestBuild{
            {QLatin1String("id"),        QStringLiteral("build-42")},
            {QLatin1String("status"),    QStringLiteral("running")},
            {QLatin1String("resources"), QJsonArray{resource}},
        };
        QJsonObject health{
            {QLatin1String("healthy"), true},
        };
        QJsonObject wsObj{
            {QLatin1String("id"),                         QStringLiteral("ws-1")},
            {QLatin1String("name"),                       QStringLiteral("my-workspace")},
            {QLatin1String("owner_name"),                 QStringLiteral("admin")},
            {QLatin1String("template_name"),              QStringLiteral("docker")},
            {QLatin1String("template_icon"),              QStringLiteral("/icon/docker.svg")},
            {QLatin1String("health"),                     health},
            {QLatin1String("favorite"),                   true},
            {QLatin1String("outdated"),                   false},
            {QLatin1String("latest_build"),               latestBuild},
            {QLatin1String("template_active_version_id"), QStringLiteral("v-99")},
        };

        const Workspace w = Workspace::fromJson(wsObj);
        QCOMPARE(w.id,           QStringLiteral("ws-1"));
        QCOMPARE(w.name,         QStringLiteral("my-workspace"));
        QCOMPARE(w.ownerName,    QStringLiteral("admin"));
        QCOMPARE(w.templateName, QStringLiteral("docker"));
        QCOMPARE(w.status,       WorkspaceStatus::Running);
        QCOMPARE(w.health,       QStringLiteral("healthy"));
        QCOMPARE(w.favorite,     true);
        QCOMPARE(w.outdated,     false);
        QCOMPARE(w.latestBuildId, QStringLiteral("build-42"));
        QCOMPARE(w.templateActiveVersionId, QStringLiteral("v-99"));

        // Agents
        QCOMPARE(w.agents.size(), 1);
        QCOMPARE(w.agents[0].id,   QStringLiteral("agent-1"));
        QCOMPARE(w.agents[0].name, QStringLiteral("main"));
        QCOMPARE(w.agents[0].status, AgentStatus::Connected);

        // Apps
        QCOMPARE(w.agents[0].apps.size(), 1);
        QCOMPARE(w.agents[0].apps[0].displayName, QStringLiteral("VS Code"));
        QCOMPARE(w.agents[0].apps[0].subdomain, true);
    }

    void testWorkspaceStatusEnum()
    {
        QCOMPARE(workspaceStatusFromString(QStringLiteral("running")),
                 WorkspaceStatus::Running);
        QCOMPARE(workspaceStatusFromString(QStringLiteral("stopped")),
                 WorkspaceStatus::Stopped);
        QCOMPARE(workspaceStatusFromString(QStringLiteral("bogus")),
                 WorkspaceStatus::Unknown);
    }

    void testAgentStatusEnum()
    {
        QCOMPARE(agentStatusFromString(QStringLiteral("connected")),
                 AgentStatus::Connected);
        QCOMPARE(agentStatusFromString(QStringLiteral("timeout")),
                 AgentStatus::Timeout);
        QCOMPARE(agentStatusFromString(QStringLiteral("xyz")),
                 AgentStatus::Unknown);
    }

    // -----------------------------------------------------------------------
    // CoderApiClient: buildRequest header verification
    // -----------------------------------------------------------------------

    void testBuildRequestHeaders()
    {
        CoderApiClient client;
        client.setBaseUrl(QStringLiteral("https://coder.example.com/"));
        client.setSessionToken(QStringLiteral("tok-secret-123"));

        // Use getAuthenticatedUser to obtain a reply, inspect the request.
        QNetworkReply *reply = client.getAuthenticatedUser();
        QVERIFY(reply);

        const QNetworkRequest req = reply->request();
        // URL should have trailing slash stripped + path appended.
        QCOMPARE(req.url().toString(),
                 QStringLiteral("https://coder.example.com/api/v2/users/me"));
        // Auth header must be Coder-Session-Token, not Bearer.
        QCOMPARE(req.rawHeader("Coder-Session-Token"),
                 QByteArray("tok-secret-123"));
        QVERIFY(!req.hasRawHeader("Authorization"));
        // Content-Type
        QCOMPARE(req.header(QNetworkRequest::ContentTypeHeader).toString(),
                 QStringLiteral("application/json"));

        reply->abort();
        reply->deleteLater();
    }

    void testBaseUrlNormalisation()
    {
        CoderApiClient client;
        client.setBaseUrl(QStringLiteral("https://coder.example.com///"));
        QCOMPARE(client.baseUrl(), QStringLiteral("https://coder.example.com"));
    }

    void testAuthStateSignal()
    {
        CoderApiClient client;
        QVERIFY(!client.isAuthenticated());

        QSignalSpy spy(&client, &CoderApiClient::authStateChanged);
        client.setSessionToken(QStringLiteral("tok"));
        QCOMPARE(spy.count(), 1);
        QVERIFY(client.isAuthenticated());

        // Setting the same token should not re-emit (still authenticated).
        client.setSessionToken(QStringLiteral("tok2"));
        QCOMPARE(spy.count(), 1);

        // Clearing should emit.
        client.setSessionToken(QString());
        QCOMPARE(spy.count(), 2);
        QVERIFY(!client.isAuthenticated());
    }

    // -----------------------------------------------------------------------
    // SecureStorage: file-based fallback round-trip
    // -----------------------------------------------------------------------

    void testSecureStorageFileFallback()
    {
        // We instantiate SecureStorage but override its fallback path
        // by using a temp dir via environment variable or direct member.
        // Since m_fallbackPath is private, we test end-to-end: the test
        // environment likely has no keyring, so it will use the file backend.
        SecureStorage storage;

        const QString url1 = QStringLiteral("https://coder1.example.com");
        const QString url2 = QStringLiteral("https://coder2.example.com");
        const QString tok1 = QStringLiteral("session-token-abc");
        const QString tok2 = QStringLiteral("session-token-xyz");

        // Store
        QVERIFY(storage.storeToken(url1, tok1));
        QVERIFY(storage.storeToken(url2, tok2));

        // Retrieve
        QCOMPARE(storage.retrieveToken(url1), tok1);
        QCOMPARE(storage.retrieveToken(url2), tok2);

        // Miss
        QVERIFY(storage.retrieveToken(QStringLiteral("https://nope")).isEmpty());

        // List
        QStringList urls = storage.storedDeploymentUrls();
        QVERIFY(urls.contains(url1));
        QVERIFY(urls.contains(url2));

        // Remove
        QVERIFY(storage.removeToken(url1));
        QVERIFY(storage.retrieveToken(url1).isEmpty());
        QVERIFY(!storage.removeToken(url1)); // already gone

        // Clean up url2
        [[maybe_unused]] bool removed = storage.removeToken(url2);
    }
};

QTEST_MAIN(TestApiClient)
#include "tst_apiclient.moc"
