#include "coder_dlp.h"
#include "compositor_internal.h"

#include <stdlib.h>
#include <string.h>

coder_dlp_compositor* coder_dlp_create(void* parent_wl_surface) {
    (void)parent_wl_surface;

    coder_dlp_compositor* comp = calloc(1, sizeof(*comp));
    if (!comp) {
        return NULL;
    }

    /* TODO (Phase 4): initialise wlr_backend, wlr_renderer, wl_display,
     * create wlr_wl_output from parent_wl_surface, etc. */

    return comp;
}

void coder_dlp_destroy(coder_dlp_compositor* comp) {
    if (!comp) {
        return;
    }
    /* TODO (Phase 4): tear down wlroots objects */
    free(comp);
}

bool coder_dlp_is_available(void) {
    return getenv("WAYLAND_DISPLAY") != NULL;
}

void coder_dlp_on_new_surface(coder_dlp_compositor* comp,
                               coder_dlp_surface_cb cb, void* data) {
    if (!comp) {
        return;
    }
    comp->surface_cb = cb;
    comp->surface_cb_data = data;
}
