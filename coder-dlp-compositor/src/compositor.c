#include "coder_dlp.h"
#include "compositor_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wlr/util/log.h>

#define LOG_ERR(fmt, ...) fprintf(stderr, "coder-dlp: " fmt "\n", ##__VA_ARGS__)

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
    wlr_log_init(wlr_level, NULL);

    coder_dlp_compositor* comp = calloc(1, sizeof(*comp));
    if (!comp) {
        return NULL;
    }

    wl_list_init(&comp->toplevels);

    /* Wayland display */
    comp->wl_display = wl_display_create();
    if (!comp->wl_display) {
        LOG_ERR("failed to create wl_display");
        goto err_free;
    }
    comp->wl_event_loop = wl_display_get_event_loop(comp->wl_display);

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

    /* Output layout + scene graph */
    comp->output_layout = wlr_output_layout_create(comp->wl_display);
    comp->scene = wlr_scene_create();
    comp->scene_layout = wlr_scene_attach_output_layout(comp->scene, comp->output_layout);

    /* XDG shell (version 3) */
    comp->xdg_shell = wlr_xdg_shell_create(comp->wl_display, 3);
    comp->new_xdg_toplevel.notify = compositor_handle_new_xdg_toplevel;
    wl_signal_add(&comp->xdg_shell->events.new_toplevel, &comp->new_xdg_toplevel);
    comp->new_xdg_popup.notify = compositor_handle_new_xdg_popup;
    wl_signal_add(&comp->xdg_shell->events.new_popup, &comp->new_xdg_popup);

    /* Seat (needed for keyboard/pointer/clipboard) */
    comp->seat = wlr_seat_create(comp->wl_display, "seat0");

    /* DLP clipboard mediation */
    dlp_clipboard_init(comp);

    /* Security context protocol */
    dlp_security_context_init(comp);

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

    /* Remove all signal listeners BEFORE tearing down wlroots objects.
     * wlroots asserts that listener lists are empty during destroy. */
    wl_list_remove(&comp->new_output.link);
    wl_list_remove(&comp->new_input.link);
    wl_list_remove(&comp->new_xdg_toplevel.link);
    wl_list_remove(&comp->new_xdg_popup.link);
    wl_list_remove(&comp->request_set_selection.link);
    wl_list_remove(&comp->request_set_primary_selection.link);
    wl_list_remove(&comp->security_context_commit.link);

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
    if (comp->pointer_motion.notify) {
        wl_list_remove(&comp->pointer_motion.link);
        wl_list_remove(&comp->pointer_button.link);
        wl_list_remove(&comp->pointer_axis.link);
        wl_list_remove(&comp->pointer_frame.link);
    }

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
        wl_list_remove(&toplevel->link);
        free(toplevel);
    }

    /* Destroy scene graph */
    if (comp->scene) {
        wlr_scene_node_destroy(&comp->scene->tree.node);
        comp->scene = NULL;
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
