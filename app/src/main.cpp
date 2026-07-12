#include <QApplication>
#include <QCommandLineParser>
#include <QFont>
#include <QIcon>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>

#include <QSocketNotifier>

#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>

#include "apps/IconThemeProvider.h"
#include "tray/SystemTrayIcon.h"
#include "vpn/VpnBridge.h"

#include "agents/AgentNotifier.h"
#include "agents/AgentsController.h"
#include "api/AgentApiClient.h"
#include "api/AgentsApiClient.h"
#include "api/CoderApiClient.h"
#include "api/PollingController.h"
#include "api/dto/Workspace.h"
#include "apps/AppModel.h"
#include "auth/LoginFlowController.h"
#include "data/SecureStorage.h"
#include "data/SessionManager.h"
#ifdef HAS_DLP
#include "dlp/DlpCompositorManager.h"
#endif
#include "filesync/FileSyncManager.h"
#include "filesync/FileTransferManager.h"
#include "filesync/MutagenDaemon.h"
#include "models/ChatListModel.h"
#include "models/PeerModel.h"
#include "models/WorkspaceModel.h"
#include "notifications/NotificationManager.h"
#include "settings/SettingsManager.h"
#include "terminal/TerminalBridge.h"
#include "updater/AutoUpdater.h"
#include "webview/AppBrowserWidget.h"

#include <QtQml/qqml.h>

#ifdef HAS_WEBENGINE
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#endif

static int s_sigFd[2];  // socketpair for signal→Qt bridge

static void unixSignalHandler(int /* sig */) {
    // Async-signal-safe: write a single byte to wake the Qt notifier.
    char c = 1;
    (void)write(s_sigFd[0], &c, sizeof(c));
}

