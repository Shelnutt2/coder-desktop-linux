set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Dependencies (Qt6, libsecret, etc.) are found centrally in
# cmake/FindDependencies.cmake — variables are inherited from the parent scope.

# ---------------------------------------------------------------------------
# D-Bus interface proxy (auto-generated from introspection XML)
# ---------------------------------------------------------------------------
qt_add_dbus_interface(APP_DBUS_SOURCES
    ${CMAKE_SOURCE_DIR}/dbus/com.coder.Desktop.Helper1.xml
    helper1_interface
)

add_executable(coder-desktop
    src/main.cpp
    ${APP_DBUS_SOURCES}
    src/vpn/VpnBridge.h
    src/vpn/VpnBridge.cpp
    src/tray/SystemTrayIcon.h
    src/tray/SystemTrayIcon.cpp
    src/api/CoderApiClient.h
    src/api/CoderApiClient.cpp
    src/api/dto/User.h
    src/api/dto/Workspace.h
    src/api/dto/Template.h
    src/api/dto/BuildInfo.h
    src/data/Deployment.h
    src/data/SecureStorage.h
    src/data/SecureStorage.cpp
    src/data/SessionManager.h
    src/data/SessionManager.cpp
    src/models/WorkspaceModel.h
    src/models/WorkspaceModel.cpp
    src/models/PeerModel.h
    src/models/PeerModel.cpp
    src/notifications/NotificationManager.h
    src/notifications/NotificationManager.cpp
    src/settings/SettingsManager.h
    src/settings/SettingsManager.cpp
    src/settings/MdmConfigManager.h
    src/settings/MdmConfigManager.cpp
    src/api/WebSocketBase.h
    src/api/WebSocketBase.cpp
    src/api/TerminalWebSocket.h
    src/api/TerminalWebSocket.cpp
    src/api/BuildLogWebSocket.h
    src/api/BuildLogWebSocket.cpp
    src/webview/AppBrowserWidget.h
    src/webview/AppBrowserWidget.cpp
    src/util/UniqueFd.h
    src/apps/AppEntry.h
    src/apps/AppDiscovery.h
    src/apps/AppDiscovery.cpp
    src/apps/AppModel.h
    src/apps/AppModel.cpp
    src/apps/AppLaunchProfile.h
    src/apps/AppLaunchProfile.cpp
    src/apps/IconThemeProvider.h
    src/apps/IconThemeProvider.cpp
    src/apps/RunningAppModel.h
    src/apps/RunningAppModel.cpp
    src/updater/AutoUpdater.h
    src/updater/AutoUpdater.cpp
    src/auth/LoginFlowController.h
    src/auth/LoginFlowController.cpp
    src/api/PollingController.h
    src/api/PollingController.cpp
    src/api/dto/Chat.h
    src/api/dto/ChatMessage.h
    src/api/dto/ChatStreamEvent.h
    src/api/dto/ChatModels.h
    src/api/AgentsApiClient.h
    src/api/AgentsApiClient.cpp
    src/api/ChatStreamWebSocket.h
    src/api/ChatStreamWebSocket.cpp
    src/api/ChatWatchWebSocket.h
    src/api/ChatWatchWebSocket.cpp
    src/agents/ChatSession.h
    src/agents/ChatSession.cpp
    src/agents/AgentsController.h
    src/agents/AgentsController.cpp
    src/agents/AgentNotifier.h
    src/agents/AgentNotifier.cpp
    src/agents/PlanStepParser.h
    src/agents/PlanStepParser.cpp
    src/agents/AskUserQuestionParser.h
    src/agents/AskUserQuestionParser.cpp
    src/agents/JsonSchemaParser.h
    src/agents/JsonSchemaParser.cpp
    src/agents/DiffParser.h
    src/agents/DiffParser.cpp
    src/models/ChatListModel.h
    src/models/ChatListModel.cpp
    src/models/ChatMessagesModel.h
    src/models/ChatMessagesModel.cpp
    src/terminal/TerminalBridge.h
    src/terminal/TerminalBridge.cpp
)
if(ENABLE_DLP)
    target_sources(coder-desktop PRIVATE
        src/dlp/DlpCompositorManager.h
        src/dlp/DlpCompositorManager.cpp
    )
endif()

# File sync sources (always compiled — runtime feature-gated by VPN + policy)
target_sources(coder-desktop PRIVATE
    src/api/AgentApiClient.h
    src/api/AgentApiClient.cpp
    src/api/dto/AgentDirectory.h
    src/filesync/MutagenDaemon.h
    src/filesync/MutagenDaemon.cpp
    src/filesync/FileSyncSession.h
    src/filesync/FileSyncSession.cpp
    src/filesync/FileSyncManager.h
    src/filesync/FileSyncManager.cpp
    src/filesync/FileTransferManager.h
    src/filesync/FileTransferManager.cpp
)

