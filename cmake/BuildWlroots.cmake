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
    # Explicitly list backends without x11
    set(_wlr_backends "drm,libinput,wayland")
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

    BUILD_BYPRODUCTS
        "${WLROOTS_INSTALL_PREFIX}/${_wlr_libdir}/libwlroots-0.19.a"
)

# Set variables matching what pkg_check_modules would produce.
# Include the wlroots install prefix AND all transitive dependency include dirs
# (wlroots headers like wlr/types/wlr_output.h include <pixman.h>, <wayland-server.h>, etc.)
set(WLROOTS_INCLUDE_DIRS "${WLROOTS_INSTALL_PREFIX}/include/wlroots-0.19")
set(WLROOTS_VERSION "${WLROOTS_VERSION}")
set(WLROOTS_BUILT_FROM_SOURCE TRUE)
set(WLROOTS_STATIC_LIB "${WLROOTS_INSTALL_PREFIX}/${_wlr_libdir}/libwlroots-0.19.a")
set(WLROOTS_EXTERNAL_TARGET "wlroots_external")

# Transitive deps — these are the same system libs that wlroots links against.
# Found via pkg-config at configure time (they must be installed as build deps).
pkg_check_modules(_WLR_WAYLAND REQUIRED wayland-server)
pkg_check_modules(_WLR_DRM REQUIRED libdrm)
pkg_check_modules(_WLR_GBM REQUIRED gbm)
pkg_check_modules(_WLR_PIXMAN REQUIRED pixman-1)
pkg_check_modules(_WLR_XKBCOMMON REQUIRED xkbcommon)
pkg_check_modules(_WLR_EGL REQUIRED egl)
pkg_check_modules(_WLR_GLES REQUIRED glesv2)
pkg_check_modules(_WLR_SEAT QUIET libseat)
pkg_check_modules(_WLR_INPUT QUIET libinput)
pkg_check_modules(_WLR_DISPLAY_INFO QUIET libdisplay-info)
pkg_check_modules(_WLR_LIFTOFF QUIET libliftoff)

# Append transitive dependency include dirs so that wlroots public headers
# (which #include <pixman.h>, <wayland-server-core.h>, <drm_fourcc.h>, etc.)
# can find them.
list(APPEND WLROOTS_INCLUDE_DIRS
    ${_WLR_WAYLAND_INCLUDE_DIRS}
    ${_WLR_DRM_INCLUDE_DIRS}
    ${_WLR_GBM_INCLUDE_DIRS}
    ${_WLR_PIXMAN_INCLUDE_DIRS}
    ${_WLR_XKBCOMMON_INCLUDE_DIRS}
    ${_WLR_EGL_INCLUDE_DIRS}
    ${_WLR_GLES_INCLUDE_DIRS}
    ${_WLR_SEAT_INCLUDE_DIRS}
    ${_WLR_INPUT_INCLUDE_DIRS}
    ${_WLR_DISPLAY_INFO_INCLUDE_DIRS}
    ${_WLR_LIFTOFF_INCLUDE_DIRS}
)
list(REMOVE_DUPLICATES WLROOTS_INCLUDE_DIRS)

set(WLROOTS_LIBRARIES
    "${WLROOTS_INSTALL_PREFIX}/${_wlr_libdir}/libwlroots-0.19.a"
    ${_WLR_WAYLAND_LIBRARIES}
    ${_WLR_DRM_LIBRARIES}
    ${_WLR_GBM_LIBRARIES}
    ${_WLR_PIXMAN_LIBRARIES}
    ${_WLR_XKBCOMMON_LIBRARIES}
    ${_WLR_EGL_LIBRARIES}
    ${_WLR_GLES_LIBRARIES}
    ${_WLR_SEAT_LIBRARIES}
    ${_WLR_INPUT_LIBRARIES}
    ${_WLR_DISPLAY_INFO_LIBRARIES}
    ${_WLR_LIFTOFF_LIBRARIES}
    m
)
