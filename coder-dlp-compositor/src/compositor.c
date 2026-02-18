#include "coder_dlp.h"
#include "compositor_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdarg.h>
#include <wlr/util/log.h>

#include <wlr/backend/wayland.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection_v1.h>

#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>

#define LOG_ERR(fmt, ...) fprintf(stderr, "coder-dlp: " fmt "\n", ##__VA_ARGS__)

/* Global pointer used by the wlr_log callback — wlr_log is process-global so
 * we cannot pass per-compositor context through it directly. */
static coder_dlp_compositor* s_log_comp = NULL;

static void custom_wlr_log(enum wlr_log_importance importance, const char* fmt, va_list args) {
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);

    const char* level = "DEBUG";
    if (importance == WLR_ERROR) {
        level = "ERROR";
    } else if (importance == WLR_INFO) {
        level = "INFO";
    }

    if (s_log_comp && s_log_comp->log_cb) {
        char full[1100];
        snprintf(full, sizeof(full), "[wlr %s] %s", level, buf);
        s_log_comp->log_cb(full, s_log_comp->log_cb_data);
    } else {
        /* Fallback: no Qt callback registered yet (during coder_dlp_create) */
        fprintf(stderr, "[wlr %s] %s\n", level, buf);
    }
}

/* --- Wayland client tracking -------------------------------------------- */

struct client_destroy_wrapper {
    struct wl_listener destroy;
    struct coder_dlp_compositor* comp;
};

static void handle_client_destroyed(struct wl_listener* listener, void* data) {
    (void)data;
    struct client_destroy_wrapper* wrapper = wl_container_of(listener, wrapper, destroy);
    wrapper->comp->client_count--;
    wlr_log(WLR_DEBUG, "client disconnected (count=%d)", wrapper->comp->client_count);
    wl_list_remove(&wrapper->destroy.link);
    free(wrapper);
}

static void handle_client_created(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, client_created);
    struct wl_client* client = data;

    comp->client_count++;
    wlr_log(WLR_DEBUG, "client connected (count=%d)", comp->client_count);

    struct client_destroy_wrapper* wrapper = calloc(1, sizeof(*wrapper));
    if (!wrapper) {
        wlr_log(WLR_ERROR, "failed to allocate client destroy wrapper");
        return;
    }
    wrapper->comp = comp;
    wrapper->destroy.notify = handle_client_destroyed;
    wl_client_add_destroy_listener(client, &wrapper->destroy);
}

int coder_dlp_get_client_count(const coder_dlp_compositor* comp) {
    return comp ? comp->client_count : 0;
}
void coder_dlp_set_output_title(coder_dlp_compositor* comp, const char* title) {
    if (!comp || !comp->output || !title) {
        return;
    }
    wlr_wl_output_set_title(comp->output, title);
}

/* ------------------------------------------------------------------------ */

