#include "coder_dlp.h"
#include "compositor_internal.h"

#include <stdlib.h>

/* --- Toplevel event handlers --- */

static void handle_toplevel_map(struct wl_listener *listener, void *data) {
    (void)data;
    struct coder_dlp_toplevel *toplevel =
        wl_container_of(listener, toplevel, map);

    /* Focus the newly mapped surface by raising it in the scene graph */
    wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
}

static void handle_toplevel_unmap(struct wl_listener *listener, void *data) {
    (void)data;
    (void)listener;
    /* Nothing to do on unmap for now — the scene tree handles visibility */
}

static void handle_toplevel_commit(struct wl_listener *listener, void *data) {
    (void)data;
    (void)listener;
    /* Surface commits are handled automatically by the scene graph */
}

static void handle_toplevel_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct coder_dlp_toplevel *toplevel =
        wl_container_of(listener, toplevel, destroy);

    wl_list_remove(&toplevel->map.link);
    wl_list_remove(&toplevel->unmap.link);
    wl_list_remove(&toplevel->commit.link);
    wl_list_remove(&toplevel->destroy.link);
    wl_list_remove(&toplevel->request_move.link);
    wl_list_remove(&toplevel->request_resize.link);
    wl_list_remove(&toplevel->link);
    free(toplevel);
}

static void handle_toplevel_request_move(struct wl_listener *listener,
                                          void *data) {
    (void)listener;
    (void)data;
    /* Interactive move is not supported in the nested compositor.
     * Clients are tiled/maximised within the output. */
}

static void handle_toplevel_request_resize(struct wl_listener *listener,
                                            void *data) {
    (void)listener;
    (void)data;
    /* Interactive resize is not supported in the nested compositor. */
}

/* --- XDG shell handlers (called from compositor.c signal setup) --- */

void compositor_handle_new_xdg_toplevel(struct wl_listener *listener,
                                         void *data) {
    struct coder_dlp_compositor *comp =
        wl_container_of(listener, comp, new_xdg_toplevel);
    struct wlr_xdg_toplevel *xdg_toplevel = data;

    struct coder_dlp_toplevel *toplevel = calloc(1, sizeof(*toplevel));
    if (!toplevel) {
        return;
    }

    toplevel->xdg_toplevel = xdg_toplevel;
    toplevel->compositor = comp;
    toplevel->scene_tree =
        wlr_scene_xdg_surface_create(&comp->scene->tree, xdg_toplevel->base);
    toplevel->scene_tree->node.data = toplevel;

    /* Wire up surface lifecycle listeners */
    toplevel->map.notify = handle_toplevel_map;
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);

    toplevel->unmap.notify = handle_toplevel_unmap;
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap,
                  &toplevel->unmap);

    toplevel->commit.notify = handle_toplevel_commit;
    wl_signal_add(&xdg_toplevel->base->surface->events.commit,
                  &toplevel->commit);

    toplevel->destroy.notify = handle_toplevel_destroy;
    wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);

    toplevel->request_move.notify = handle_toplevel_request_move;
    wl_signal_add(&xdg_toplevel->events.request_move,
                  &toplevel->request_move);

    toplevel->request_resize.notify = handle_toplevel_request_resize;
    wl_signal_add(&xdg_toplevel->events.request_resize,
                  &toplevel->request_resize);

    wl_list_insert(&comp->toplevels, &toplevel->link);

    /* Notify the surface callback if registered */
    if (comp->surface_cb) {
        comp->surface_cb(comp, xdg_toplevel->base->surface,
                         comp->surface_cb_data);
    }
}

void compositor_handle_new_xdg_popup(struct wl_listener *listener,
                                      void *data) {
    (void)listener;
    struct wlr_xdg_popup *popup = data;

    /* Find the parent scene tree to attach the popup */
    struct wlr_xdg_surface *parent =
        wlr_xdg_surface_try_from_wlr_surface(popup->parent);
    if (!parent) {
        return;
    }

    struct wlr_scene_tree *parent_tree = parent->data;
    if (!parent_tree) {
        return;
    }

    /* Create the popup in the scene graph — wlroots handles positioning */
    wlr_scene_xdg_surface_create(parent_tree, popup->base);
}
