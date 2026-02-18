/* Bubblewrap sandbox launcher.
 *
 * Forks and exec's the requested command inside a bubblewrap (bwrap)
 * sandbox with WAYLAND_DISPLAY pointing at the nested compositor
 * socket and DISPLAY unset to prevent X11 escape. */

#include "coder_dlp.h"
#include "compositor_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
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

    const char* xdg_runtime = getenv("XDG_RUNTIME_DIR");

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

    /* GPU access for hardware-accelerated rendering (Electron, etc.) */
    PUSH("--dev-bind");
    PUSH("/dev/dri");
    PUSH("/dev/dri");

    /* Shared memory (needed by Electron/Chromium) */
    PUSH("--bind");
    PUSH("/dev/shm");
    PUSH("/dev/shm");

    PUSH("--proc");
    PUSH("/proc");
    PUSH("--tmpfs");
    PUSH("/tmp");

    /* XDG_RUNTIME_DIR — apps need a writable runtime directory to create
     * their own sockets, lock files, dconf databases, etc.  We overlay a
     * tmpfs and then bind the specific host sockets the app needs. */
    if (xdg_runtime) {
        PUSH("--tmpfs");
        PUSH(xdg_runtime);

        /* Compositor Wayland socket: bind into the fresh tmpfs so
         * clients can connect to our nested compositor. */
        if (comp->socket) {
            char socket_path[PATH_MAX];
            snprintf(socket_path, sizeof(socket_path), "%s/%s", xdg_runtime, comp->socket);
            PUSH("--bind");
            PUSH(socket_path);
            PUSH(socket_path);

            char lock_path[PATH_MAX];
            snprintf(lock_path, sizeof(lock_path), "%s/%s.lock", xdg_runtime, comp->socket);
            PUSH("--ro-bind");
            PUSH(lock_path);
            PUSH(lock_path);
        }
    }

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

    if (xdg_runtime) {
        PUSH("--setenv");
        PUSH("XDG_RUNTIME_DIR");
        PUSH(xdg_runtime);
    }

    /* D-Bus session bus — bind the host bus socket into the sandbox tmpfs.
     * Must come after the tmpfs overlay of XDG_RUNTIME_DIR above. */
    if (xdg_runtime) {
        char dbus_path[PATH_MAX];
        snprintf(dbus_path, sizeof(dbus_path), "%s/bus", xdg_runtime);
        if (access(dbus_path, F_OK) == 0) {
            PUSH("--bind");
            PUSH(dbus_path);
            PUSH(dbus_path);
            PUSH("--setenv");
            PUSH("DBUS_SESSION_BUS_ADDRESS");
            char dbus_addr[PATH_MAX + 32];
            snprintf(dbus_addr, sizeof(dbus_addr), "unix:path=%s", dbus_path);
            PUSH(dbus_addr);
        }
    }

    PUSH("--unsetenv");
    PUSH("DISPLAY");

    /* Chromium/Electron's internal sandbox conflicts with bwrap's user
     * namespace.  Disable it since bwrap already provides sandboxing. */
    PUSH("--setenv");
    PUSH("CHROME_FLAGS");
    PUSH("--no-sandbox");
    PUSH("--setenv");
    PUSH("ELECTRON_NO_SANDBOX");
    PUSH("1");

    PUSH("--die-with-parent");

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
        /* Redirect stdout+stderr to a log file for debugging.
         * The fd is inherited through bwrap to the child process. */
        int log_fd = open("/tmp/coder-dlp-child.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }
        execvp("bwrap", argv);
        /* If we get here, exec failed */
        dprintf(STDERR_FILENO, "coder-dlp: execvp(bwrap) failed: %s\n", strerror(errno));
        _exit(127);
    }

    /* Parent */
    wlr_log(WLR_INFO, "app launched with pid %d (child logs: /tmp/coder-dlp-child.log)", pid);
    dlp_free_bwrap_args(argv);
    return (int)pid;
}
