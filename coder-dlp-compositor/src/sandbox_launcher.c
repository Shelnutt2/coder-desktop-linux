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

    if (sandbox && sandbox->isolate_filesystem) {
        /* Strict DLP: read-only root + ephemeral writable home */
        PUSH("--ro-bind");
        PUSH("/");
        PUSH("/");
        PUSH("--dev");
        PUSH("/dev");
        PUSH("--dev-bind");
        PUSH("/dev/dri");
        PUSH("/dev/dri");
        PUSH("--bind");
        PUSH("/dev/shm");
        PUSH("/dev/shm");
        PUSH("--proc");
        PUSH("/proc");
        PUSH("--tmpfs");
        PUSH("/tmp");

        /* Home directory: if bind_home_rw is set, bind the real $HOME
         * read-write (for app-specific profiles that need persistence).
         * Otherwise use a tmpfs overlay — data is lost on exit, preventing
         * persistent exfiltration while letting apps write profiles,
         * config, and shader caches. */
        const char* home = getenv("HOME");
        if (home) {
            if (sandbox->bind_home_rw) {
                PUSH("--bind");
                PUSH(home);
                PUSH(home);
            } else {
                PUSH("--tmpfs");
                PUSH(home);
            }
        }
    } else {
        /* Non-isolated: full host filesystem access (read-write).
         * The sandbox still enforces Wayland isolation (no X11)
         * and optional PID/IPC/network namespacing. */
        PUSH("--bind");
        PUSH("/");
        PUSH("/");
        PUSH("--dev");
        PUSH("/dev");
        PUSH("--dev-bind");
        PUSH("/dev/dri");
        PUSH("/dev/dri");
        PUSH("--bind");
        PUSH("/dev/shm");
        PUSH("/dev/shm");
        PUSH("--proc");
        PUSH("/proc");

        /* Overlay /tmp so sandboxed apps cannot see host IPC sockets.
         * Without this, apps like VS Code and Firefox detect a running
         * host instance (via /tmp/vscode-*.sock, lock files, etc.),
         * send an IPC "open" message to it, and exit immediately. */
        PUSH("--tmpfs");
        PUSH("/tmp");
    }

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

    /* Workspace path: writable bind mount (only needed when filesystem is
     * isolated, since the non-isolated mode already has full rw access). */
    if (sandbox && sandbox->isolate_filesystem && sandbox->workspace_path) {
        PUSH("--bind");
        PUSH(sandbox->workspace_path);
        PUSH(sandbox->workspace_path);
    }

    /* Extra bind paths: additional rw bind mounts for app-specific profiles */
    if (sandbox && sandbox->extra_bind_paths) {
        for (int i = 0; i < sandbox->extra_bind_count; i++) {
            if (sandbox->extra_bind_paths[i]) {
                PUSH("--bind");
                PUSH(sandbox->extra_bind_paths[i]);
                PUSH(sandbox->extra_bind_paths[i]);
            }
        }
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

    /* X11/Xwayland display: if Xwayland is running, set DISPLAY so X11 apps
     * connect through our compositor.  Otherwise unset to prevent escape. */
    const char* xwl_display = coder_dlp_get_xwayland_display(comp);
    if (xwl_display) {
        PUSH("--setenv");
        PUSH("DISPLAY");
        PUSH(xwl_display);

        /* Bind the X11 socket into the sandbox */
        const char* display_num = xwl_display;
        if (display_num[0] == ':') {
            display_num++;
        }
        char x11_socket[PATH_MAX];
        snprintf(x11_socket, sizeof(x11_socket), "/tmp/.X11-unix/X%s", display_num);
        PUSH("--bind");
        PUSH(x11_socket);
        PUSH(x11_socket);

        /* With Xwayland available, don't force Wayland backends.
         * Native Wayland apps auto-detect WAYLAND_DISPLAY; X11-only apps
         * fall back to DISPLAY.  Both paths go through DLP compositor. */
    } else {
        PUSH("--unsetenv");
        PUSH("DISPLAY");

        /* Force Wayland backends when no Xwayland is available */
        PUSH("--setenv");
        PUSH("GDK_BACKEND");
        PUSH("wayland");
        PUSH("--setenv");
        PUSH("QT_QPA_PLATFORM");
        PUSH("wayland");
        PUSH("--setenv");
        PUSH("SDL_VIDEODRIVER");
        PUSH("wayland");
    }

    /* Chromium/Electron's internal sandbox conflicts with bwrap's user
     * namespace.  Disable it since bwrap already provides sandboxing. */
    PUSH("--setenv");
    PUSH("ELECTRON_NO_SANDBOX");
    PUSH("1");

    /* These hints are always useful regardless of Xwayland availability */
    PUSH("--setenv");
    PUSH("ELECTRON_OZONE_PLATFORM_HINT");
    PUSH("wayland");
    PUSH("--setenv");
    PUSH("MOZ_ENABLE_WAYLAND");
    PUSH("1");

    PUSH("--die-with-parent");

    /* Create a new terminal session so bwrap waits for ALL descendant
     * processes, not just the direct child.  Many apps (VS Code, Firefox,
     * Electron-based IDEs) fork the real process and exit the wrapper
     * immediately — without --new-session, bwrap would exit too and we'd
     * destroy the compositor while the app is still running. */
    PUSH("--new-session");

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
        int log_fd = open("/tmp/coder-dlp-child.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
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
