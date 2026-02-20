/* Bubblewrap sandbox launcher.
 *
 * Forks and exec's the requested command inside a bubblewrap (bwrap)
 * sandbox with WAYLAND_DISPLAY pointing at the nested compositor
 * socket and DISPLAY unset to prevent X11 escape. */

#define _GNU_SOURCE /* pipe2, inotify_init1 */

#include "coder_dlp.h"
#include "compositor_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <unistd.h>

#include <poll.h>
#include <signal.h>
#include <wlr/util/log.h>

/* Check if a binary is available somewhere in PATH. */
static bool find_in_path(const char* name) {
    char* path = getenv("PATH");
    if (!path) {
        return false;
    }
    char* dup = strdup(path);
    if (!dup) {
        return false;
    }
    char* tok = strtok(dup, ":");
    while (tok) {
        char buf[PATH_MAX];
        snprintf(buf, sizeof(buf), "%s/%s", tok, name);
        if (access(buf, X_OK) == 0) {
            free(dup);
            return true;
        }
        tok = strtok(NULL, ":");
    }
    free(dup);
    return false;
}

/* Start xdg-dbus-proxy with filtering.  Returns 0 on success, -1 on failure.
 * On success, fills in proxy->pid, proxy->pipe_write_fd, proxy->socket_path. */