coder_dlp_compositor* coder_dlp_create(void* parent_wl_surface, coder_dlp_log_level log_level) {
    /* parent_wl_surface is reserved for future use with
     * wlr_wl_output_create_from_surface() for embedded rendering.
     * Currently the Wayland backend picks up WAYLAND_DISPLAY automatically. */
    (void)parent_wl_surface;

    enum wlr_log_importance wlr_level;
    switch (log_level) {
        case CODER_DLP_LOG_DEBUG:
            wlr_level = WLR_DEBUG;
            break;
        case CODER_DLP_LOG_INFO:
            wlr_level = WLR_INFO;
            break;
        default:
            wlr_level = WLR_ERROR;
            break;
    }
    wlr_log_init(wlr_level, custom_wlr_log);

    coder_dlp_compositor* comp = calloc(1, sizeof(*comp));
    if (!comp) {
        return NULL;
    }
    s_log_comp = comp;

    wl_list_init(&comp->toplevels);

    /* Wayland display */
    comp->wl_display = wl_display_create();
    if (!comp->wl_display) {
        LOG_ERR("failed to create wl_display");
        goto err_free;
    }
    comp->wl_event_loop = wl_display_get_event_loop(comp->wl_display);

    /* Track connected Wayland clients */
    comp->client_count = 0;
    comp->client_created.notify = handle_client_created;
    wl_display_add_client_created_listener(comp->wl_display, &comp->client_created);

    /* Backend — auto-detects Wayland backend when WAYLAND_DISPLAY is set */
    comp->backend = wlr_backend_autocreate(comp->wl_event_loop, NULL);
    if (!comp->backend) {
        LOG_ERR("failed to create wlr_backend");
        goto err_display;
    }

    /* Renderer + allocator */
    comp->renderer = wlr_renderer_autocreate(comp->backend);
    if (!comp->renderer) {
        LOG_ERR("failed to create wlr_renderer");
        goto err_display;
    }
    wlr_renderer_init_wl_display(comp->renderer, comp->wl_display);

    comp->allocator = wlr_allocator_autocreate(comp->backend, comp->renderer);
    if (!comp->allocator) {
        LOG_ERR("failed to create wlr_allocator");
        goto err_display;
    }

    /* Wayland globals */
    comp->wlr_compositor = wlr_compositor_create(comp->wl_display, 5, comp->renderer);
    comp->subcompositor = wlr_subcompositor_create(comp->wl_display);
    comp->data_device_mgr = wlr_data_device_manager_create(comp->wl_display);

    /* Additional protocol globals required by Electron/Chromium clients */
    wlr_linux_dmabuf_v1_create_with_renderer(comp->wl_display, 4, comp->renderer);
    wlr_viewporter_create(comp->wl_display);
    wlr_xdg_decoration_manager_v1_create(comp->wl_display);
    wlr_fractional_scale_manager_v1_create(comp->wl_display, 1);
    wlr_presentation_create(comp->wl_display, comp->backend, 1);
    wlr_cursor_shape_manager_v1_create(comp->wl_display, 1);
    wlr_single_pixel_buffer_manager_v1_create(comp->wl_display);

    /* Output layout + scene graph */
    comp->output_layout = wlr_output_layout_create(comp->wl_display);
    wlr_xdg_output_manager_v1_create(comp->wl_display, comp->output_layout);
    comp->scene = wlr_scene_create();
    comp->scene_layout = wlr_scene_attach_output_layout(comp->scene, comp->output_layout);

    /* Cursor — manages the hardware/software cursor image */
    comp->cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(comp->cursor, comp->output_layout);
    comp->cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

    /* XDG shell (version 3) */
    comp->xdg_shell = wlr_xdg_shell_create(comp->wl_display, 3);
    comp->new_xdg_toplevel.notify = compositor_handle_new_xdg_toplevel;
    wl_signal_add(&comp->xdg_shell->events.new_toplevel, &comp->new_xdg_toplevel);
    comp->new_xdg_popup.notify = compositor_handle_new_xdg_popup;
    wl_signal_add(&comp->xdg_shell->events.new_popup, &comp->new_xdg_popup);

    /* Seat (needed for keyboard/pointer/clipboard) */
    comp->seat = wlr_seat_create(comp->wl_display, "seat0");

    /* Primary selection protocol — required by GTK3+ for middle-click paste */
    wlr_primary_selection_v1_device_manager_create(comp->wl_display);

    /* DLP clipboard mediation */
    dlp_clipboard_init(comp);

    /* Security context protocol */
    dlp_security_context_init(comp);

    /* Cursor event listeners — cursor aggregates all pointer devices */
    comp->cursor_motion.notify = handle_cursor_motion;
    wl_signal_add(&comp->cursor->events.motion, &comp->cursor_motion);
    comp->cursor_motion_absolute.notify = handle_cursor_motion_absolute;
    wl_signal_add(&comp->cursor->events.motion_absolute, &comp->cursor_motion_absolute);
    comp->cursor_button.notify = handle_cursor_button;
    wl_signal_add(&comp->cursor->events.button, &comp->cursor_button);
    comp->cursor_axis.notify = handle_cursor_axis;
    wl_signal_add(&comp->cursor->events.axis, &comp->cursor_axis);
    comp->cursor_frame.notify = handle_cursor_frame;
    wl_signal_add(&comp->cursor->events.frame, &comp->cursor_frame);
    comp->request_set_cursor.notify = handle_request_set_cursor;
    wl_signal_add(&comp->seat->events.request_set_cursor, &comp->request_set_cursor);

    /* Listen for new outputs from the backend */
    comp->new_output.notify = compositor_handle_new_output;
    wl_signal_add(&comp->backend->events.new_output, &comp->new_output);

    /* Listen for new input devices from the backend */
    comp->new_input.notify = compositor_handle_new_input;
    wl_signal_add(&comp->backend->events.new_input, &comp->new_input);

    /* Start the backend — this triggers new_output for Wayland backend */
    if (!wlr_backend_start(comp->backend)) {
        LOG_ERR("failed to start wlr_backend");
        goto err_display;
    }

    /* Create a Wayland socket for client connections */
    comp->socket = wl_display_add_socket_auto(comp->wl_display);
    if (!comp->socket) {
        LOG_ERR("failed to add wayland socket");
        goto err_display;
    }

    return comp;

err_display:
    wl_display_destroy(comp->wl_display);
err_free:
    free(comp);
    return NULL;
}

