#include "coder_dlp.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Expose internals for testing */
#include "../src/compositor_internal.h"

static void test_set_policy(void) {
    /* Allocate the struct directly to test policy logic without needing
     * a running Wayland display (coder_dlp_create requires one). */
    coder_dlp_compositor* comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);

    coder_dlp_policy policy;
    memset(&policy, 0, sizeof(policy));
    policy.clipboard_block_outgoing = true;
    policy.clipboard_block_incoming = false;
    policy.screenshot_block = true;
    policy.file_sandbox = true;
    policy.network_sandbox = false;

    coder_dlp_set_policy(comp, &policy);

    assert(comp->policy.clipboard_block_outgoing == true);
    assert(comp->policy.clipboard_block_incoming == false);
    assert(comp->policy.screenshot_block == true);
    assert(comp->policy.file_sandbox == true);
    assert(comp->policy.network_sandbox == false);

    free(comp);
    printf("test_set_policy: PASSED\n");
}

static void test_null_safety(void) {
    /* These should not crash */
    coder_dlp_set_policy(NULL, NULL);
    coder_dlp_destroy(NULL);
    coder_dlp_dispatch(NULL);
    assert(coder_dlp_get_fd(NULL) == -1);

    printf("test_null_safety: PASSED\n");
}

static void test_sandbox_launch_null_safety(void) {
    /* NULL compositor and/or command must return -1 without crashing. */
    assert(coder_dlp_launch_app(NULL, NULL, NULL) == -1);
    assert(coder_dlp_launch_app(NULL, "echo hello", NULL) == -1);

    /* Non-NULL comp but NULL command */
    coder_dlp_compositor* comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);
    assert(coder_dlp_launch_app(comp, NULL, NULL) == -1);
    free(comp);

    printf("test_sandbox_launch_null_safety: PASSED\n");
}

static void test_bwrap_args_basic(void) {
    /* Verify the argv array built by dlp_build_bwrap_args contains the
     * expected entries for a minimal configuration. */
    coder_dlp_compositor* comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);
    comp->socket = "wayland-test";

    char** argv = dlp_build_bwrap_args(comp, "echo hello", NULL, NULL);
    assert(argv != NULL);

    /* First arg must be "bwrap" */
    assert(strcmp(argv[0], "bwrap") == 0);

    /* Must contain --bind / / (non-isolated mode, sandbox=NULL) */
    int found_bind_root = 0;
    int found_wayland = 0;
    int found_unsetenv_display = 0;
    int found_command = 0;
    for (int i = 0; argv[i]; i++) {
        if (strcmp(argv[i], "--bind") == 0 && argv[i + 1] && strcmp(argv[i + 1], "/") == 0)
            found_bind_root = 1;
        if (strcmp(argv[i], "WAYLAND_DISPLAY") == 0 && i > 0 &&
            strcmp(argv[i - 1], "--setenv") == 0) {
            assert(strcmp(argv[i + 1], "wayland-test") == 0);
            found_wayland = 1;
        }
        if (strcmp(argv[i], "DISPLAY") == 0 && i > 0 && strcmp(argv[i - 1], "--unsetenv") == 0)
            found_unsetenv_display = 1;
        if (strcmp(argv[i], "echo hello") == 0) found_command = 1;
    }
    assert(found_bind_root);
    assert(found_wayland);
    assert(found_unsetenv_display);
    assert(found_command);

    dlp_free_bwrap_args(argv);
    free(comp);
    printf("test_bwrap_args_basic: PASSED\n");
}

