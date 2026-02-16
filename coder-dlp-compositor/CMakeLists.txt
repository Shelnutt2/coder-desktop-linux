cmake_minimum_required(VERSION 3.16)
project(coder-dlp-compositor C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WLROOTS REQUIRED wlroots-0.19)
pkg_check_modules(WAYLAND REQUIRED wayland-server)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
pkg_get_variable(WAYLAND_SCANNER_BIN wayland-scanner wayland_scanner)

# Generate xdg-shell protocol header (required by wlroots xdg_shell.h)
set(XDG_SHELL_XML "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
set(XDG_SHELL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.h")
add_custom_command(
    OUTPUT "${XDG_SHELL_HEADER}"
    COMMAND "${WAYLAND_SCANNER_BIN}" server-header "${XDG_SHELL_XML}" "${XDG_SHELL_HEADER}"
    DEPENDS "${XDG_SHELL_XML}"
    COMMENT "Generating xdg-shell-protocol.h"
)
add_custom_target(xdg_shell_protocol DEPENDS "${XDG_SHELL_HEADER}")

add_library(coderdlp SHARED
    src/compositor.c
    src/shell.c
    src/clipboard.c
    src/security_context.c
    src/dlp_policy.c
    src/sandbox_launcher.c
    src/output.c
)
add_dependencies(coderdlp xdg_shell_protocol)
target_include_directories(coderdlp
    PUBLIC include
    PRIVATE ${WLROOTS_INCLUDE_DIRS} ${WAYLAND_INCLUDE_DIRS}
            ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(coderdlp PRIVATE ${WLROOTS_LIBRARIES} ${WAYLAND_LIBRARIES})
target_compile_definitions(coderdlp PRIVATE WLR_USE_UNSTABLE)

# Tests
enable_testing()
add_executable(test_dlp_policy tests/test_policy.c)
target_include_directories(test_dlp_policy PRIVATE
    ${WLROOTS_INCLUDE_DIRS} ${WAYLAND_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_compile_definitions(test_dlp_policy PRIVATE WLR_USE_UNSTABLE)
target_link_libraries(test_dlp_policy PRIVATE coderdlp)
add_test(NAME DlpPolicyTest COMMAND test_dlp_policy)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
set(CODER_LIB_DIR "${CMAKE_INSTALL_LIBDIR}/coder-desktop")
install(TARGETS coderdlp
    LIBRARY DESTINATION "${CODER_LIB_DIR}"
    COMPONENT runtime
)