set_source_files_properties(qml/CoderTheme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(coder-desktop
    URI CoderDesktop
    VERSION 1.0
    RESOURCE_PREFIX /
    QML_FILES
        qml/Main.qml
        qml/LoginPage.qml
        qml/WorkspacesPage.qml
        qml/VpnPage.qml
        qml/DeploymentPage.qml
        qml/SettingsPage.qml
        qml/AppBrowserPage.qml
        qml/WorkspaceDetailPage.qml
        qml/AppTile.qml
        qml/CategoryFilterBar.qml
        qml/LaunchDialog.qml
        qml/RunningAppsPanel.qml
        qml/TerminalPage.qml
        qml/CoderTheme.qml
        qml/StatusChip.qml
        qml/CoderButton.qml
        qml/AgentsPage.qml
        qml/AgentCreatePage.qml
        qml/AgentChatPage.qml
        qml/agents/MarkdownText.qml
        qml/agents/ReasoningBlock.qml
        qml/agents/ToolCallCard.qml
        qml/agents/SourceCitationRow.qml
        qml/agents/FileChip.qml
        qml/agents/SkillChip.qml
        qml/agents/SubagentInlineCard.qml
        qml/agents/AskUserQuestionCard.qml
        qml/agents/StreamingIndicator.qml
        qml/agents/ChatStatusCallout.qml
        qml/agents/QueuedMessagesList.qml
        qml/agents/ChatComposer.qml
        qml/agents/PlanCard.qml
        qml/agents/ActionRequiredForm.qml
        qml/agents/ChatDiffPage.qml
)
if(ENABLE_DLP)
    qt_target_qml_sources(coder-desktop QML_FILES qml/SecureDevPage.qml)
endif()

# File sync / file browser QML pages (always registered — runtime feature-gated)
qt_target_qml_sources(coder-desktop QML_FILES
    qml/FileSyncPage.qml
    qml/FileSyncSessionDialog.qml
    qml/RemoteDirectoryPicker.qml
    qml/FileBrowserPage.qml
    qml/TransferPanel.qml
)

qt_add_resources(coder-desktop "terminal_assets"
    PREFIX "/CoderDesktop"
    FILES
        assets/terminal/terminal.html
        assets/terminal/xterm.min.js
        assets/terminal/xterm.min.css
        assets/terminal/addon-fit.min.js
)

qt_add_resources(coder-desktop "brand_assets"
    PREFIX "/CoderDesktop"
    FILES
        assets/coder-logo.svg
        assets/coder-icon.svg
)

# Monochrome UI icons bundled so the agents UI never depends on the host
# freedesktop icon theme or emoji font coverage. Rendering needs the Qt SVG
# image plugin at runtime (packaged as libqt6svg / qt6-qtsvg).
qt_add_resources(coder-desktop "agent_icons"
    PREFIX "/CoderDesktop"
    FILES
        assets/icons/paperclip.svg
        assets/icons/send.svg
        assets/icons/stop.svg
        assets/icons/plan.svg
        assets/icons/subagent.svg
        assets/icons/question.svg
)

target_include_directories(coder-desktop PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(coder-desktop PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(coder-desktop PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent
    Qt6::Network Qt6::Widgets Qt6::DBus
)
if(ENABLE_DLP)
    target_link_libraries(coder-desktop PRIVATE coderdlp)
    target_compile_definitions(coder-desktop PRIVATE HAS_DLP)
endif()

if(Qt6WebSockets_FOUND)
    target_link_libraries(coder-desktop PRIVATE Qt6::WebSockets)
    target_compile_definitions(coder-desktop PRIVATE HAS_WEBSOCKETS)
endif()

if(Qt6WebEngineQuick_FOUND)
    target_link_libraries(coder-desktop PRIVATE Qt6::WebEngineQuick)
    target_compile_definitions(coder-desktop PRIVATE HAS_WEBENGINE)
endif()

if(Qt6WebChannel_FOUND)
    target_link_libraries(coder-desktop PRIVATE Qt6::WebChannel)
    target_compile_definitions(coder-desktop PRIVATE HAS_WEBCHANNEL)
endif()

if(LIBSECRET_FOUND)
    target_link_libraries(coder-desktop PRIVATE ${LIBSECRET_LIBRARIES})
    target_include_directories(coder-desktop PRIVATE ${LIBSECRET_INCLUDE_DIRS})
    target_compile_definitions(coder-desktop PRIVATE HAS_LIBSECRET)
endif()

# ---------------------------------------------------------------------------
# Unit tests  (disable with -DBUILD_TESTING=OFF)
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
    enable_testing()

    add_executable(tst_apiclient
        tests/unit/tst_apiclient.cpp
        src/api/CoderApiClient.cpp
        src/data/SecureStorage.cpp
    )
    target_include_directories(tst_apiclient PRIVATE src)
    target_link_libraries(tst_apiclient PRIVATE Qt6::Core Qt6::Test Qt6::Network)
    if(LIBSECRET_FOUND)
        target_link_libraries(tst_apiclient PRIVATE ${LIBSECRET_LIBRARIES})
        target_include_directories(tst_apiclient PRIVATE ${LIBSECRET_INCLUDE_DIRS})
        target_compile_definitions(tst_apiclient PRIVATE HAS_LIBSECRET)
    endif()
    add_test(NAME ApiClientTest COMMAND tst_apiclient)

    add_executable(tst_models
        tests/unit/tst_models.cpp
        src/models/WorkspaceModel.cpp
        src/models/PeerModel.cpp
        src/notifications/NotificationManager.cpp
    )
    target_include_directories(tst_models PRIVATE src)
    target_link_libraries(tst_models PRIVATE Qt6::Core Qt6::Test Qt6::Widgets)
    add_test(NAME ModelsTest COMMAND tst_models)

    add_executable(tst_settings
        tests/unit/tst_settings.cpp
        src/settings/SettingsManager.cpp
        src/settings/MdmConfigManager.cpp
    )
    target_include_directories(tst_settings PRIVATE src)
    target_link_libraries(tst_settings PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME SettingsTest COMMAND tst_settings)

    add_executable(tst_runningappmodel
        tests/unit/tst_runningappmodel.cpp
        src/apps/RunningAppModel.cpp
    )
    target_include_directories(tst_runningappmodel PRIVATE src)
    target_link_libraries(tst_runningappmodel PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME RunningAppModelTest COMMAND tst_runningappmodel)

    add_executable(tst_applaunchprofile
        tests/unit/tst_applaunchprofile.cpp
        src/apps/AppLaunchProfile.cpp
    )
    target_include_directories(tst_applaunchprofile PRIVATE src)
    target_link_libraries(tst_applaunchprofile PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME AppLaunchProfileTest COMMAND tst_applaunchprofile)

    add_executable(tst_filesync
        tests/unit/tst_filesync.cpp
        src/filesync/FileSyncSession.cpp
        src/filesync/FileSyncManager.cpp
        src/filesync/MutagenDaemon.cpp
        src/api/AgentApiClient.cpp
        src/api/dto/AgentDirectory.h
        src/filesync/FileTransferManager.cpp
        src/settings/SettingsManager.cpp
        src/settings/MdmConfigManager.cpp
    )
    target_include_directories(tst_filesync PRIVATE src)
    target_link_libraries(tst_filesync PRIVATE Qt6::Core Qt6::Test Qt6::Network)
    add_test(NAME FileSyncTest COMMAND tst_filesync)

    add_executable(tst_agentdtos
        tests/unit/tst_agentdtos.cpp
        src/api/dto/Chat.h
        src/api/dto/ChatMessage.h
        src/api/dto/ChatStreamEvent.h
        src/api/dto/ChatModels.h
    )
    target_include_directories(tst_agentdtos PRIVATE src)
    target_link_libraries(tst_agentdtos PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME AgentDtosTest COMMAND tst_agentdtos)

    add_executable(tst_chatsession
        tests/unit/tst_chatsession.cpp
        src/agents/ChatSession.cpp
    )
    target_include_directories(tst_chatsession PRIVATE src)
    target_link_libraries(tst_chatsession PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME ChatSessionTest COMMAND tst_chatsession)

    add_executable(tst_chatmodels
        tests/unit/tst_chatmodels.cpp
        src/agents/ChatSession.cpp
        src/models/ChatListModel.cpp
        src/models/ChatMessagesModel.cpp
    )
    target_include_directories(tst_chatmodels PRIVATE src)
    target_link_libraries(tst_chatmodels PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME ChatModelsTest COMMAND tst_chatmodels)

    add_executable(tst_agentparsers
        tests/unit/tst_agentparsers.cpp
        src/agents/PlanStepParser.cpp
        src/agents/JsonSchemaParser.cpp
        src/agents/DiffParser.cpp
        src/agents/AskUserQuestionParser.cpp
    )
    target_include_directories(tst_agentparsers PRIVATE src)
    target_link_libraries(tst_agentparsers PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME AgentParsersTest COMMAND tst_agentparsers)
endif()

# ---------------------------------------------------------------------------
# Install  (GNUInstallDirs included at top level)
# ---------------------------------------------------------------------------
install(TARGETS coder-desktop
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT runtime
)
