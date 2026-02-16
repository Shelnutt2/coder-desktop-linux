#include "coder_dlp.h"
#include "compositor_internal.h"

int coder_dlp_get_fd(coder_dlp_compositor* comp) {
    if (!comp) {
        return -1;
    }
    /* TODO (Phase 4): return wl_event_loop fd from wl_display */
    return -1;
}

void coder_dlp_dispatch(coder_dlp_compositor* comp) {
    if (!comp) {
        return;
    }
    /* TODO (Phase 4): call wl_display_flush_clients + wl_event_loop_dispatch */
}