static int dlp_start_dbus_proxy(struct dlp_dbus_proxy* proxy,
                                const coder_dlp_sandbox_config* sandbox) {
    const char* dbus_addr = getenv("DBUS_SESSION_BUS_ADDRESS");
    if (!dbus_addr) {
        wlr_log(WLR_ERROR, "DBUS_SESSION_BUS_ADDRESS not set, cannot start D-Bus proxy");
        return -1;
    }

    /* Check xdg-dbus-proxy is available somewhere in PATH */
    if (!find_in_path("xdg-dbus-proxy")) {
        wlr_log(WLR_ERROR, "xdg-dbus-proxy not found in PATH, D-Bus filtering unavailable");
        return -1;
    }

    /* Create a unique directory for the proxy socket (avoids TOCTOU —
     * mkdtemp creates the directory atomically with mode 0700). */
    snprintf(proxy->dir_path, sizeof(proxy->dir_path), "/tmp/coder-dlp-dbus-XXXXXX");
    if (!mkdtemp(proxy->dir_path)) {
        wlr_log(WLR_ERROR, "mkdtemp failed for D-Bus proxy: %s", strerror(errno));
        return -1;
    }
    snprintf(proxy->socket_path, sizeof(proxy->socket_path), "%s/bus", proxy->dir_path);

    /* Create lifecycle pipe — closing the write end signals proxy to exit.
     * O_CLOEXEC prevents child processes from inheriting both ends. */
    int pipe_fds[2];
    if (pipe2(pipe_fds, O_CLOEXEC) != 0) {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child: exec xdg-dbus-proxy */

        /* Reset signal mask and handlers inherited from parent */
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, NULL);
        struct sigaction sa = {.sa_handler = SIG_DFL};
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGCHLD, &sa, NULL);

        close(pipe_fds[1]); /* close write end */

        /* Clear O_CLOEXEC on read end — xdg-dbus-proxy needs it via --fd */
        int flags = fcntl(pipe_fds[0], F_GETFD);
        fcntl(pipe_fds[0], F_SETFD, flags & ~FD_CLOEXEC);

        /* Build argv.  Max entries: base(4) + --fd(1) + default allows(~10)
         * + user talks(talk_count) + NULL */
        const char* av[128];
        int ac = 0;
        av[ac++] = "xdg-dbus-proxy";
        av[ac++] = dbus_addr;
        av[ac++] = proxy->socket_path;
        av[ac++] = "--filter";

        /* --fd=N keeps proxy alive until the pipe is closed */
        char fd_arg[32];
        snprintf(fd_arg, sizeof(fd_arg), "--fd=%d", pipe_fds[0]);
        av[ac++] = fd_arg;

        /* Default safe allowlist — fine-grained portal access */
        av[ac++] =
            "--call=org.freedesktop.portal.Desktop="
            "org.freedesktop.portal.Settings.*@/org/freedesktop/portal/desktop";
        av[ac++] =
            "--call=org.freedesktop.portal.Desktop="
            "org.freedesktop.portal.Inhibit.*@/org/freedesktop/portal/desktop";
        /* Accessibility */
        av[ac++] = "--talk=org.a11y.Bus";
        /* Notifications — see + specific calls */
        av[ac++] = "--see=org.freedesktop.Notifications";
        av[ac++] =
            "--call=org.freedesktop.Notifications="
            "org.freedesktop.Notifications.Notify@/org/freedesktop/Notifications";
        av[ac++] =
            "--call=org.freedesktop.Notifications="
            "org.freedesktop.Notifications.GetCapabilities@/org/freedesktop/Notifications";
        av[ac++] =
            "--call=org.freedesktop.Notifications="
            "org.freedesktop.Notifications.GetServerInformation@/org/freedesktop/Notifications";

        /* User-specified additional --talk names */
        char talk_bufs[16][256];
        if (sandbox && sandbox->dbus_talk_names) {
            for (int i = 0; i < sandbox->dbus_talk_count && i < 16 && ac < 60; i++) {
                if (sandbox->dbus_talk_names[i]) {
                    snprintf(talk_bufs[i], sizeof(talk_bufs[i]), "--talk=%s",
                             sandbox->dbus_talk_names[i]);
                    av[ac++] = talk_bufs[i];
                }
            }
        }

        if (ac >= 127) { /* overflow guard — leave room for NULL */
            _exit(125);
        }
        av[ac] = NULL;
        execvp("xdg-dbus-proxy", (char**)av);
        _exit(127);
    }

    /* Parent */
    close(pipe_fds[0]); /* close read end */
    proxy->pid = pid;
    proxy->pipe_write_fd = pipe_fds[1];

    /* Wait for proxy socket to appear (max 2 seconds).
     * Prefer inotify+poll to avoid busy-waiting that blocks the
     * compositor event loop.  Fall back to brief usleep loop if
     * inotify is unavailable. */
    bool socket_ready = (access(proxy->socket_path, F_OK) == 0);
    if (!socket_ready) {
        int ifd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (ifd >= 0) {
            inotify_add_watch(ifd, proxy->dir_path, IN_CREATE);
            /* Re-check after watch is armed (race with fast proxy start) */
            if (access(proxy->socket_path, F_OK) != 0) {
                struct pollfd pfd = {.fd = ifd, .events = POLLIN};
                poll(&pfd, 1, 2000); /* 2 second timeout */
            }
            close(ifd);
            socket_ready = (access(proxy->socket_path, F_OK) == 0);
        } else {
            /* Fallback: brief poll loop */
            for (int i = 0; i < 200 && !socket_ready; i++) {
                usleep(10000); /* 10ms */
                socket_ready = (access(proxy->socket_path, F_OK) == 0);
            }
        }
    }

    if (!socket_ready) {
        /* Timeout — proxy didn't start */
        wlr_log(WLR_ERROR, "xdg-dbus-proxy timed out waiting for socket");
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        proxy->pid = 0;
        close(pipe_fds[1]);
        proxy->pipe_write_fd = -1;
        unlink(proxy->socket_path);
        rmdir(proxy->dir_path);
        return -1;
    }
    return 0;
}

void dlp_reap_dbus_proxies(coder_dlp_compositor* comp) {
    if (!comp || !comp->dbus_proxies) return;
    for (int i = 0; i < comp->dbus_proxy_count; i++) {
        if (comp->dbus_proxies[i].pid <= 0) continue;
        int status;
        pid_t ret = waitpid(comp->dbus_proxies[i].pid, &status, WNOHANG);
        if (ret > 0) {
            /* Proxy exited — clean up its resources. */
            if (comp->dbus_proxies[i].pipe_write_fd >= 0) {
                close(comp->dbus_proxies[i].pipe_write_fd);
                comp->dbus_proxies[i].pipe_write_fd = -1;
            }
            if (comp->dbus_proxies[i].socket_path[0]) {
                unlink(comp->dbus_proxies[i].socket_path);
            }
            if (comp->dbus_proxies[i].dir_path[0]) {
                rmdir(comp->dbus_proxies[i].dir_path);
            }
            comp->dbus_proxies[i].pid = 0; /* mark as reaped */
        }
    }
}

