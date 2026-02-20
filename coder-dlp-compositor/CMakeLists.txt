cmake_minimum_required(VERSION 3.16)
project(coder-dlp-compositor C)

# Dependencies (wlroots, wayland, wayland-protocols) are found centrally in
# cmake/FindDependencies.cmake — variables are inherited from the parent scope.

# xkbcommon is needed for keyboard XKB keymap setup in input.c.
pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)

# Optional X11 backend support — wlroots may be compiled with WLR_HAS_X11_BACKEND
include(CheckSymbolExists)
set(CMAKE_REQUIRED_INCLUDES ${WLROOTS_INCLUDE_DIRS})
check_symbol_exists(WLR_HAS_X11_BACKEND "wlr/config.h" HAS_X11_BACKEND)
check_symbol_exists(WLR_HAS_XWAYLAND "wlr/config.h" HAS_XWAYLAND)
if(HAS_X11_BACKEND)
    pkg_check_modules(XCB REQUIRED xcb)
endif()
if(HAS_XWAYLAND)
    # Xwayland support — xcb-ewmh and xcb-icccm are needed by wlr/xwayland/xwayland.h
    pkg_check_modules(XCB_EWMH REQUIRED xcb-ewmh)
    pkg_check_modules(XCB_ICCCM REQUIRED xcb-icccm)
endif()

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

# Generate cursor-shape-v1 protocol header (required by wlroots cursor_shape_v1.h)
set(CURSOR_SHAPE_XML "${WAYLAND_PROTOCOLS_DIR}/staging/cursor-shape/cursor-shape-v1.xml")
set(CURSOR_SHAPE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/cursor-shape-v1-protocol.h")
add_custom_command(
    OUTPUT "${CURSOR_SHAPE_HEADER}"
    COMMAND "${WAYLAND_SCANNER_BIN}" server-header "${CURSOR_SHAPE_XML}" "${CURSOR_SHAPE_HEADER}"
    DEPENDS "${CURSOR_SHAPE_XML}"
    COMMENT "Generating cursor-shape-v1-protocol.h"
)
add_custom_target(cursor_shape_protocol DEPENDS "${CURSOR_SHAPE_HEADER}")

set(CODERDLP_SOURCES
    src/compositor.c
    src/shell.c
    src/clipboard.c
    src/security_context.c
    src/dlp_policy.c
    src/sandbox_launcher.c
    src/output.c
    src/input.c
    src/watermark.c
)
if(HAS_XWAYLAND)
    list(APPEND CODERDLP_SOURCES src/xwayland.c)
endif()

add_library(coderdlp SHARED ${CODERDLP_SOURCES})
add_dependencies(coderdlp xdg_shell_protocol cursor_shape_protocol)
target_include_directories(coderdlp
    PUBLIC include
    PRIVATE ${WLROOTS_INCLUDE_DIRS} ${WAYLAND_INCLUDE_DIRS} ${XKBCOMMON_INCLUDE_DIRS}
            ${XCB_INCLUDE_DIRS} ${XCB_EWMH_INCLUDE_DIRS} ${XCB_ICCCM_INCLUDE_DIRS}
            ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(coderdlp PRIVATE ${WLROOTS_LIBRARIES} ${WAYLAND_LIBRARIES} ${XKBCOMMON_LIBRARIES}
    ${XCB_LIBRARIES} ${XCB_EWMH_LIBRARIES} ${XCB_ICCCM_LIBRARIES})
target_compile_definitions(coderdlp PRIVATE WLR_USE_UNSTABLE)

# Tests  (disable with -DBUILD_TESTING=OFF at top level)
if(BUILD_TESTING)
    enable_testing()
    add_executable(test_dlp_policy tests/test_policy.c)
    target_include_directories(test_dlp_policy PRIVATE
        ${WLROOTS_INCLUDE_DIRS} ${WAYLAND_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    target_compile_definitions(test_dlp_policy PRIVATE WLR_USE_UNSTABLE)
    target_link_libraries(test_dlp_policy PRIVATE coderdlp)
    add_test(NAME DlpPolicyTest COMMAND test_dlp_policy)
endif()

# ---------------------------------------------------------------------------
# Install  (GNUInstallDirs + CODER_LIB_DIR set at top level)
# ---------------------------------------------------------------------------
install(TARGETS coderdlp
    LIBRARY DESTINATION "${CODER_LIB_DIR}"
    COMPONENT runtime
)