void coder_dlp_destroy(coder_dlp_compositor* comp) {
    if (!comp) {
        return;
    }

    /* Clear global log pointer so the callback is not invoked after free. */
    if (s_log_comp == comp) {
        s_log_comp = NULL;
    }

    /* Remove all signal listeners BEFORE tearing down wlroots objects.
     * wlroots asserts that listener lists are empty during destroy. */
    wl_list_remove(&comp->new_output.link);
    wl_list_remove(&comp->new_input.link);
    wl_list_remove(&comp->new_xdg_toplevel.link);
    wl_list_remove(&comp->new_xdg_popup.link);
    wl_list_remove(&comp->request_set_selection.link);
    wl_list_remove(&comp->request_set_primary_selection.link);
    wl_list_remove(&comp->security_context_commit.link);
    wl_list_remove(&comp->client_created.link);

    /* Output listeners (wired in compositor_handle_new_output) */
    if (comp->output) {
        wl_list_remove(&comp->output_frame.link);
        wl_list_remove(&comp->output_request_state.link);
        wl_list_remove(&comp->output_destroy.link);
    }

    /* Input listeners (wired in setup_keyboard / setup_pointer) */
    if (comp->keyboard) {
        wl_list_remove(&comp->keyboard_key.link);
        wl_list_remove(&comp->keyboard_modifiers.link);
    }
    /* Touch listeners */
    if (comp->touch) {
        wl_list_remove(&comp->touch_down.link);
        wl_list_remove(&comp->touch_up.link);
        wl_list_remove(&comp->touch_motion.link);
        wl_list_remove(&comp->touch_cancel.link);
        wl_list_remove(&comp->touch_frame.link);
    }
    /* Cursor listeners */
    wl_list_remove(&comp->cursor_motion.link);
    wl_list_remove(&comp->cursor_motion_absolute.link);
    wl_list_remove(&comp->cursor_button.link);
    wl_list_remove(&comp->cursor_axis.link);
    wl_list_remove(&comp->cursor_frame.link);
    wl_list_remove(&comp->request_set_cursor.link);

    /* Clean up any remaining toplevels — remove their listeners so wlroots
     * assertions don't fire during display teardown. */
    struct coder_dlp_toplevel* toplevel;
    struct coder_dlp_toplevel* tmp;
    wl_list_for_each_safe(toplevel, tmp, &comp->toplevels, link) {
        wl_list_remove(&toplevel->map.link);
        wl_list_remove(&toplevel->unmap.link);
        wl_list_remove(&toplevel->commit.link);
        wl_list_remove(&toplevel->destroy.link);
        wl_list_remove(&toplevel->request_move.link);
        wl_list_remove(&toplevel->request_resize.link);
        wl_list_remove(&toplevel->request_maximize.link);
        wl_list_remove(&toplevel->request_fullscreen.link);
        wl_list_remove(&toplevel->link);
        free(toplevel);
    }

    /* Destroy scene graph */
    if (comp->scene) {
        wlr_scene_node_destroy(&comp->scene->tree.node);
        comp->scene = NULL;
    }

    /* Destroy cursor objects */
    if (comp->cursor_mgr) {
        wlr_xcursor_manager_destroy(comp->cursor_mgr);
    }
    if (comp->cursor) {
        wlr_cursor_destroy(comp->cursor);
    }

    /* Destroy backend (triggers output destroy etc.) */
    if (comp->backend) {
        wlr_backend_destroy(comp->backend);
    }

    /* Finally destroy display */
    if (comp->wl_display) {
        wl_display_destroy_clients(comp->wl_display);
        wl_display_destroy(comp->wl_display);
    }
    free(comp);
}

bool coder_dlp_is_available(void) {
    return getenv("WAYLAND_DISPLAY") != NULL;
}

void coder_dlp_on_new_surface(coder_dlp_compositor* comp, coder_dlp_surface_cb cb, void* data) {
    if (!comp) {
        return;
    }
    comp->surface_cb = cb;
    comp->surface_cb_data = data;
}

void coder_dlp_set_log_callback(coder_dlp_compositor* comp, coder_dlp_log_cb cb, void* user_data) {
    if (!comp) {
        return;
    }
    comp->log_cb = cb;
    comp->log_cb_data = user_data;
}