static void test_bwrap_args_sandbox_options(void) {
    /* Verify sandbox config options appear in argv. */
    coder_dlp_compositor* comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);
    comp->socket = "wayland-test";

    coder_dlp_sandbox_config sandbox;
    memset(&sandbox, 0, sizeof(sandbox));
    sandbox.workspace_path = "/home/user/workspace";
    sandbox.isolate_filesystem = true;
    sandbox.isolate_pid = true;
    sandbox.isolate_ipc = true;
    sandbox.network_namespace = "vpn0";

    char** argv = dlp_build_bwrap_args(comp, "ls", &sandbox, NULL);
    assert(argv != NULL);

    int found_bind_ws = 0, found_pid = 0, found_ipc = 0, found_net = 0;
    for (int i = 0; argv[i]; i++) {
        if (strcmp(argv[i], "--bind") == 0 && argv[i + 1] &&
            strcmp(argv[i + 1], "/home/user/workspace") == 0)
            found_bind_ws = 1;
        if (strcmp(argv[i], "--unshare-pid") == 0) found_pid = 1;
        if (strcmp(argv[i], "--unshare-ipc") == 0) found_ipc = 1;
        if (strcmp(argv[i], "--unshare-net") == 0) found_net = 1;
    }
    assert(found_bind_ws);
    assert(found_pid);
    assert(found_ipc);
    assert(found_net);

    dlp_free_bwrap_args(argv);
    free(comp);
    printf("test_bwrap_args_sandbox_options: PASSED\n");
}

static void test_bwrap_args_no_fs_isolation(void) {
    /* When isolate_filesystem is false, the sandbox should use --bind / /
     * (read-write) and NOT include workspace_path bind (already accessible). */
    coder_dlp_compositor* comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);
    comp->socket = "wayland-test";

    coder_dlp_sandbox_config sandbox;
    memset(&sandbox, 0, sizeof(sandbox));
    sandbox.workspace_path = "/home/user/workspace";
    sandbox.isolate_filesystem = false;

    char** argv = dlp_build_bwrap_args(comp, "ls", &sandbox, NULL);
    assert(argv != NULL);

    int found_rw_bind_root = 0;
    int found_ro_bind_root = 0;
    int found_bind_ws = 0;
    for (int i = 0; argv[i]; i++) {
        if (strcmp(argv[i], "--bind") == 0 && argv[i + 1] && strcmp(argv[i + 1], "/") == 0)
            found_rw_bind_root = 1;
        if (strcmp(argv[i], "--ro-bind") == 0 && argv[i + 1] && strcmp(argv[i + 1], "/") == 0)
            found_ro_bind_root = 1;
        if (strcmp(argv[i], "--bind") == 0 && argv[i + 1] &&
            strcmp(argv[i + 1], "/home/user/workspace") == 0)
            found_bind_ws = 1;
    }
    /* Non-isolated: should have rw bind, not ro bind */
    assert(found_rw_bind_root);
    assert(!found_ro_bind_root);
    /* workspace_path bind is unnecessary when fs is not isolated */
    assert(!found_bind_ws);

    dlp_free_bwrap_args(argv);
    free(comp);
    printf("test_bwrap_args_no_fs_isolation: PASSED\n");
}

