#include "coder_dlp.h"
#include "compositor_internal.h"

#include <time.h>

#include <wlr/render/allocator.h>
#include <wlr/types/wlr_cursor.h>
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

    if (!wlr_scene_output_commit(scene_output, NULL)) {
        wlr_log(WLR_DEBUG, "scene output commit failed");
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

    /* Advertise the output to Wayland clients.  Without this call clients
     * (e.g. Electron/Chromium) see no wl_output global and will never
     * create an xdg_toplevel window. */
    wlr_output_create_global(output, comp->wl_display);

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