void dlp_cleanup_dbus_proxies(coder_dlp_compositor* comp) {
    if (!comp) {
        return;
    }
    for (int i = 0; i < comp->dbus_proxy_count; i++) {
        /* Skip entries already reaped by dlp_reap_dbus_proxies(). */
        if (comp->dbus_proxies[i].pid == 0) continue;

        /* Close the lifecycle pipe first — this signals the proxy to exit
         * gracefully via EOF on its --fd pipe. */
        if (comp->dbus_proxies[i].pipe_write_fd >= 0) {
            close(comp->dbus_proxies[i].pipe_write_fd);
            comp->dbus_proxies[i].pipe_write_fd = -1;
        }
        if (comp->dbus_proxies[i].pid > 0) {
            /* Give the proxy a moment to exit from pipe EOF */
            int status;
            pid_t ret = waitpid(comp->dbus_proxies[i].pid, &status, WNOHANG);
            if (ret == 0) {
                /* Still running — send SIGTERM and wait */
                kill(comp->dbus_proxies[i].pid, SIGTERM);
                waitpid(comp->dbus_proxies[i].pid, NULL, 0);
            }
        }
        /* Remove socket file and directory */
        if (comp->dbus_proxies[i].socket_path[0]) {
            unlink(comp->dbus_proxies[i].socket_path);
        }
        if (comp->dbus_proxies[i].dir_path[0]) {
            rmdir(comp->dbus_proxies[i].dir_path);
        }
    }
    free(comp->dbus_proxies);
    comp->dbus_proxies = NULL;
    comp->dbus_proxy_count = 0;
    comp->dbus_proxy_capacity = 0;
}

/* Visible for testing via the internal header — builds the NULL-terminated
 * argv array for bwrap.  Caller must free with dlp_free_bwrap_args(). */
