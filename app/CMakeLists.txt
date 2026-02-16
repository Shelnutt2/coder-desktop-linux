set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network Widgets Test)
# Optional components — available on some distros/configurations.
find_package(Qt6 COMPONENTS WebSockets QUIET)
find_package(Qt6 COMPONENTS WebEngineQuick QUIET)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSECRET libsecret-1)  # optional — credential storage

add_executable(coder-desktop
    src/main.cpp
    src/vpn/VpnBridge.h
    src/vpn/VpnBridge.cpp
    src/vpn/TunManager.h
    src/vpn/TunManager.cpp
    src/vpn/DnsManager.h
    src/vpn/DnsManager.cpp
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
    src/api/dto/Task.h
    src/api/TaskWatchWebSocket.h
    src/api/TaskWatchWebSocket.cpp
    src/models/TaskModel.h
    src/models/TaskModel.cpp
    src/webview/AppBrowserWidget.h
    src/webview/AppBrowserWidget.cpp
    src/util/UniqueFd.h
    src/dlp/DlpCompositorWidget.h
    src/dlp/DlpCompositorWidget.cpp
    src/updater/AutoUpdater.h
    src/updater/AutoUpdater.cpp
)

qt_add_qml_module(coder-desktop
    URI CoderDesktop
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/LoginPage.qml
        qml/WorkspacesPage.qml
        qml/VpnPage.qml
        qml/DeploymentPage.qml
        qml/SettingsPage.qml
        qml/AppBrowserPage.qml
        qml/TasksPage.qml
        qml/TaskDetailPage.qml
        qml/DlpPage.qml
)

target_include_directories(coder-desktop PRIVATE src)
target_compile_definitions(coder-desktop PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(coder-desktop PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2
    Qt6::Network Qt6::Widgets
    codervpn coderdlp
)

if(Qt6WebSockets_FOUND)
    target_link_libraries(coder-desktop PRIVATE Qt6::WebSockets)
    target_compile_definitions(coder-desktop PRIVATE HAS_WEBSOCKETS)
endif()

if(Qt6WebEngineQuick_FOUND)
    target_link_libraries(coder-desktop PRIVATE Qt6::WebEngineQuick)
    target_compile_definitions(coder-desktop PRIVATE HAS_WEBENGINE)
endif()

if(LIBSECRET_FOUND)
    target_link_libraries(coder-desktop PRIVATE ${LIBSECRET_LIBRARIES})
    target_include_directories(coder-desktop PRIVATE ${LIBSECRET_INCLUDE_DIRS})
    target_compile_definitions(coder-desktop PRIVATE HAS_LIBSECRET)
endif()

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
enable_testing()

add_executable(tst_dnsmanager
    tests/unit/tst_dnsmanager.cpp
    src/vpn/DnsManager.cpp
)
target_include_directories(tst_dnsmanager PRIVATE src)
target_link_libraries(tst_dnsmanager PRIVATE Qt6::Core Qt6::Test)
add_test(NAME DnsManagerTest COMMAND tst_dnsmanager)

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

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS coder-desktop
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT runtime
)
