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
    coder_dlp_compositor *comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);
    assert(coder_dlp_launch_app(comp, NULL, NULL) == -1);
    free(comp);

    printf("test_sandbox_launch_null_safety: PASSED\n");
}

static void test_bwrap_args_basic(void) {
    /* Verify the argv array built by dlp_build_bwrap_args contains the
     * expected entries for a minimal configuration. */
    coder_dlp_compositor *comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);
    comp->socket = "wayland-test";

    char **argv = dlp_build_bwrap_args(comp, "echo hello", NULL);
    assert(argv != NULL);

    /* First arg must be "bwrap" */
    assert(strcmp(argv[0], "bwrap") == 0);

    /* Must contain --ro-bind / / */
    int found_ro_bind = 0;
    int found_wayland = 0;
    int found_unsetenv_display = 0;
    int found_command = 0;
    for (int i = 0; argv[i]; i++) {
        if (strcmp(argv[i], "--ro-bind") == 0) found_ro_bind = 1;
        if (strcmp(argv[i], "WAYLAND_DISPLAY") == 0 && i > 0 &&
            strcmp(argv[i - 1], "--setenv") == 0) {
            assert(strcmp(argv[i + 1], "wayland-test") == 0);
            found_wayland = 1;
        }
        if (strcmp(argv[i], "DISPLAY") == 0 && i > 0 &&
            strcmp(argv[i - 1], "--unsetenv") == 0)
            found_unsetenv_display = 1;
        if (strcmp(argv[i], "echo hello") == 0) found_command = 1;
    }
    assert(found_ro_bind);
    assert(found_wayland);
    assert(found_unsetenv_display);
    assert(found_command);

    dlp_free_bwrap_args(argv);
    free(comp);
    printf("test_bwrap_args_basic: PASSED\n");
}

static void test_bwrap_args_sandbox_options(void) {
    /* Verify sandbox config options appear in argv. */
    coder_dlp_compositor *comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);
    comp->socket = "wayland-test";

    coder_dlp_sandbox_config sandbox;
    memset(&sandbox, 0, sizeof(sandbox));
    sandbox.workspace_path = "/home/user/workspace";
    sandbox.isolate_pid = true;
    sandbox.isolate_ipc = true;
    sandbox.network_namespace = "vpn0";

    char **argv = dlp_build_bwrap_args(comp, "ls", &sandbox);
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

static void test_bwrap_args_null_returns_null(void) {
    /* NULL comp or command must return NULL. */
    assert(dlp_build_bwrap_args(NULL, "echo", NULL) == NULL);

    coder_dlp_compositor *comp = calloc(1, sizeof(*comp));
    assert(comp != NULL);
    assert(dlp_build_bwrap_args(comp, NULL, NULL) == NULL);
    free(comp);

    /* dlp_free_bwrap_args(NULL) must not crash */
    dlp_free_bwrap_args(NULL);

    printf("test_bwrap_args_null_returns_null: PASSED\n");
}

int main(void) {
    test_set_policy();
    test_null_safety();
    test_sandbox_launch_null_safety();
    test_bwrap_args_basic();
    test_bwrap_args_sandbox_options();
    test_bwrap_args_null_returns_null();
    printf("All tests passed.\n");
    return 0;
}
