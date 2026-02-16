cmake_minimum_required(VERSION 3.16)
project(coder-dlp-compositor C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WLROOTS REQUIRED wlroots-0.19)
pkg_check_modules(WAYLAND REQUIRED wayland-server)

add_library(coderdlp SHARED
    src/compositor.c
    src/shell.c
    src/clipboard.c
    src/security_context.c
    src/dlp_policy.c
    src/sandbox_launcher.c
    src/output.c
)
target_include_directories(coderdlp
    PUBLIC include
    PRIVATE ${WLROOTS_INCLUDE_DIRS} ${WAYLAND_INCLUDE_DIRS}
)
target_link_libraries(coderdlp PRIVATE ${WLROOTS_LIBRARIES} ${WAYLAND_LIBRARIES})
target_compile_definitions(coderdlp PRIVATE WLR_USE_UNSTABLE)

# Tests
enable_testing()
add_executable(test_dlp_policy tests/test_policy.c)
target_link_libraries(test_dlp_policy PRIVATE coderdlp)
add_test(NAME DlpPolicyTest COMMAND test_dlp_policy)