static void test_bwrap_args_bind_home_rw(void) {
    /* Verify bind_home_rw controls whether $HOME is bound rw or tmpfs'd. */
    setenv("HOME", "/home/testuser", 1);

    coder_dlp_compositor* comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);
    comp->socket = "wayland-test";

    /* Case 1: isolate_filesystem=true, bind_home_rw=false → tmpfs $HOME */
    {
        coder_dlp_sandbox_config sandbox;
        memset(&sandbox, 0, sizeof(sandbox));
        sandbox.isolate_filesystem = true;
        sandbox.bind_home_rw = false;

        char** argv = dlp_build_bwrap_args(comp, "echo hi", &sandbox, NULL);
        assert(argv != NULL);

        int found_ro_bind_root = 0, found_tmpfs_home = 0, found_bind_home = 0;
        for (int i = 0; argv[i]; i++) {
            if (strcmp(argv[i], "--ro-bind") == 0 && argv[i + 1] && strcmp(argv[i + 1], "/") == 0)
                found_ro_bind_root = 1;
            if (strcmp(argv[i], "--tmpfs") == 0 && argv[i + 1] &&
                strcmp(argv[i + 1], "/home/testuser") == 0)
                found_tmpfs_home = 1;
            if (strcmp(argv[i], "--bind") == 0 && argv[i + 1] &&
                strcmp(argv[i + 1], "/home/testuser") == 0)
                found_bind_home = 1;
        }
        assert(found_ro_bind_root);
        assert(found_tmpfs_home);
        assert(!found_bind_home);

        dlp_free_bwrap_args(argv);
    }

    /* Case 2: isolate_filesystem=true, bind_home_rw=true → --bind $HOME $HOME */
    {
        coder_dlp_sandbox_config sandbox;
        memset(&sandbox, 0, sizeof(sandbox));
        sandbox.isolate_filesystem = true;
        sandbox.bind_home_rw = true;

        char** argv = dlp_build_bwrap_args(comp, "echo hi", &sandbox, NULL);
        assert(argv != NULL);

        int found_ro_bind_root = 0, found_tmpfs_home = 0, found_bind_home = 0;
        for (int i = 0; argv[i]; i++) {
            if (strcmp(argv[i], "--ro-bind") == 0 && argv[i + 1] && strcmp(argv[i + 1], "/") == 0)
                found_ro_bind_root = 1;
            if (strcmp(argv[i], "--tmpfs") == 0 && argv[i + 1] &&
                strcmp(argv[i + 1], "/home/testuser") == 0)
                found_tmpfs_home = 1;
            if (strcmp(argv[i], "--bind") == 0 && argv[i + 1] &&
                strcmp(argv[i + 1], "/home/testuser") == 0)
                found_bind_home = 1;
        }
        assert(found_ro_bind_root);
        assert(!found_tmpfs_home);
        assert(found_bind_home);

        dlp_free_bwrap_args(argv);
    }

    free(comp);
    printf("test_bwrap_args_bind_home_rw: PASSED\n");
}

static void test_bwrap_args_extra_bind_paths(void) {
    /* Verify extra_bind_paths appear as --bind pairs in argv. */
    coder_dlp_compositor* comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);
    comp->socket = "wayland-test";

    const char* extra_paths[] = {"/opt/tools", "/data/project"};

    coder_dlp_sandbox_config sandbox;
    memset(&sandbox, 0, sizeof(sandbox));
    sandbox.extra_bind_paths = extra_paths;
    sandbox.extra_bind_count = 2;

    char** argv = dlp_build_bwrap_args(comp, "ls", &sandbox, NULL);
    assert(argv != NULL);

    int found_opt = 0, found_data = 0;
    for (int i = 0; argv[i]; i++) {
        if (strcmp(argv[i], "--bind") == 0 && argv[i + 1] && strcmp(argv[i + 1], "/opt/tools") == 0)
            found_opt = 1;
        if (strcmp(argv[i], "--bind") == 0 && argv[i + 1] &&
            strcmp(argv[i + 1], "/data/project") == 0)
            found_data = 1;
    }
    assert(found_opt);
    assert(found_data);

    dlp_free_bwrap_args(argv);
    free(comp);
    printf("test_bwrap_args_extra_bind_paths: PASSED\n");
}

static void test_bwrap_args_extra_bind_paths_null(void) {
    /* NULL extra_bind_paths with count=0 must not crash. */
    coder_dlp_compositor* comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);
    comp->socket = "wayland-test";

    coder_dlp_sandbox_config sandbox;
    memset(&sandbox, 0, sizeof(sandbox));
    sandbox.extra_bind_paths = NULL;
    sandbox.extra_bind_count = 0;

    char** argv = dlp_build_bwrap_args(comp, "ls", &sandbox, NULL);
    assert(argv != NULL);

    /* Must contain at least "bwrap" and the command */
    assert(strcmp(argv[0], "bwrap") == 0);

    dlp_free_bwrap_args(argv);
    free(comp);
    printf("test_bwrap_args_extra_bind_paths_null: PASSED\n");
}

static void test_bwrap_args_null_returns_null(void) {
    /* NULL comp or command must return NULL. */
    assert(dlp_build_bwrap_args(NULL, "echo", NULL, NULL) == NULL);

    coder_dlp_compositor* comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);
    assert(dlp_build_bwrap_args(comp, NULL, NULL, NULL) == NULL);
    free(comp);

    /* dlp_free_bwrap_args(NULL) must not crash */
    dlp_free_bwrap_args(NULL);

    printf("test_bwrap_args_null_returns_null: PASSED\n");
}

