#include "coder_dlp.h"
#include "compositor_internal.h"

#include <time.h>

#include "watermark.h"

#include <wlr/backend/wayland.h>
#include <wlr/config.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_cursor.h>

#if WLR_HAS_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>

/* --- Output event handlers --- */

static void handle_output_frame(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, output_frame);

    struct wlr_scene_output* scene_output = wlr_scene_get_scene_output(comp->scene, comp->output);
    if (!scene_output) {
        return;
    }

    if (comp->policy.watermark_enabled && comp->watermark.has_identity) {
        /* Watermark path: build output state, apply watermark, then commit */
        struct wlr_output_state state;
        wlr_output_state_init(&state);
        if (wlr_scene_output_build_state(scene_output, &state, NULL)) {
            if (state.buffer) {
                dlp_watermark_apply(state.buffer, &comp->watermark);
            }
            wlr_output_commit_state(comp->output, &state);
        } else {
            wlr_log(WLR_DEBUG, "scene output build state failed");
        }
        wlr_output_state_finish(&state);
    } else {
        /* Fast path: no watermark overhead */
        if (!wlr_scene_output_commit(scene_output, NULL)) {
            wlr_log(WLR_DEBUG, "scene output commit failed");
        }
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);
}

static void handle_output_request_state(struct wl_listener* listener, void* data) {
    (void)listener;
    struct wlr_output_event_request_state* event = data;
    wlr_output_commit_state(event->output, event->state);
}

static void handle_output_destroy(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, output_destroy);

    wl_list_remove(&comp->output_frame.link);
    wl_list_remove(&comp->output_request_state.link);
    wl_list_remove(&comp->output_destroy.link);
    comp->output = NULL;
}

/* Called when the backend creates a new output (e.g. the Wayland backend
 * creates one output automatically on start). */
void compositor_handle_new_output(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, new_output);
    struct wlr_output* output = data;

    /* Only handle one output for the nested compositor */
    comp->output = output;

    /* Apply stored window title before the first commit.
     * For Wayland backend, the title must be set before commit;
     * for X11 backend, it can be set at any time but setting it
     * early avoids a flash of an untitled window. */
    if (comp->output_title) {
        if (wlr_backend_is_wl(comp->backend)) {
            wlr_wl_output_set_title(output, comp->output_title);
        }
#if WLR_HAS_X11_BACKEND
        else if (wlr_backend_is_x11(comp->backend)) {
            wlr_x11_output_set_title(output, comp->output_title);
        }
#endif
    }

    /* wlroots 0.19 requires render initialization before the output can
     * produce frames.  Without this call the output stays blank. */
    wlr_output_init_render(output, comp->allocator, comp->renderer);

    /* Configure the output with its preferred mode */
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    struct wlr_output_mode* mode = wlr_output_preferred_mode(output);
    if (mode) {
        wlr_output_state_set_mode(&state, mode);
    }
    wlr_output_commit_state(output, &state);
    wlr_output_state_finish(&state);

    /* Add to the output layout and connect the scene graph */
    struct wlr_output_layout_output* l_output =
        wlr_output_layout_add_auto(comp->output_layout, output);
    struct wlr_scene_output* scene_output = wlr_scene_output_create(comp->scene, output);
    wlr_scene_output_layout_add_output(comp->scene_layout, l_output, scene_output);

    /* Set initial cursor image so it's visible when hovering over the window */
    wlr_cursor_set_xcursor(comp->cursor, comp->cursor_mgr, "default");

    /* Listen for output events */
    comp->output_frame.notify = handle_output_frame;
    wl_signal_add(&output->events.frame, &comp->output_frame);

    comp->output_request_state.notify = handle_output_request_state;
    wl_signal_add(&output->events.request_state, &comp->output_request_state);

    comp->output_destroy.notify = handle_output_destroy;
    wl_signal_add(&output->events.destroy, &comp->output_destroy);
}

/* --- Public API --- */

int coder_dlp_get_fd(coder_dlp_compositor* comp) {
    if (!comp || !comp->wl_event_loop) {
        return -1;
    }
    return wl_event_loop_get_fd(comp->wl_event_loop);
}

void coder_dlp_dispatch(coder_dlp_compositor* comp) {
    if (!comp || !comp->wl_display) {
        return;
    }
    wl_display_flush_clients(comp->wl_display);
    wl_event_loop_dispatch(comp->wl_event_loop, 0); /* non-blocking */
}
