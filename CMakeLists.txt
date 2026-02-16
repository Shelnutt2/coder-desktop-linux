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
