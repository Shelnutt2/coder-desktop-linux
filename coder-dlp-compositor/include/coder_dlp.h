#ifndef CODER_DLP_H
#define CODER_DLP_H

#include <stdbool.h>
#include <stdint.h>

/* Symbol visibility — only public API functions are exported from libcoderdlp.so.
 * When building the library, CODERDLP_BUILDING is defined and CODERDLP_EXPORT
 * marks symbols as visible.  All other symbols (including statically-linked
 * wlroots internals) remain hidden thanks to -fvisibility=hidden. */
#if defined(CODERDLP_BUILDING)
#define CODERDLP_EXPORT __attribute__((visibility("default")))
#else
#define CODERDLP_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque types */
typedef struct coder_dlp_compositor coder_dlp_compositor;

/* DLP policy configuration */
typedef struct coder_dlp_policy {
    bool clipboard_block_outgoing; /* block copy from compositor to system */
    bool clipboard_block_incoming; /* block paste from system to compositor */
    bool screenshot_block;         /* always true — we don't expose screencopy */
    bool file_sandbox;             /* enable filesystem sandbox via bwrap */
    bool network_sandbox;          /* force traffic through VPN tunnel */
    bool watermark_enabled;        /* embed invisible watermark in output frames */
} coder_dlp_policy;

/* Sandbox config for launched apps */
typedef struct coder_dlp_sandbox_config {
    const char* workspace_path;    /* filesystem path to mount */
    const char* network_namespace; /* network namespace name (or NULL) */
    bool isolate_pid;
    bool isolate_ipc;
    bool isolate_filesystem;       /* true: ro-bind / + tmpfs $HOME; false: full host fs access */
    bool bind_home_rw;             /* bind $HOME rw even with fs isolation (instead of tmpfs) */
    const char** extra_bind_paths; /* NULL-terminated array of extra rw bind paths */
    int extra_bind_count;          /* number of entries in extra_bind_paths */
    bool filter_dbus;              /* enable xdg-dbus-proxy D-Bus filtering */
    const char** dbus_talk_names;  /* additional bus names to allow (--talk) */
    int dbus_talk_count;           /* number of entries in dbus_talk_names */
} coder_dlp_sandbox_config;

/* Log verbosity (maps to wlroots log levels internally) */
typedef enum coder_dlp_log_level {
    CODER_DLP_LOG_ERROR = 0, /* WLR_ERROR — errors only (default) */
    CODER_DLP_LOG_INFO = 1,  /* WLR_INFO  — errors + info */
    CODER_DLP_LOG_DEBUG = 2, /* WLR_DEBUG — everything */
} coder_dlp_log_level;

/* Lifecycle */
CODERDLP_EXPORT coder_dlp_compositor* coder_dlp_create(void* parent_wl_surface,
                                                       coder_dlp_log_level log_level);
CODERDLP_EXPORT void coder_dlp_destroy(coder_dlp_compositor* comp);
CODERDLP_EXPORT int coder_dlp_get_fd(coder_dlp_compositor* comp);
CODERDLP_EXPORT void coder_dlp_dispatch(coder_dlp_compositor* comp);
CODERDLP_EXPORT bool coder_dlp_is_available(void); /* returns true if WAYLAND_DISPLAY is set */

/* Client tracking */
CODERDLP_EXPORT int coder_dlp_get_client_count(const coder_dlp_compositor* comp);

/* Set the title of the compositor's output window (visible in the parent
 * compositor's window list / title bar). */
CODERDLP_EXPORT void coder_dlp_set_output_title(coder_dlp_compositor* comp, const char* title);

/* Policy */
CODERDLP_EXPORT void coder_dlp_set_policy(coder_dlp_compositor* comp,
                                          const coder_dlp_policy* policy);

/* App launching (returns PID or -1 on error) */
CODERDLP_EXPORT int coder_dlp_launch_app(coder_dlp_compositor* comp, const char* command,
                                         const coder_dlp_sandbox_config* sandbox);

/* Optional log callback — if set, compositor logs are forwarded here
 * in addition to stderr.  The callback receives the formatted message. */
typedef void (*coder_dlp_log_cb)(const char* message, void* user_data);
CODERDLP_EXPORT void coder_dlp_set_log_callback(coder_dlp_compositor* comp, coder_dlp_log_cb cb,
                                                void* user_data);

/* Surface callback */
typedef void (*coder_dlp_surface_cb)(coder_dlp_compositor* comp, void* wlr_surface, void* data);
CODERDLP_EXPORT void coder_dlp_on_new_surface(coder_dlp_compositor* comp, coder_dlp_surface_cb cb,
                                              void* data);

/* Watermark — set session identity string for steganographic watermarking.
 * Only effective when policy.watermark_enabled is true. */
CODERDLP_EXPORT void coder_dlp_set_watermark_identity(coder_dlp_compositor* comp,
                                                      const char* identity);

/* Returns the Xwayland DISPLAY string (e.g. ":1") or NULL if not ready. */
CODERDLP_EXPORT const char* coder_dlp_get_xwayland_display(const coder_dlp_compositor* comp);

#ifdef __cplusplus
}
#endif

#endif /* CODER_DLP_H */