int main(int argc, char* argv[]) {
    // QtWebEngine requires initialization before QApplication is created.
#ifdef HAS_WEBENGINE
    QtWebEngineQuick::initialize();
#endif

    QApplication app(argc, argv);
    app.setApplicationVersion(QStringLiteral(APP_VERSION));
    app.setApplicationName(QStringLiteral("Coder Desktop"));
    app.setOrganizationName(QStringLiteral("Coder"));
    app.setOrganizationDomain(QStringLiteral("coder.com"));

    // Use Inter font if available, fallback to system sans-serif.
    QFont appFont(QStringLiteral("Inter"));
    appFont.setStyleHint(QFont::SansSerif);
    app.setFont(appFont);

    app.setWindowIcon(QIcon(QStringLiteral(":/CoderDesktop/assets/coder-icon.svg")));

    // Keep the app running when the last window is closed (tray keeps it alive).
    app.setQuitOnLastWindowClosed(false);

    // Use the Material style for Coder brand theming.
    QQuickStyle::setStyle(QStringLiteral("Material"));

    // ---- VPN bridge (D-Bus client) ----
    VpnBridge vpnBridge;

    // ---- CLI argument parsing ----
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Coder Desktop for Linux"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption logLevelOption(
        QStringList() << QStringLiteral("l") << QStringLiteral("log-level"),
        QStringLiteral("Set log level (trace, debug, info, warn, error). Default: info"),
        QStringLiteral("level"), QStringLiteral("info"));
    parser.addOption(logLevelOption);

    QCommandLineOption logFileOption(
        QStringLiteral("log-file"),
        QStringLiteral("Write logs to file (default: /tmp/coder-desktop.log)"),
        QStringLiteral("path"), QStringLiteral("/tmp/coder-desktop.log"));
    parser.addOption(logFileOption);

    parser.process(app);

    // ---- Phase 2 managers ----
    SettingsManager settingsManager;

    // ---- Logging configuration ----
    QString logLevel =
        parser.isSet(logLevelOption) ? parser.value(logLevelOption) : settingsManager.logLevel();

    // Qt log types: debug, info, warning, critical, fatal
    // Map our levels to Qt filter rules:
    //   trace/debug → show all (debug + info + warning + critical)
    //   info        → hide debug; show info + warning + critical
    //   warn        → hide debug + info; show warning + critical
    //   error       → hide debug + info + warning; show critical only
    if (logLevel == QStringLiteral("trace") || logLevel == QStringLiteral("debug")) {
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true"));
        qSetMessagePattern(
            QStringLiteral("[%{time yyyy-MM-dd hh:mm:ss.zzz}] [%{type}] %{category}: %{message}"));
        qDebug() << "Debug logging enabled (level:" << logLevel << ")";
    } else if (logLevel == QStringLiteral("error")) {
        QLoggingCategory::setFilterRules(
            QStringLiteral("*.debug=false\n*.info=false\n*.warning=false"));
        qSetMessagePattern(QStringLiteral("[%{time hh:mm:ss}] [%{type}] %{category}: %{message}"));
    } else if (logLevel == QStringLiteral("warn")) {
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false\n*.info=false"));
        qSetMessagePattern(QStringLiteral("[%{time hh:mm:ss}] [%{type}] %{category}: %{message}"));
    } else {
        // "info" or unrecognized → default: suppress debug messages
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
        qSetMessagePattern(QStringLiteral("[%{time hh:mm:ss}] [%{type}] %{category}: %{message}"));
    }

    // ---- Log file support ----
    // File-scope static so the message handler lambda can access it.
    static FILE* s_logFile = nullptr;
    const QString logFilePath = parser.value(logFileOption);
    if (!logFilePath.isEmpty()) {
        s_logFile = std::fopen(logFilePath.toUtf8().constData(), "w");
        if (!s_logFile) {
            std::fprintf(stderr, "Warning: could not open log file: %s\n",
                         logFilePath.toUtf8().constData());
        }
    }

    // Install a custom message handler that writes to both stderr and the log file.
    qInstallMessageHandler(
        [](QtMsgType type, const QMessageLogContext& context, const QString& msg) {
            const QString formatted = qFormatLogMessage(type, context, msg);
            const QByteArray utf8 = formatted.toUtf8();

            std::fprintf(stderr, "%s\n", utf8.constData());

            if (s_logFile) {
                std::fprintf(s_logFile, "%s\n", utf8.constData());
                std::fflush(s_logFile);
            }
        });

    SecureStorage secureStorage;
    CoderApiClient apiClient;
    SessionManager sessionManager(apiClient, secureStorage);
    WorkspaceModel workspaceModel;
    PeerModel peerModel;

    // Wire VPN peer updates from the D-Bus bridge into the peer model.
    QObject::connect(&vpnBridge, &VpnBridge::peerUpdated,
                     [&peerModel](const QString& workspace, const QString& agent,
                                  const QString& hostname, int status, int lastPingMs, bool isP2P) {
                         peerModel.updatePeer({workspace, agent, hostname, status,
                                               static_cast<qint64>(lastPingMs), isP2P});
                     });

    // Clear stale peers when VPN disconnects.
    QObject::connect(&vpnBridge, &VpnBridge::stateChanged, [&vpnBridge, &peerModel]() {
        if (vpnBridge.state() == QStringLiteral("disconnected")) {
            peerModel.clear();
        }
    });

    NotificationManager notificationManager;

    // ---- Auto-updater ----
    AutoUpdater autoUpdater(QStringLiteral(APP_VERSION), &settingsManager);
    QObject::connect(&autoUpdater, &AutoUpdater::updateAvailable,
                     [](const QString& version, const QString& url) {
                         qInfo() << "Update available:" << version << "—" << url;
                     });

    // ---- Coder Agents (experimental chats API) ----
    AgentsApiClient agentsApiClient;
    AgentsController agentsController(&agentsApiClient);
    ChatListModel chatListModel;

    // Keep the chat list model in sync with the controller's chat state.
    QObject::connect(&agentsController, &AgentsController::chatsReset, &chatListModel,
                     &ChatListModel::setChats);
    QObject::connect(&agentsController, &AgentsController::chatUpserted, &chatListModel,
                     &ChatListModel::upsertChat);
    QObject::connect(&agentsController, &AgentsController::chatRemoved, &chatListModel,
                     &ChatListModel::removeChat);

    // Resolve workspace names for the chat list's workspace chips.
    WorkspaceModel* workspaceModelPtr = &workspaceModel;  // non-owning
    auto syncChatWorkspaceNames = [&chatListModel, workspaceModelPtr]() {
        QHash<QString, QString> names;
        const int rows = workspaceModelPtr->rowCount();
        for (int i = 0; i < rows; ++i) {
            const QModelIndex idx = workspaceModelPtr->index(i, 0);
            names.insert(workspaceModelPtr->data(idx, WorkspaceModel::IdRole).toString(),
                         workspaceModelPtr->data(idx, WorkspaceModel::NameRole).toString());
        }
        chatListModel.setWorkspaceNames(names);
    };
    QObject::connect(&workspaceModel, &WorkspaceModel::countChanged, syncChatWorkspaceNames);
    QObject::connect(&workspaceModel, &WorkspaceModel::dataChanged, syncChatWorkspaceNames);

    // Configure and (re)start the agents stack on auth and deployment
    // changes: always stop first so a deployment switch tears down the watch
    // socket and per-chat sessions before the base URL and token change.
    auto syncAgentsAuth = [&agentsController, &agentsApiClient, &sessionManager]() {
        agentsController.stop();
        if (!sessionManager.isAuthenticated()) return;
        agentsApiClient.setBaseUrl(sessionManager.currentUrl());
        agentsApiClient.setSessionToken(sessionManager.sessionToken());
        agentsController.start();
    };
    QObject::connect(&sessionManager, &SessionManager::authStateChanged, syncAgentsAuth);
    QObject::connect(&sessionManager, &SessionManager::tokenExpired, &agentsController,
                     &AgentsController::stop);

    // Desktop notifications for agent chat status changes and action
    // requests (skips the focused chat, dedupes watch vs polling).
    AgentNotifier agentNotifier(&agentsController, &notificationManager, &settingsManager);

    // ---- File sync / transfer ----
    AgentApiClient agentApiClient;
    MutagenDaemon mutagenDaemon;
    FileSyncManager fileSyncManager(&settingsManager, &mutagenDaemon);
    FileTransferManager fileTransferManager;

    // ---- Polling controller (auto-refresh, caching, notifications) ----
    PollingController pollingController(apiClient, workspaceModel, notificationManager,
                                        settingsManager);

    // NOTE: PollingController's constructor already connects
    // apiClient.workspacesReceived to its handler slot.

    // Show loading state and handle errors for workspace fetches.
    QObject::connect(&apiClient, &CoderApiClient::requestFailed,
                     [&workspaceModel](const QString& endpoint, int /*statusCode*/,
                                       const QString& errorMessage) {
                         if (endpoint.contains(QLatin1String("workspaces"))) {
                             workspaceModel.setLoading(false);
                             workspaceModel.setErrorMessage(errorMessage);
                         }
                     });

    // Start/stop polling on auth state changes.
    QObject::connect(&sessionManager, &SessionManager::authStateChanged,
                     [&pollingController, &sessionManager]() {
                         if (sessionManager.isAuthenticated())
                             pollingController.start();
                         else
                             pollingController.stop();
                     });
    QObject::connect(&sessionManager, &SessionManager::tokenExpired, &pollingController,
                     &PollingController::stop);

    // Re-fetch after workspace actions (start/stop/update/delete).
    QObject::connect(&apiClient, &CoderApiClient::workspaceActionCompleted, &pollingController,
                     &PollingController::refreshNow);

    // Wire notificationsEnabled setting to NotificationManager.
    auto syncNotifEnabled = [&]() {
        notificationManager.setEnabled(settingsManager.notificationsEnabled());
    };
    syncNotifEnabled();
    QObject::connect(&settingsManager, &SettingsManager::settingsChanged, syncNotifEnabled);

    // Wire VPN state to FileSyncManager so it knows when the tunnel is up.
    QObject::connect(&vpnBridge, &VpnBridge::stateChanged, [&]() {
        const bool connected = vpnBridge.state() == QStringLiteral("connected");
        fileSyncManager.setVpnConnected(connected);
        if (!connected) {
            peerModel.clear();
        }
    });

    // Wire VPN peer updates into the PeerModel so QML can enumerate connected agents.
    QObject::connect(&vpnBridge, &VpnBridge::peerUpdated,
                     [&](const QString& workspace, const QString& agent, const QString& hostname,
                         int status, int lastPingMs, bool isP2P) {
                         peerModel.updatePeer(PeerModel::PeerInfo{
                             .workspaceName = workspace,
                             .agentName = agent,
                             .hostname = hostname,
                             .status = status,
                             .lastPingMs = lastPingMs,
                             .isP2P = isP2P,
                         });
                     });

    // ---- Login flow (browser-based auth) ----
    LoginFlowController loginFlowController(sessionManager);

    // ---- Phase 3 components ----
    AppBrowserWidget appBrowser;

    // ---- App model (discovered desktop apps) ----
    AppModel appModel;
    appModel.refresh();  // Start async app discovery

