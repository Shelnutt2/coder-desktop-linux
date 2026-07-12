# cmake/BuildWlroots.cmake — Build wlroots from source via ExternalProject
#
# Sets the same variables that pkg_check_modules would:
#   WLROOTS_INCLUDE_DIRS, WLROOTS_LIBRARIES, WLROOTS_VERSION
# Also sets WLROOTS_BUILT_FROM_SOURCE=TRUE and the ExternalProject target name.

include(ExternalProject)

find_program(MESON_EXECUTABLE meson REQUIRED)
find_program(NINJA_EXECUTABLE ninja REQUIRED)

set(WLROOTS_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/_deps/wlroots-install")

# Map CMake options to meson -D flags
# Build meson -Dbackends value
if(WLROOTS_X11_BACKEND)
    set(_wlr_backends "auto")
else()
    # Explicitly list backends without x11. The wayland backend is always
    # built and is not a valid value for the meson backends option.
    set(_wlr_backends "drm,libinput")
endif()
if(WLROOTS_XWAYLAND)
    set(_wlr_xwayland "enabled")
else()
    set(_wlr_xwayland "disabled")
endif()

# Determine lib dir (some distros use lib64)
if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND EXISTS "/usr/lib64")
    set(_wlr_libdir "lib64")
else()
    set(_wlr_libdir "lib")
endif()

# Determine build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_wlr_buildtype "debug")
else()
    set(_wlr_buildtype "release")
endif()