static void test_bwrap_args_dbus_proxy_socket(void) {
    /* When a proxy socket path is provided, bwrap args should bind it to the
     * standard XDG_RUNTIME_DIR/bus path instead of the real bus socket. */
    setenv("XDG_RUNTIME_DIR", "/run/user/1000", 1);

    coder_dlp_compositor* comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);
    comp->socket = "wayland-test";

    char** argv = dlp_build_bwrap_args(comp, "echo hi", NULL, "/tmp/coder-dlp-dbus-proxy");
    assert(argv != NULL);

    int found_proxy_bind = 0;
    int found_dbus_env = 0;
    for (int i = 0; argv[i]; i++) {
        /* Check that the proxy socket is bound to the standard bus path */
        if (strcmp(argv[i], "--bind") == 0 && argv[i + 1] &&
            strcmp(argv[i + 1], "/tmp/coder-dlp-dbus-proxy") == 0 && argv[i + 2] &&
            strcmp(argv[i + 2], "/run/user/1000/bus") == 0)
            found_proxy_bind = 1;
        /* Check DBUS_SESSION_BUS_ADDRESS points to the standard path */
        if (strcmp(argv[i], "DBUS_SESSION_BUS_ADDRESS") == 0 && i > 0 &&
            strcmp(argv[i - 1], "--setenv") == 0 && argv[i + 1] &&
            strcmp(argv[i + 1], "unix:path=/run/user/1000/bus") == 0)
            found_dbus_env = 1;
    }
    assert(found_proxy_bind);
    assert(found_dbus_env);

    dlp_free_bwrap_args(argv);
    free(comp);
    printf("test_bwrap_args_dbus_proxy_socket: PASSED\n");
}

static void test_bwrap_args_dbus_filter_config(void) {
    /* Verify that filter_dbus in sandbox config doesn't affect bwrap args
     * directly — it only controls whether the proxy is started in
     * coder_dlp_launch_app().  dlp_build_bwrap_args() uses the proxy socket
     * parameter.  With proxy_socket=NULL, behavior is identical to unfiltered. */
    setenv("XDG_RUNTIME_DIR", "/run/user/1000", 1);

    coder_dlp_compositor* comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);
    comp->socket = "wayland-test";

    coder_dlp_sandbox_config sandbox;
    memset(&sandbox, 0, sizeof(sandbox));
    sandbox.filter_dbus = true;
    sandbox.dbus_talk_names = NULL;
    sandbox.dbus_talk_count = 0;

    /* With proxy_socket=NULL, even though filter_dbus is set, bwrap args
     * should fall through to the unfiltered code path. */
    char** argv = dlp_build_bwrap_args(comp, "echo hi", &sandbox, NULL);
    assert(argv != NULL);

    /* Should NOT have a proxy socket bind */
    int found_proxy_bind = 0;
    for (int i = 0; argv[i]; i++) {
        if (strcmp(argv[i], "--bind") == 0 && argv[i + 1] &&
            strstr(argv[i + 1], "coder-dlp-dbus") != NULL)
            found_proxy_bind = 1;
    }
    assert(!found_proxy_bind);

    dlp_free_bwrap_args(argv);
    free(comp);
    printf("test_bwrap_args_dbus_filter_config: PASSED\n");
}

int main(void) {
    test_set_policy();
    test_null_safety();
    test_sandbox_launch_null_safety();
    test_bwrap_args_basic();
    test_bwrap_args_sandbox_options();
    test_bwrap_args_no_fs_isolation();
    test_bwrap_args_bind_home_rw();
    test_bwrap_args_extra_bind_paths();
    test_bwrap_args_extra_bind_paths_null();
    test_bwrap_args_null_returns_null();
    test_bwrap_args_dbus_proxy_socket();
    test_bwrap_args_dbus_filter_config();
    printf("All tests passed.\n");
    return 0;
}