#ifdef HAS_DLP
    // ---- DLP compositor manager (per-app isolation) ----
    DlpCompositorManager dlpCompositor;
    dlpCompositor.setLogLevel(logLevel);

    // Wire DLP settings changes to the compositor policy.
    QObject::connect(&settingsManager, &SettingsManager::settingsChanged, [&]() {
        if (dlpCompositor.isRunning()) {
            dlpCompositor.updatePolicy(settingsManager.dlpClipboardBlock(),
                                       settingsManager.dlpClipboardBlock(),  // both directions
                                       true,  // screenshot always blocked
                                       settingsManager.dlpFileSandbox(),
                                       settingsManager.dlpNetworkSandbox(),
                                       settingsManager.dlpWatermark());
        }
    });

    // Wire auth state changes to update watermark identity.
    auto updateWatermarkIdentity = [&]() {
        if (sessionManager.isAuthenticated()) {
            const QString identity = QStringLiteral("%1@%2").arg(sessionManager.currentUsername(),
                                                                 sessionManager.currentUrl());
            dlpCompositor.setWatermarkIdentity(identity);
        }
    };
    QObject::connect(&sessionManager, &SessionManager::authStateChanged, updateWatermarkIdentity);
    updateWatermarkIdentity();  // apply immediately if already authenticated
