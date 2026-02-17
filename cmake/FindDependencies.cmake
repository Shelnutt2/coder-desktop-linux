# cmake/FindDependencies.cmake — Centralized dependency detection
#
# All external dependencies are system-installed packages. We use:
#   - find_package()       for CMake-native packages (Qt 6)
#   - pkg_check_modules()  for pkg-config packages (wlroots, wayland, libsecret)
#   - find_program()       for the Go toolchain
#
# Why system packages (not vcpkg / FetchContent / ExternalProject)?
#   - wlroots links against system Wayland/EGL/DRM; building from source
#     would pull in its entire dependency tree.
#   - Qt 6 is ~2 GB of source — always a distro package.
#   - libsecret depends on D-Bus and the system keyring daemon.
#   - Go is a separate toolchain, not a C/C++ library.
#   - All target distros ship these deps in their repos.
#   - The packaging configs (deb/rpm) declare the correct Build-Depends.

include(CMakePrintHelpers)
find_package(PkgConfig REQUIRED)

# ── Qt 6 (required) ──────────────────────────────────────────────────────
find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core Gui Quick QuickControls2 Network Widgets DBus Test)

# Optional Qt components — available on some distros/configurations.
find_package(Qt6 COMPONENTS WebSockets QUIET)
find_package(Qt6 COMPONENTS WebEngineQuick QUIET)
find_package(Qt6 COMPONENTS WebChannel QUIET)

cmake_print_variables(Qt6_VERSION Qt6WebSockets_FOUND Qt6WebEngineQuick_FOUND Qt6WebChannel_FOUND)

# ── wlroots + Wayland (required for DLP compositor) ──────────────────────
pkg_check_modules(WLROOTS REQUIRED wlroots-0.19)
pkg_check_modules(WAYLAND REQUIRED wayland-server)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
pkg_get_variable(WAYLAND_SCANNER_BIN wayland-scanner wayland_scanner)

message(STATUS "wlroots: ${WLROOTS_VERSION}")
message(STATUS "Wayland protocols dir: ${WAYLAND_PROTOCOLS_DIR}")
message(STATUS "wayland-scanner: ${WAYLAND_SCANNER_BIN}")

# ── libsecret (optional — credential storage) ────────────────────────────
pkg_check_modules(LIBSECRET libsecret-1)
if(LIBSECRET_FOUND)
    message(STATUS "libsecret: ${LIBSECRET_VERSION}")
else()
    message(STATUS "libsecret not found — credential storage will use file fallback")
endif()

# ── Go toolchain (for coder-desktop-helper) ──────────────────────────────
find_program(GO_EXECUTABLE go)
if(GO_EXECUTABLE)
    execute_process(
        COMMAND ${GO_EXECUTABLE} version
        OUTPUT_VARIABLE GO_VERSION_STRING
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    message(STATUS "Go: ${GO_VERSION_STRING}")
else()
    message(STATUS "Go not found — coder-desktop-helper must be built separately "
                   "(cd coder-vpn-linux && go build ./cmd/coder-desktop-helper/)")
endif()
