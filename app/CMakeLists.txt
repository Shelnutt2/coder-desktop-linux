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
)

qt_add_qml_module(coder-desktop
    URI CoderDesktop
    VERSION 1.0
    QML_FILES
        qml/main.qml
)

target_include_directories(coder-desktop PRIVATE src)
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
