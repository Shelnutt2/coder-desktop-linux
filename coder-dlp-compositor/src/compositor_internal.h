#ifndef CODER_DLP_COMPOSITOR_INTERNAL_H
#define CODER_DLP_COMPOSITOR_INTERNAL_H

#include "coder_dlp.h"

/* Internal definition of the opaque compositor struct */
struct coder_dlp_compositor {
    coder_dlp_policy policy;

    /* Surface callback */
    coder_dlp_surface_cb surface_cb;
    void* surface_cb_data;

    /* TODO (Phase 4): wlr_backend, wlr_renderer, wl_display, etc. */
};

#endif /* CODER_DLP_COMPOSITOR_INTERNAL_H */