#endif

    // ---- QML engine ----
    // All context properties must be set BEFORE loadFromModule() so they
    // are available when QML bindings are first evaluated.
    // Register QML types for in-app terminal.
    qmlRegisterType<TerminalBridge>("CoderDesktop", 1, 0, "TerminalBridge");

    QQmlApplicationEngine engine;

    // Tell the QML engine to look for modules inside the Qt resource system.
    // qt_add_qml_module() with RESOURCE_PREFIX / embeds the CoderDesktop
    // module's qmldir at qrc:/CoderDesktop/qmldir.  Without this import path
    // the engine only searches on-disk paths — which works from the build tree
    // (where it finds build/app/CoderDesktop/qmldir) but fails when installed
    // because only the binary is deployed.  Adding "qrc:/" lets "import
    // CoderDesktop" resolve the embedded qmldir and its singleton declarations.
    engine.addImageProvider(QStringLiteral("icon-theme"), new IconThemeProvider);
    engine.addImportPath(QStringLiteral("qrc:/"));

    engine.rootContext()->setContextProperty(QStringLiteral("vpnBridge"), &vpnBridge);
    engine.rootContext()->setContextProperty(QStringLiteral("autoUpdater"), &autoUpdater);
    engine.rootContext()->setContextProperty(QStringLiteral("settingsManager"), &settingsManager);
    engine.rootContext()->setContextProperty(QStringLiteral("sessionManager"), &sessionManager);
    engine.rootContext()->setContextProperty(QStringLiteral("apiClient"), &apiClient);
    engine.rootContext()->setContextProperty(QStringLiteral("workspaceModel"), &workspaceModel);
    engine.rootContext()->setContextProperty(QStringLiteral("peerModel"), &peerModel);
    engine.rootContext()->setContextProperty(QStringLiteral("notificationManager"),
                                             &notificationManager);
    engine.rootContext()->setContextProperty(QStringLiteral("agentsController"), &agentsController);
    engine.rootContext()->setContextProperty(QStringLiteral("chatListModel"), &chatListModel);
    engine.rootContext()->setContextProperty(QStringLiteral("appBrowser"), &appBrowser);
#ifdef HAS_DLP
    engine.rootContext()->setContextProperty(QStringLiteral("dlpCompositor"), &dlpCompositor);
