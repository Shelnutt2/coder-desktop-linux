cmake_minimum_required(VERSION 3.21)
project(coder-desktop-linux VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Default install prefix to dist/ for local development.
# Override with: cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
# ---------------------------------------------------------------------------
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist"
        CACHE PATH "Install prefix (default: dist/)" FORCE)
endif()

include(GNUInstallDirs)

option(BUILD_TESTING "Build unit tests" ON)
option(ENABLE_DLP "Build the DLP compositor (requires wlroots-0.19)" ON)

# wlroots build options (only relevant when ENABLE_DLP=ON)
set(WLROOTS_FROM_SOURCE "AUTO" CACHE STRING
    "Build wlroots from source: AUTO (try system, fallback to build), ON, OFF")
set_property(CACHE WLROOTS_FROM_SOURCE PROPERTY STRINGS AUTO ON OFF)
set(WLROOTS_VERSION "0.19.0" CACHE STRING "wlroots version to build from source")
option(WLROOTS_X11_BACKEND "Enable wlroots X11 backend" ON)
option(WLROOTS_XWAYLAND "Enable Xwayland support in wlroots" ON)

# Private library directory for bundled .so files
set(CODER_LIB_DIR "${CMAKE_INSTALL_LIBDIR}/coder-desktop" CACHE PATH
    "Install directory for private shared libraries" FORCE)

# ---------------------------------------------------------------------------
# RPATH — ensure the installed binary finds private .so files.
# At install time the binary's RUNPATH is set to $ORIGIN/../<libdir>/coder-desktop
# so it works regardless of the chosen install prefix.
# ---------------------------------------------------------------------------
set(CMAKE_SKIP_BUILD_RPATH OFF)
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
set(CMAKE_INSTALL_RPATH "\$ORIGIN/../${CODER_LIB_DIR}")
set(CMAKE_INSTALL_RPATH_USE_LINK_LIBRARIES ON)

# ---------------------------------------------------------------------------
# Centralized dependency detection
# ---------------------------------------------------------------------------
include(cmake/FindDependencies.cmake)
include(cmake/FetchMutagen.cmake)

# ---------------------------------------------------------------------------
# Target 1: coder-desktop-helper (Go binary — D-Bus system service)
# ---------------------------------------------------------------------------
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/coder-desktop-helper
    COMMAND go build -o ${CMAKE_BINARY_DIR}/coder-desktop-helper
            ./cmd/coder-desktop-helper/
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/coder-vpn-linux
    DEPENDS coder-vpn-linux/cmd/coder-desktop-helper/main.go
    COMMENT "Building Go VPN helper binary"
)
add_custom_target(helper ALL DEPENDS ${CMAKE_BINARY_DIR}/coder-desktop-helper)

# ---------------------------------------------------------------------------
# Target 2: libcoderdlp.so (C / wlroots)
# ---------------------------------------------------------------------------
if(ENABLE_DLP)
    add_subdirectory(coder-dlp-compositor)
endif()

# ---------------------------------------------------------------------------
# Target 3: coder-desktop (Qt / C++)
# ---------------------------------------------------------------------------
add_subdirectory(app)

# ---------------------------------------------------------------------------
# Install targets
# ---------------------------------------------------------------------------

# Install the Go VPN helper binary
install(PROGRAMS ${CMAKE_BINARY_DIR}/coder-desktop-helper
    DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}"
    COMPONENT runtime
)

# D-Bus, Polkit, and systemd files MUST go to absolute system paths —
# the daemons only search /usr/share (or /etc), not ${CMAKE_INSTALL_PREFIX}/share.
# Packagers can override these with -DDBUS_SYSTEM_CONF_DIR=... etc.
set(DBUS_SYSTEM_CONF_DIR "/usr/share/dbus-1/system.d"
    CACHE PATH "D-Bus system bus policy directory")
set(DBUS_SYSTEM_SERVICES_DIR "/usr/share/dbus-1/system-services"
    CACHE PATH "D-Bus system bus service activation directory")
set(POLKIT_ACTIONS_DIR "/usr/share/polkit-1/actions"
    CACHE PATH "Polkit actions directory")
set(SYSTEMD_SYSTEM_UNIT_DIR "/usr/lib/systemd/system"
    CACHE PATH "systemd system unit directory")

# Compute the full helper binary path for service file templates.
set(HELPER_BINARY_PATH "${CMAKE_INSTALL_FULL_LIBEXECDIR}/coder-desktop-helper")

# Generate D-Bus activation and systemd unit files with the correct binary path.
configure_file(dbus/com.coder.Desktop.Helper.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/com.coder.Desktop.Helper.service" @ONLY)
configure_file(packaging/systemd/coder-desktop-helper.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/coder-desktop-helper.service" @ONLY)

install(FILES dbus/com.coder.Desktop.Helper.conf
    DESTINATION "${DBUS_SYSTEM_CONF_DIR}"
    COMPONENT runtime
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/com.coder.Desktop.Helper.service"
    DESTINATION "${DBUS_SYSTEM_SERVICES_DIR}"
    COMPONENT runtime
)

# Polkit policy (authorises desktop users to control the VPN helper)
install(FILES packaging/polkit/com.coder.Desktop.Helper.policy
    DESTINATION "${POLKIT_ACTIONS_DIR}"
    COMPONENT runtime
)

# systemd unit (optional — for distros that prefer systemd activation)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/coder-desktop-helper.service"
    DESTINATION "${SYSTEMD_SYSTEM_UNIT_DIR}"
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

# ---------------------------------------------------------------------------
# CPack — package generation (cmake --build build --target package)
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "coder-desktop")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Coder Desktop for Linux — Remote development workspace manager")
set(CPACK_PACKAGE_VENDOR "Coder Technologies Inc.")
set(CPACK_PACKAGE_CONTACT "support@coder.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")

# TGZ (always available)
set(CPACK_GENERATOR "TGZ")

# DEB (if dpkg-deb is available)
find_program(DPKG_PROG dpkg-deb)
if(DPKG_PROG)
    list(APPEND CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER
        "Coder Technologies Inc. <support@coder.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libqt6core6 (>= 6.5), libqt6gui6 (>= 6.5), libqt6quick6 (>= 6.5), libqt6network6 (>= 6.5), libqt6widgets6 (>= 6.5), libsecret-1-0")
    if(ENABLE_DLP AND NOT WLROOTS_BUILT_FROM_SOURCE)
        set(CPACK_DEBIAN_PACKAGE_RECOMMENDS
            "libwlroots12 (>= 0.19) | libwlroots-0.19-0")
    endif()
endif()

# RPM (if rpmbuild is available)
find_program(RPMBUILD_PROG rpmbuild)
if(RPMBUILD_PROG)
    list(APPEND CPACK_GENERATOR "RPM")
    set(CPACK_RPM_PACKAGE_LICENSE "AGPL-3.0-only")
    set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
    set(CPACK_RPM_PACKAGE_REQUIRES
        "qt6-qtbase >= 6.5, qt6-qtdeclarative >= 6.5, libsecret")
    if(ENABLE_DLP AND NOT WLROOTS_BUILT_FROM_SOURCE)
        set(CPACK_RPM_PACKAGE_RECOMMENDS "wlroots >= 0.19")
    endif()
endif()

include(CPack)
