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

# Private library directory for bundled .so files
set(CODER_LIB_DIR "${CMAKE_INSTALL_LIBDIR}/coder-desktop" CACHE PATH
    "Install directory for private shared libraries" FORCE)

# ---------------------------------------------------------------------------
# Centralized dependency detection
# ---------------------------------------------------------------------------
include(cmake/FindDependencies.cmake)

# ---------------------------------------------------------------------------
# Target 1: libcodervpn.so (Go c-shared) — built via custom command
# ---------------------------------------------------------------------------
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

# ---------------------------------------------------------------------------
# Target 2: libcoderdlp.so (C / wlroots)
# ---------------------------------------------------------------------------
add_subdirectory(coder-dlp-compositor)

# ---------------------------------------------------------------------------
# Target 3: coder-desktop (Qt / C++)
# ---------------------------------------------------------------------------
add_subdirectory(app)

# ---------------------------------------------------------------------------
# Install targets
# ---------------------------------------------------------------------------

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
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS
        "libwlroots12 (>= 0.19) | libwlroots-0.19-0")
endif()

# RPM (if rpmbuild is available)
find_program(RPMBUILD_PROG rpmbuild)
if(RPMBUILD_PROG)
    list(APPEND CPACK_GENERATOR "RPM")
    set(CPACK_RPM_PACKAGE_LICENSE "AGPL-3.0-only")
    set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
    set(CPACK_RPM_PACKAGE_REQUIRES
        "qt6-qtbase >= 6.5, qt6-qtdeclarative >= 6.5, libsecret")
    set(CPACK_RPM_PACKAGE_RECOMMENDS "wlroots >= 0.19")
endif()

include(CPack)