#endif
    engine.rootContext()->setContextProperty(QStringLiteral("appModel"), &appModel);
    engine.rootContext()->setContextProperty(QStringLiteral("agentApiClient"), &agentApiClient);
    engine.rootContext()->setContextProperty(QStringLiteral("fileSyncManager"), &fileSyncManager);
    engine.rootContext()->setContextProperty(QStringLiteral("fileTransferManager"),
                                             &fileTransferManager);
    engine.rootContext()->setContextProperty(QStringLiteral("loginFlowController"),
                                             &loginFlowController);
    engine.rootContext()->setContextProperty(QStringLiteral("pollingController"),
                                             &pollingController);
    engine.rootContext()->setContextProperty(QStringLiteral("qtRuntimeVersion"),
                                             QString::fromLatin1(qVersion()));

    // Load Main.qml from compiled-in Qt resources.  Each QML file has an
    // explicit "import CoderDesktop" so the module's qmldir is processed and
    // singletons (CoderTheme) are properly registered regardless of how the
    // root component is loaded.
    //
    // We use engine.load(qrc:…) instead of engine.loadFromModule() for Qt 6.4
    // compatibility (loadFromModule was added in Qt 6.5).
#ifdef HAS_DLP
    engine.rootContext()->setContextProperty(QStringLiteral("hasDlp"), true);
#else
    engine.rootContext()->setContextProperty(QStringLiteral("hasDlp"), false);
#endif

    const QUrl mainQml(QStringLiteral("qrc:/CoderDesktop/qml/Main.qml"));

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [&mainQml](QObject* obj, const QUrl& url) {
            if (!obj && url == mainQml) {
                qCritical() << "Failed to load QML:" << url;
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);

    engine.load(mainQml);

    // ---- System tray ----
    SystemTrayIcon tray(&vpnBridge, &sessionManager, &fileSyncManager, &agentsController);
    notificationManager.setTrayIcon(tray.trayIcon());

    // Raise the first root window (tray and notification click routing).
    auto showMainWindow = [&engine]() -> QObject* {
        const auto roots = engine.rootObjects();
        for (auto* obj : roots) {
            if (auto* w = qobject_cast<QQuickWindow*>(obj)) {
                w->show();
                w->raise();
                w->requestActivate();
                return w;
            }
        }
        return nullptr;
    };

    QObject::connect(&tray, &SystemTrayIcon::showWindowRequested, &engine,
                     [showMainWindow]() { showMainWindow(); });

    // "Open Agents" in the tray menu: raise the window and switch to the
    // Agents tab (Main.qml exposes openAgentsTab()).
    QObject::connect(&tray, &SystemTrayIcon::showAgentsRequested, &engine, [showMainWindow]() {
        if (QObject* window = showMainWindow()) QMetaObject::invokeMethod(window, "openAgentsTab");
    });

    // Clicking a notification balloon raises the window; agent-update
    // notifications additionally open the Agents tab.
    QObject::connect(&notificationManager, &NotificationManager::notificationClicked, &engine,
                     [showMainWindow](const QString& category) {
                         QObject* window = showMainWindow();
                         if (window && category == QLatin1String("AgentUpdate"))
                             QMetaObject::invokeMethod(window, "openAgentsTab");
                     });

    // If already authenticated from a saved session, start polling immediately.
    if (sessionManager.isAuthenticated()) {
        pollingController.start();
        syncAgentsAuth();
    }

    // Set up graceful shutdown on SIGINT/SIGTERM.
    // Without this, Ctrl+C in the terminal calls _exit() immediately,
    // skipping C++ destructors.  DLP compositors need orderly cleanup
    // (wlr_backend_destroy, Xwayland teardown, GPU resource release)
    // to avoid destabilizing the host display server.
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, s_sigFd) == 0) {
        auto* sigNotifier = new QSocketNotifier(s_sigFd[1], QSocketNotifier::Read, &app);
        QObject::connect(sigNotifier, &QSocketNotifier::activated, &app, [&]() {
            char c;
            (void)read(s_sigFd[1], &c, sizeof(c));
            qInfo() << "Received termination signal, shutting down gracefully...";
            QApplication::quit();
        });

        struct sigaction sa;
        sa.sa_handler = unixSignalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
    }

    return app.exec();
}
