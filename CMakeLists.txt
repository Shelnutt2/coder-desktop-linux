cmake_minimum_required(VERSION 3.21)
project(coder-desktop-linux VERSION 0.1.0 LANGUAGES C CXX)

# Target 1: libcodervpn.so (Go c-shared) — built via custom command
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/libcodervpn.so ${CMAKE_BINARY_DIR}/libcodervpn.h
    COMMAND ${CMAKE_SOURCE_DIR}/coder-vpn-linux/scripts/build-so.sh
            ${CMAKE_BINARY_DIR}/libcodervpn.so
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/coder-vpn-linux
    DEPENDS coder-vpn-linux/bridge.go coder-vpn-linux/go.mod
    COMMENT "Building Go VPN shared library"
)
add_custom_target(codervpn_so DEPENDS ${CMAKE_BINARY_DIR}/libcodervpn.so)

add_library(codervpn SHARED IMPORTED)
set_target_properties(codervpn PROPERTIES
    IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/libcodervpn.so
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR}
)
add_dependencies(codervpn codervpn_so)

# Target 2: libcoderdlp.so (C / wlroots)
add_subdirectory(coder-dlp-compositor)

# Target 3: coder-desktop (Qt / C++)
add_subdirectory(app)

# ---------------------------------------------------------------------------
# Install targets
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

# Private library directory for bundled .so files
set(CODER_LIB_DIR "${CMAKE_INSTALL_LIBDIR}/coder-desktop")

# Install the Go VPN shared library (built via custom command)
install(FILES ${CMAKE_BINARY_DIR}/libcodervpn.so
    DESTINATION "${CODER_LIB_DIR}"
    COMPONENT runtime
)

# Desktop file and icon
install(FILES packaging/resources/com.coder.CoderDesktop.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    COMPONENT runtime
)
install(FILES packaging/resources/coder-desktop.svg
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
    COMPONENT runtime
)

# AppStream metainfo (optional, for software centres)
# install(FILES packaging/resources/com.coder.CoderDesktop.metainfo.xml
#     DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo"
#     COMPONENT runtime
# )