char** dlp_build_bwrap_args(const coder_dlp_compositor* comp, const char* command,
                            const coder_dlp_sandbox_config* sandbox,
                            const char* dbus_proxy_socket) {
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

    /* D-Bus session bus — bind either the xdg-dbus-proxy filtered socket or
     * the host bus socket into the sandbox tmpfs.
     * Must come after the tmpfs overlay of XDG_RUNTIME_DIR above.
     *
     * dbus_proxy_socket semantics:
     *   non-NULL, non-empty  → filtered proxy socket, bind it
     *   empty string ("")    → D-Bus blocked entirely (proxy failed with filter_dbus=true)
     *   NULL                 → unfiltered, bind real bus (filter_dbus=false) */
    if (xdg_runtime) {
        char dbus_path[PATH_MAX];
        snprintf(dbus_path, sizeof(dbus_path), "%s/bus", xdg_runtime);
        if (dbus_proxy_socket && dbus_proxy_socket[0] != '\0') {
            /* Filtered: bind the proxy socket to the standard bus path */
            PUSH("--bind");
            PUSH(dbus_proxy_socket);
            PUSH(dbus_path);
            PUSH("--setenv");
            PUSH("DBUS_SESSION_BUS_ADDRESS");
            char dbus_addr[PATH_MAX + 32];
            snprintf(dbus_addr, sizeof(dbus_addr), "unix:path=%s", dbus_path);
            PUSH(dbus_addr);
        } else if (dbus_proxy_socket && dbus_proxy_socket[0] == '\0') {
            /* D-Bus blocked: proxy was requested but failed — no bus access.
             * Unset DBUS_SESSION_BUS_ADDRESS so apps don't try to connect. */
            PUSH("--unsetenv");
            PUSH("DBUS_SESSION_BUS_ADDRESS");
        } else if (access(dbus_path, F_OK) == 0) {
            /* Unfiltered: bind the real bus socket directly */
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

        /* Disable DRI3 for X11 clients going through Xwayland.
         * Xwayland's DRI3/DMA-BUF GPU path can produce garbled output
         * (black window with random colours) when buffer formats/modifiers
         * from the client's GPU context are incompatible with the nested
         * compositor's renderer (common with Chromium/Electron apps like
         * VS Code).  Disabling DRI3 forces the DRI2 fallback, which goes
         * through X11 pixmaps that Xwayland converts to wl_shm buffers —
         * universally compatible.  Unlike LIBGL_ALWAYS_SOFTWARE, this
         * preserves GPU rasterisation in the client (only the buffer
         * transport changes).  The compositor itself still uses GPU
         * compositing, and native Wayland clients are unaffected. */
        PUSH("--setenv");
        PUSH("LIBGL_DRI3_DISABLE");
        PUSH("1");

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

    /* Optionally start a D-Bus filtering proxy */
    struct dlp_dbus_proxy proxy;
    memset(&proxy, 0, sizeof(proxy));
    proxy.pid = -1;
    proxy.pipe_write_fd = -1;
    const char* proxy_socket = NULL;

    if (sandbox && sandbox->filter_dbus) {
        if (dlp_start_dbus_proxy(&proxy, sandbox) == 0) {
            proxy_socket = proxy.socket_path;
            wlr_log(WLR_INFO, "D-Bus proxy started (pid=%d, socket=%s)", proxy.pid,
                    proxy.socket_path);
        } else {
            wlr_log(WLR_ERROR, "D-Bus proxy failed, launching without D-Bus access");
            proxy_socket = ""; /* empty string sentinel = no D-Bus at all */
        }
    }

    char** argv = dlp_build_bwrap_args(comp, command, sandbox, proxy_socket);
    if (!argv) {
        /* Clean up proxy if we started one */
        if (proxy.pid > 0) {
            close(proxy.pipe_write_fd);
            kill(proxy.pid, SIGTERM);
            waitpid(proxy.pid, NULL, 0);
            unlink(proxy.socket_path);
            if (proxy.dir_path[0]) {
                rmdir(proxy.dir_path);
            }
        }
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        dlp_free_bwrap_args(argv);
        /* Clean up proxy if we started one */
        if (proxy.pid > 0) {
            close(proxy.pipe_write_fd);
            kill(proxy.pid, SIGTERM);
            waitpid(proxy.pid, NULL, 0);
            unlink(proxy.socket_path);
            if (proxy.dir_path[0]) {
                rmdir(proxy.dir_path);
            }
        }
        return -1;
    }

    if (pid == 0) {
        /* Reset signal mask and handlers inherited from parent */
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, NULL);
        struct sigaction sa = {.sa_handler = SIG_DFL};
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGCHLD, &sa, NULL);

        /* Redirect stdout+stderr to a per-PID log file for debugging.
         * O_NOFOLLOW prevents symlink attacks on multi-user systems.
         * O_CLOEXEC ensures the fd doesn't leak through exec after dup2. */
        char logpath[64];
        snprintf(logpath, sizeof(logpath), "/tmp/coder-dlp-child-%d.log", getpid());
        int log_fd = open(logpath, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0600);
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
    wlr_log(WLR_INFO, "app launched with pid %d (child logs: /tmp/coder-dlp-child-*.log)", pid);
    dlp_free_bwrap_args(argv);

    /* Track the proxy for cleanup during compositor teardown */
    if (proxy.pid > 0) {
        if (comp->dbus_proxy_count >= comp->dbus_proxy_capacity) {
            int new_cap = comp->dbus_proxy_capacity ? comp->dbus_proxy_capacity * 2 : 4;
            struct dlp_dbus_proxy* tmp =
                realloc(comp->dbus_proxies, (size_t)new_cap * sizeof(*tmp));
            if (tmp) {
                comp->dbus_proxies = tmp;
                comp->dbus_proxy_capacity = new_cap;
            } else {
                wlr_log(WLR_ERROR, "failed to track D-Bus proxy, cleaning up");
                close(proxy.pipe_write_fd);
                kill(proxy.pid, SIGTERM);
                waitpid(proxy.pid, NULL, 0);
                unlink(proxy.socket_path);
                if (proxy.dir_path[0]) {
                    rmdir(proxy.dir_path);
                }
            }
        }
        if (comp->dbus_proxy_count < comp->dbus_proxy_capacity) {
            comp->dbus_proxies[comp->dbus_proxy_count++] = proxy;
        }
    }

    return (int)pid;
}
