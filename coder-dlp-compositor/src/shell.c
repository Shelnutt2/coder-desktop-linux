#include "coder_dlp.h"
#include "compositor_internal.h"

#include <stdlib.h>

#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>

/* --- Toplevel event handlers --- */

static void handle_toplevel_map(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_toplevel* toplevel = wl_container_of(listener, toplevel, map);

    wlr_log(WLR_INFO, "toplevel mapped: app_id=%s title=%s",
            toplevel->xdg_toplevel->app_id ? toplevel->xdg_toplevel->app_id : "(null)",
            toplevel->xdg_toplevel->title ? toplevel->xdg_toplevel->title : "(null)");

    /* Focus the newly mapped surface by raising it in the scene graph */
    wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);

    /* Deactivate the previously focused toplevel, if any */
    struct wlr_surface* prev_surface = toplevel->compositor->seat->keyboard_state.focused_surface;
    if (prev_surface) {
        struct wlr_xdg_toplevel* prev_toplevel =
            wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
        if (prev_toplevel) {
            wlr_xdg_toplevel_set_activated(prev_toplevel, false);
        }
    }
    wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);

    /* Grant keyboard focus to the newly mapped surface */
    struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(toplevel->compositor->seat);
    if (keyboard) {
        wlr_seat_keyboard_notify_enter(toplevel->compositor->seat,
                                       toplevel->xdg_toplevel->base->surface, keyboard->keycodes,
                                       keyboard->num_keycodes, &keyboard->modifiers);
    }
}

static void handle_toplevel_unmap(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_toplevel* toplevel = wl_container_of(listener, toplevel, unmap);

    wlr_log(WLR_INFO, "toplevel unmapped: app_id=%s",
            toplevel->xdg_toplevel->app_id ? toplevel->xdg_toplevel->app_id : "(null)");
}

static void handle_toplevel_commit(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_toplevel* toplevel = wl_container_of(listener, toplevel, commit);

    if (toplevel->xdg_toplevel->base->initial_commit) {
        /* Send initial configure — size 0,0 lets the client choose its size.
         * The client will not map until it receives this configure event. */
        wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, 0, 0);
    }
}

static void handle_toplevel_destroy(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_toplevel* toplevel = wl_container_of(listener, toplevel, destroy);

    wlr_log(WLR_INFO, "toplevel destroyed: app_id=%s",
            toplevel->xdg_toplevel->app_id ? toplevel->xdg_toplevel->app_id : "(null)");

    wl_list_remove(&toplevel->map.link);
    wl_list_remove(&toplevel->unmap.link);
    wl_list_remove(&toplevel->commit.link);
    wl_list_remove(&toplevel->destroy.link);
    wl_list_remove(&toplevel->request_move.link);
    wl_list_remove(&toplevel->request_resize.link);
    wl_list_remove(&toplevel->link);
    free(toplevel);
}

static void handle_toplevel_request_move(struct wl_listener* listener, void* data) {
    (void)listener;
    (void)data;
    /* Interactive move is not supported in the nested compositor.
     * Clients are tiled/maximised within the output. */
}

static void handle_toplevel_request_resize(struct wl_listener* listener, void* data) {
    (void)listener;
    (void)data;
    /* Interactive resize is not supported in the nested compositor. */
}

/* --- XDG shell handlers (called from compositor.c signal setup) --- */

void compositor_handle_new_xdg_toplevel(struct wl_listener* listener, void* data) {
    struct coder_dlp_compositor* comp = wl_container_of(listener, comp, new_xdg_toplevel);
    struct wlr_xdg_toplevel* xdg_toplevel = data;

    wlr_log(WLR_INFO, "new xdg toplevel: app_id=%s title=%s",
            xdg_toplevel->app_id ? xdg_toplevel->app_id : "(null)",
            xdg_toplevel->title ? xdg_toplevel->title : "(null)");

    struct coder_dlp_toplevel* toplevel = calloc(1, sizeof(*toplevel));
    if (!toplevel) {
        return;
    }

    toplevel->xdg_toplevel = xdg_toplevel;
    toplevel->compositor = comp;
    toplevel->scene_tree = wlr_scene_xdg_surface_create(&comp->scene->tree, xdg_toplevel->base);
    toplevel->scene_tree->node.data = toplevel;

    /* Link the xdg_surface to its scene tree so that popups parented to this
     * surface can look up the tree via parent->data (see new_xdg_popup). */
    xdg_toplevel->base->data = toplevel->scene_tree;

    /* Wire up surface lifecycle listeners */
    toplevel->map.notify = handle_toplevel_map;
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);

    toplevel->unmap.notify = handle_toplevel_unmap;
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);

    toplevel->commit.notify = handle_toplevel_commit;
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);

    toplevel->destroy.notify = handle_toplevel_destroy;
    wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);

    toplevel->request_move.notify = handle_toplevel_request_move;
    wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);

    toplevel->request_resize.notify = handle_toplevel_request_resize;
    wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);

    wl_list_insert(&comp->toplevels, &toplevel->link);

    /* Notify the surface callback if registered */
    if (comp->surface_cb) {
        comp->surface_cb(comp, xdg_toplevel->base->surface, comp->surface_cb_data);
    }
}

struct coder_dlp_popup {
    struct wlr_xdg_popup* xdg_popup;
    struct wl_listener commit;
    struct wl_listener destroy;
};

static void handle_popup_commit(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_popup* popup = wl_container_of(listener, popup, commit);
    if (popup->xdg_popup->base->initial_commit) {
        wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
    }
}

static void handle_popup_destroy(struct wl_listener* listener, void* data) {
    (void)data;
    struct coder_dlp_popup* popup = wl_container_of(listener, popup, destroy);
    wl_list_remove(&popup->commit.link);
    wl_list_remove(&popup->destroy.link);
    free(popup);
}

void compositor_handle_new_xdg_popup(struct wl_listener* listener, void* data) {
    (void)listener;
    struct wlr_xdg_popup* xdg_popup = data;

    /* Find the parent scene tree to attach the popup */
    struct wlr_xdg_surface* parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
    if (!parent) {
        return;
    }

    struct wlr_scene_tree* parent_tree = parent->data;
    if (!parent_tree) {
        return;
    }

    /* Create the popup in the scene graph — wlroots handles positioning */
    struct wlr_scene_tree* popup_tree = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);

    struct coder_dlp_popup* popup = calloc(1, sizeof(*popup));
    if (!popup) {
        return;
    }
    popup->xdg_popup = xdg_popup;
    xdg_popup->base->data = popup_tree;

    popup->commit.notify = handle_popup_commit;
    wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

    popup->destroy.notify = handle_popup_destroy;
    wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);
}