# Post-install script: merge all .a files (wlroots + any meson subprojects like
# pixman, libliftoff, etc.) into a single combined archive.  This avoids needing
# to know which subprojects meson decided to build.
set(_wlr_merge_script "${CMAKE_BINARY_DIR}/_deps/merge-wlroots-archives.sh")
set(_wlr_merged_lib "${WLROOTS_INSTALL_PREFIX}/${_wlr_libdir}/libwlroots-merged.a")
file(WRITE "${_wlr_merge_script}"
"#!/bin/sh
set -e
LIBDIR=\"${WLROOTS_INSTALL_PREFIX}/${_wlr_libdir}\"
MERGED=\"${_wlr_merged_lib}\"
TMPDIR=\"$LIBDIR/_merge_tmp\"
rm -rf \"$TMPDIR\" \"$MERGED\"
mkdir -p \"$TMPDIR\"
# Extract all .o files from every .a in the lib dir
for archive in \"$LIBDIR\"/*.a; do
    [ -f \"$archive\" ] || continue
    cd \"$TMPDIR\"
    ar x \"$archive\"
done
# Create merged archive
ar crs \"$MERGED\" \"$TMPDIR\"/*.o
rm -rf \"$TMPDIR\"
echo \"Merged wlroots archives into $MERGED\"
")

ExternalProject_Add(wlroots_external
    GIT_REPOSITORY https://gitlab.freedesktop.org/wlroots/wlroots.git
    GIT_TAG        "${WLROOTS_VERSION}"
    GIT_SHALLOW    TRUE

    CONFIGURE_COMMAND ${MESON_EXECUTABLE} setup
        <BINARY_DIR>/meson-build <SOURCE_DIR>
        --prefix=${WLROOTS_INSTALL_PREFIX}
        --libdir=${_wlr_libdir}
        --buildtype=${_wlr_buildtype}
        -Ddefault_library=static
        -Db_staticpic=true
        -Dexamples=false
        -Dxwayland=${_wlr_xwayland}
        -Dbackends=${_wlr_backends}
        -Drenderers=auto
        --wrap-mode=default

    BUILD_COMMAND ${NINJA_EXECUTABLE} -C <BINARY_DIR>/meson-build
    INSTALL_COMMAND ${NINJA_EXECUTABLE} -C <BINARY_DIR>/meson-build install
        COMMAND sh "${_wlr_merge_script}"

    BUILD_BYPRODUCTS
        "${_wlr_merged_lib}"
)

# Set variables matching what pkg_check_modules would produce.
# Include the wlroots install prefix AND all transitive dependency include dirs
# (wlroots headers like wlr/types/wlr_output.h include <pixman.h>, <wayland-server.h>, etc.)
set(WLROOTS_INCLUDE_DIRS "${WLROOTS_INSTALL_PREFIX}/include/wlroots-0.19")
set(WLROOTS_VERSION "${WLROOTS_VERSION}")
set(WLROOTS_BUILT_FROM_SOURCE TRUE)
set(WLROOTS_STATIC_LIB "${_wlr_merged_lib}")
set(WLROOTS_EXTERNAL_TARGET "wlroots_external")

# Transitive deps — system shared libraries that wlroots (and any meson
# subprojects) link against at runtime.  The merged static archive contains all
# wlroots + subproject code, but still needs these system libs at link time.
#
# Core (always required):
pkg_check_modules(_WLR_WAYLAND_SERVER REQUIRED wayland-server)
pkg_check_modules(_WLR_WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(_WLR_DRM REQUIRED libdrm)
pkg_check_modules(_WLR_GBM REQUIRED gbm)
pkg_check_modules(_WLR_PIXMAN QUIET pixman-1)   # may be built as meson subproject
pkg_check_modules(_WLR_XKBCOMMON REQUIRED xkbcommon)
pkg_check_modules(_WLR_EGL REQUIRED egl)
pkg_check_modules(_WLR_GLES REQUIRED glesv2)
# Optional (present on most desktop systems):
pkg_check_modules(_WLR_VULKAN QUIET vulkan)
pkg_check_modules(_WLR_SEAT QUIET libseat)
pkg_check_modules(_WLR_INPUT QUIET libinput)
pkg_check_modules(_WLR_DISPLAY_INFO QUIET libdisplay-info)
pkg_check_modules(_WLR_LIFTOFF QUIET libliftoff)
pkg_check_modules(_WLR_LCMS2 QUIET lcms2)
# XCB extensions (used by wlroots X11 backend and Xwayland support):
pkg_check_modules(_WLR_XCB QUIET xcb)
pkg_check_modules(_WLR_XCB_RENDER_UTIL QUIET xcb-renderutil)
pkg_check_modules(_WLR_XCB_RENDER QUIET xcb-render)
pkg_check_modules(_WLR_XCB_XFIXES QUIET xcb-xfixes)
pkg_check_modules(_WLR_XCB_COMPOSITE QUIET xcb-composite)
pkg_check_modules(_WLR_XCB_PRESENT QUIET xcb-present)
pkg_check_modules(_WLR_XCB_XINPUT QUIET xcb-xinput)
pkg_check_modules(_WLR_XCB_SHM QUIET xcb-shm)
pkg_check_modules(_WLR_XCB_RES QUIET xcb-res)
pkg_check_modules(_WLR_XCB_DRI3 QUIET xcb-dri3)
pkg_check_modules(_WLR_XCB_SHAPE QUIET xcb-shape)
pkg_check_modules(_WLR_XCB_EWMH QUIET xcb-ewmh)
pkg_check_modules(_WLR_XCB_ICCCM QUIET xcb-icccm)

# Append transitive dependency include dirs so that wlroots public headers
# (which #include <pixman.h>, <wayland-server-core.h>, <drm_fourcc.h>, etc.)
# can find them.  Also include the wlroots install prefix for headers installed
# by meson subprojects (e.g. pixman built from source).
list(APPEND WLROOTS_INCLUDE_DIRS
    "${WLROOTS_INSTALL_PREFIX}/include"
    ${_WLR_WAYLAND_SERVER_INCLUDE_DIRS}
    ${_WLR_WAYLAND_CLIENT_INCLUDE_DIRS}
    ${_WLR_DRM_INCLUDE_DIRS}
    ${_WLR_GBM_INCLUDE_DIRS}
    ${_WLR_PIXMAN_INCLUDE_DIRS}
    ${_WLR_XKBCOMMON_INCLUDE_DIRS}
    ${_WLR_EGL_INCLUDE_DIRS}
    ${_WLR_GLES_INCLUDE_DIRS}
    ${_WLR_VULKAN_INCLUDE_DIRS}
    ${_WLR_SEAT_INCLUDE_DIRS}
    ${_WLR_INPUT_INCLUDE_DIRS}
    ${_WLR_DISPLAY_INFO_INCLUDE_DIRS}
    ${_WLR_LIFTOFF_INCLUDE_DIRS}
    ${_WLR_LCMS2_INCLUDE_DIRS}
)
list(REMOVE_DUPLICATES WLROOTS_INCLUDE_DIRS)

# Link the merged archive (wlroots + all subproject .a files combined) plus
# system shared libraries for remaining transitive deps.
set(WLROOTS_LIBRARIES
    "${_wlr_merged_lib}"
    ${_WLR_WAYLAND_SERVER_LIBRARIES}
    ${_WLR_WAYLAND_CLIENT_LIBRARIES}
    ${_WLR_DRM_LIBRARIES}
    ${_WLR_GBM_LIBRARIES}
    ${_WLR_PIXMAN_LIBRARIES}
    ${_WLR_XKBCOMMON_LIBRARIES}
    ${_WLR_EGL_LIBRARIES}
    ${_WLR_GLES_LIBRARIES}
    ${_WLR_VULKAN_LIBRARIES}
    ${_WLR_SEAT_LIBRARIES}
    ${_WLR_INPUT_LIBRARIES}
    ${_WLR_DISPLAY_INFO_LIBRARIES}
    ${_WLR_LIFTOFF_LIBRARIES}
    ${_WLR_LCMS2_LIBRARIES}
    ${_WLR_XCB_LIBRARIES}
    ${_WLR_XCB_RENDER_UTIL_LIBRARIES}
    ${_WLR_XCB_RENDER_LIBRARIES}
    ${_WLR_XCB_XFIXES_LIBRARIES}
    ${_WLR_XCB_COMPOSITE_LIBRARIES}
    ${_WLR_XCB_PRESENT_LIBRARIES}
    ${_WLR_XCB_XINPUT_LIBRARIES}
    ${_WLR_XCB_SHM_LIBRARIES}
    ${_WLR_XCB_RES_LIBRARIES}
    ${_WLR_XCB_DRI3_LIBRARIES}
    ${_WLR_XCB_SHAPE_LIBRARIES}
    ${_WLR_XCB_EWMH_LIBRARIES}
    ${_WLR_XCB_ICCCM_LIBRARIES}
    m
)
