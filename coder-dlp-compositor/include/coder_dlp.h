#ifndef CODER_DLP_H
#define CODER_DLP_H

#include <stdbool.h>
#include <stdint.h>

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
} coder_dlp_policy;

/* Sandbox config for launched apps */
typedef struct coder_dlp_sandbox_config {
    const char* workspace_path;    /* filesystem path to mount */
    const char* network_namespace; /* network namespace name (or NULL) */
    bool isolate_pid;
    bool isolate_ipc;
} coder_dlp_sandbox_config;

/* Lifecycle */
coder_dlp_compositor* coder_dlp_create(void* parent_wl_surface);
void coder_dlp_destroy(coder_dlp_compositor* comp);
int coder_dlp_get_fd(coder_dlp_compositor* comp);
void coder_dlp_dispatch(coder_dlp_compositor* comp);
bool coder_dlp_is_available(void); /* returns true if WAYLAND_DISPLAY is set */

/* Policy */
void coder_dlp_set_policy(coder_dlp_compositor* comp, const coder_dlp_policy* policy);

/* App launching (returns PID or -1 on error) */
int coder_dlp_launch_app(coder_dlp_compositor* comp, const char* command,
                         const coder_dlp_sandbox_config* sandbox);

/* Surface callback */
typedef void (*coder_dlp_surface_cb)(coder_dlp_compositor* comp, void* wlr_surface, void* data);
void coder_dlp_on_new_surface(coder_dlp_compositor* comp, coder_dlp_surface_cb cb, void* data);

#ifdef __cplusplus
}
#endif

#endif /* CODER_DLP_H */
