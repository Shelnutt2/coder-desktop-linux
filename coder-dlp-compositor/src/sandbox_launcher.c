/* Bubblewrap sandbox launcher.
 *
 * Forks and exec's the requested command inside a bubblewrap (bwrap)
 * sandbox with WAYLAND_DISPLAY pointing at the nested compositor
 * socket and DISPLAY unset to prevent X11 escape. */

#include "coder_dlp.h"
#include "compositor_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <wlr/util/log.h>

/* Visible for testing via the internal header — builds the NULL-terminated
 * argv array for bwrap.  Caller must free with dlp_free_bwrap_args(). */
char** dlp_build_bwrap_args(const coder_dlp_compositor* comp, const char* command,
                            const coder_dlp_sandbox_config* sandbox) {
    if (!comp || !command) {
        return NULL;
    }

    int argc = 0;
    int capacity = 64;
    char** argv = calloc((size_t)capacity, sizeof(char*));
    if (!argv) {
        return NULL;
    }

#define PUSH(s)                                                           \
    do {                                                                  \
        if (argc >= capacity - 1) {                                       \
            capacity *= 2;                                                \
            char** tmp = realloc(argv, (size_t)capacity * sizeof(char*)); \
            if (!tmp) {                                                   \
                goto oom;                                                 \
            }                                                             \
            argv = tmp;                                                   \
        }                                                                 \
        argv[argc++] = strdup(s);                                         \
        if (!argv[argc - 1]) {                                            \
            goto oom;                                                     \
        }                                                                 \
    } while (0)

    PUSH("bwrap");

    /* Base filesystem: read-only bind of / */
    PUSH("--ro-bind");
    PUSH("/");
    PUSH("/");
    PUSH("--dev");
    PUSH("/dev");
    PUSH("--proc");
    PUSH("/proc");
    PUSH("--tmpfs");
    PUSH("/tmp");

    /* Workspace path: writable bind mount */
    if (sandbox && sandbox->workspace_path) {
        PUSH("--bind");
        PUSH(sandbox->workspace_path);
        PUSH(sandbox->workspace_path);
    }

    /* PID namespace isolation */
    if (sandbox && sandbox->isolate_pid) {
        PUSH("--unshare-pid");
    }

    /* IPC namespace isolation */
    if (sandbox && sandbox->isolate_ipc) {
        PUSH("--unshare-ipc");
    }

    /* Network namespace isolation */
    if (sandbox && sandbox->network_namespace) {
        PUSH("--unshare-net");
    }

    /* Environment: point to compositor socket, block X11 */
    PUSH("--setenv");
    PUSH("WAYLAND_DISPLAY");
    PUSH(comp->socket ? comp->socket : "wayland-0");

    PUSH("--unsetenv");
    PUSH("DISPLAY");

    /* Command */
    PUSH("--");
    PUSH("/bin/sh");
    PUSH("-c");
    PUSH(command);

    argv[argc] = NULL;
    return argv;

oom:
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
    return NULL;

#undef PUSH
}

void dlp_free_bwrap_args(char** argv) {
    if (!argv) {
        return;
    }
    for (int i = 0; argv[i]; i++) {
        free(argv[i]);
    }
    free(argv);
}

int coder_dlp_launch_app(coder_dlp_compositor* comp, const char* command,
                         const coder_dlp_sandbox_config* sandbox) {
    if (!comp || !command) {
        return -1;
    }

    wlr_log(WLR_INFO, "launching app: %s (socket=%s)", command,
            comp->socket ? comp->socket : "(null)");

    char** argv = dlp_build_bwrap_args(comp, command, sandbox);
    if (!argv) {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        dlp_free_bwrap_args(argv);
        return -1;
    }

    if (pid == 0) {
        /* Child: exec bwrap */
        execvp("bwrap", argv);
        /* execvp failed — _exit to avoid running parent atexit handlers */
        _exit(127);
    }

    /* Parent */
    wlr_log(WLR_INFO, "app launched with pid %d", pid);
    dlp_free_bwrap_args(argv);
    return (int)pid;
}
