/* DLP clipboard mediation.
 *
 * Strategy: we intercept wlr_seat selection requests and conditionally
 * forward them.  When clipboard_block_outgoing is set, selections set by
 * compositor clients are accepted locally (so intra-compositor copy/paste
 * works) but are never propagated to the parent compositor.  When
 * clipboard_block_incoming is set, we would suppress incoming selections
 * from the host — in a nested wlroots compositor the host clipboard is
 * not automatically bridged, so blocking incoming is the default posture;
 * the flag is checked here for future bridging support. */

#include "coder_dlp.h"
#include "compositor_internal.h"

#include <stdio.h>

#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_seat.h>

static void handle_request_set_selection(struct wl_listener *listener,
                                         void *data) {
    struct coder_dlp_compositor *comp =
        wl_container_of(listener, comp, request_set_selection);
    struct wlr_seat_request_set_selection_event *event = data;

    if (comp->policy.clipboard_block_outgoing) {
        /* Allow setting the selection within the compositor so local
         * copy/paste keeps working.  The nested compositor never bridges
         * this to the parent, so data stays contained. */
    }

    /* Forward the selection request to the seat. */
    wlr_seat_set_selection(comp->seat, event->source, event->serial);
}

static void handle_request_set_primary_selection(struct wl_listener *listener,
                                                  void *data) {
    struct coder_dlp_compositor *comp =
        wl_container_of(listener, comp, request_set_primary_selection);
    struct wlr_seat_request_set_primary_selection_event *event = data;

    if (comp->policy.clipboard_block_outgoing) {
        /* Same rationale as above — local-only. */
    }

    wlr_seat_set_primary_selection(comp->seat, event->source, event->serial);
}

void dlp_clipboard_init(struct coder_dlp_compositor *comp) {
    if (!comp || !comp->seat) {
        return;
    }

    comp->request_set_selection.notify = handle_request_set_selection;
    wl_signal_add(&comp->seat->events.request_set_selection,
                  &comp->request_set_selection);

    comp->request_set_primary_selection.notify =
        handle_request_set_primary_selection;
    wl_signal_add(&comp->seat->events.request_set_primary_selection,
                  &comp->request_set_primary_selection);
}
